#!/usr/bin/env python3
"""gw2-claude-daemon.py — Claude-vision OCR + text-to-speech worker for GW2.

Companion to gw2-ocr-daemon.sh. Where that daemon runs tesseract for fast,
free, offline transcription, this one sends the captured frame to the Claude
API — which can both READ and UNDERSTAND the screen — and then speaks the
answer aloud via Piper TTS. Designed for a keyboard-free, controller +
game-streaming setup: the spoken answer rides the Sunshine audio stream back
to the player.

Sandbox-bridge design (same as the tesseract daemon): the Nexus addon drops a
captured frame into /tmp (bind-mounted into the Steam pressure-vessel
sandbox); this daemon runs OUTSIDE the sandbox as a `systemd --user` unit,
polls /tmp, calls the API, writes the answer back, and speaks it.

Protocol (separate gw2-claude-* namespace so it never collides with the
tesseract OCR daemon):

    addon writes  /tmp/gw2-claude-input-<TS>.png      captured frame
    addon writes  /tmp/gw2-claude-input-<TS>.prompt   (optional) what to ask
    addon touches /tmp/gw2-claude-input-<TS>.ready    "request complete" trigger
    daemon writes /tmp/gw2-claude-output-<TS>.txt     Claude's answer
    daemon touches /tmp/gw2-claude-done-<TS>          completion marker
    daemon touches /tmp/gw2-claude-speaking           present while TTS is talking
    addon touches /tmp/gw2-claude-stop                request: stop talking now

Standalone test mode (no addon needed):

    gw2-claude-daemon.py --analyze <image> ["question"]   read + speak an image
    gw2-claude-daemon.py --say "some text"                speak text directly

Config: ~/.config/gw2-claude/config.env  (KEY=VALUE lines, mode 600)
    ANTHROPIC_API_KEY=sk-ant-...           (required)
    GW2_CLAUDE_MODEL=claude-haiku-4-5      (optional)
    GW2_CLAUDE_TTS=on                      (optional — on/off, default on)
    GW2_CLAUDE_VOICE=en_GB-cori-high     (optional — Piper voice name)
"""

import base64
import glob
import io
import os
import re
import subprocess
import sys
import time

CONFIG_PATH = os.path.expanduser("~/.config/gw2-claude/config.env")
LOG_PATH = os.path.expanduser("~/.local/state/gw2-claude-daemon.log")
VOICES_DIR = os.path.expanduser("~/.local/share/gw2-claude/voices")
TMP = "/tmp"
POLL_SECONDS = 0.2

STOP_MARKER = os.path.join(TMP, "gw2-claude-stop")
SPEAKING_MARKER = os.path.join(TMP, "gw2-claude-speaking")
TTS_WAV = os.path.join(TMP, "gw2-claude-tts.wav")

MAX_LONG_EDGE = 2576
IMAGE_EXTS = (".png", ".jpg", ".jpeg", ".bmp", ".webp", ".gif")

DEFAULT_PROMPT = (
    "Transcribe all visible text in this Guild Wars 2 screenshot. Preserve "
    "structure — list items one per line, keep tooltip text together, and "
    "report coin amounts (gold/silver/copper) and item quantities exactly."
)

SYSTEM_PROMPT = (
    "You are a vision assistant for the MMO Guild Wars 2. You receive a "
    "screenshot or a cropped region of the GW2 user interface and must read "
    "and report what is on screen accurately and concisely.\n\n"
    "Guidelines:\n"
    "- Report only text and UI state that is actually visible. Never invent, "
    "guess, or autocomplete text that is cut off or unreadable — mark it as "
    "unclear instead.\n"
    "- GW2 uses stylised fonts; read item names, vendor/trading-post listings, "
    "skill names, dialog buttons, and chat carefully.\n"
    "- Coin amounts matter: report gold/silver/copper values precisely, in "
    "order, exactly as shown.\n"
    "- Preserve structure. Lists become one item per line. Tooltips stay as a "
    "coherent block. Tables keep their columns.\n"
    "- When the user asks a specific question, answer it directly and briefly. "
    "When given no specific question, transcribe everything visible.\n"
    "- If the image is blank, black, or otherwise unreadable, say so plainly "
    "rather than describing nothing.\n"
    "- Tone: keep it light, warm, and a bit playful — like a mate reading the "
    "screen out to you over voice chat, not a stiff formal report. Stay "
    "accurate, but relaxed and natural; a little personality and humour is "
    "welcome. Don't overdo it — a quick fun aside, not a comedy routine.\n"
    "- Your answer is both shown on screen and read aloud by text-to-speech, "
    "so keep it plain and compact — no markdown, no bullet symbols."
)

# Handle to the running `pw-play` subprocess, or None.
g_tts_proc = None


def log(msg):
    line = "%s %s" % (time.strftime("%F %T"), msg)
    try:
        os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
        with open(LOG_PATH, "a") as f:
            f.write(line + "\n")
    except OSError:
        pass
    print(line, file=sys.stderr, flush=True)


def load_config():
    cfg = {}
    try:
        with open(CONFIG_PATH) as f:
            for raw in f:
                line = raw.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, _, val = line.partition("=")
                cfg[key.strip()] = val.strip()
    except FileNotFoundError:
        log("FATAL: %s missing — run scripts/gw2-claude-setup.sh" % CONFIG_PATH)
        sys.exit(1)
    if not cfg.get("ANTHROPIC_API_KEY"):
        log("FATAL: ANTHROPIC_API_KEY not set in %s" % CONFIG_PATH)
        sys.exit(1)
    return cfg


def prepare_image(path):
    """Open any supported image, downscale to <=MAX_LONG_EDGE, return PNG bytes."""
    from PIL import Image

    with Image.open(path) as im:
        im = im.convert("RGB")
        long_edge = max(im.size)
        if long_edge > MAX_LONG_EDGE:
            scale = MAX_LONG_EDGE / long_edge
            im = im.resize((round(im.width * scale), round(im.height * scale)),
                           Image.LANCZOS)
        buf = io.BytesIO()
        im.save(buf, format="PNG")
        return buf.getvalue()


def analyze(client, model, image_path, question):
    """Send one frame to Claude with vision and return the text answer."""
    png = prepare_image(image_path)
    b64 = base64.standard_b64encode(png).decode("ascii")

    resp = client.messages.create(
        model=model,
        max_tokens=4096,
        # Stable system prompt — cached so repeated calls only pay full price
        # for the always-unique image + question.
        system=[{
            "type": "text",
            "text": SYSTEM_PROMPT,
            "cache_control": {"type": "ephemeral"},
        }],
        messages=[{
            "role": "user",
            "content": [
                {
                    "type": "image",
                    "source": {
                        "type": "base64",
                        "media_type": "image/png",
                        "data": b64,
                    },
                },
                {"type": "text", "text": question or DEFAULT_PROMPT},
            ],
        }],
    )
    text = "".join(b.text for b in resp.content if b.type == "text").strip()
    u = resp.usage
    log("ok model=%s in=%s cache_read=%s out=%s" % (
        model, u.input_tokens, u.cache_read_input_tokens, u.output_tokens))
    return text or "(no text returned)"


# ---------------------------------------------------------------------------
# Text-to-speech (Piper). Synthesis is brief and runs inline; playback is a
# detached `pw-play` subprocess we can kill on a stop request. The presence of
# SPEAKING_MARKER tells the addon whether speech is currently in progress.
# ---------------------------------------------------------------------------

def clean_for_speech(text):
    """Strip markdown so the voice doesn't read 'asterisk asterisk'."""
    t = re.sub(r"`+", "", text)
    t = re.sub(r"\*+", "", t)
    t = re.sub(r"^#+\s*", "", t, flags=re.MULTILINE)
    t = re.sub(r"^\s*[-*]\s+", "", t, flags=re.MULTILINE)
    return t.strip()


def stop_speaking():
    """Kill any in-progress playback and clear the speaking marker."""
    global g_tts_proc
    if g_tts_proc is not None:
        if g_tts_proc.poll() is None:
            g_tts_proc.terminate()
            try:
                g_tts_proc.wait(timeout=2)
            except Exception:  # noqa: BLE001
                g_tts_proc.kill()
        g_tts_proc = None
    _safe_rm(SPEAKING_MARKER)


def reap_tts():
    """Drop the speaking marker once playback has finished on its own."""
    global g_tts_proc
    if g_tts_proc is not None and g_tts_proc.poll() is not None:
        g_tts_proc = None
        _safe_rm(SPEAKING_MARKER)


def speak(text, cfg):
    """Synthesize `text` with Piper and play it. Supersedes any current speech."""
    global g_tts_proc
    stop_speaking()

    if (cfg.get("GW2_CLAUDE_TTS", "on") or "on").lower() in ("off", "0", "false", "no"):
        return
    spoken = clean_for_speech(text)
    if not spoken:
        return

    voice = cfg.get("GW2_CLAUDE_VOICE") or "en_GB-cori-high"
    model = os.path.join(VOICES_DIR, voice + ".onnx")
    if not os.path.exists(model):
        log("WARN: voice model missing (%s) — skipping TTS" % model)
        return

    piper = os.path.join(os.path.dirname(sys.executable), "piper")
    try:
        subprocess.run([piper, "-m", model, "-f", TTS_WAV],
                       input=spoken.encode("utf-8"),
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       timeout=120, check=True)
    except Exception as e:  # noqa: BLE001
        log("WARN: Piper synthesis failed: %s" % e)
        return

    try:
        g_tts_proc = subprocess.Popen(["pw-play", TTS_WAV],
                                      stdout=subprocess.DEVNULL,
                                      stderr=subprocess.DEVNULL)
    except Exception as e:  # noqa: BLE001
        log("WARN: pw-play failed: %s" % e)
        return
    open(SPEAKING_MARKER, "w").close()
    log("speaking (%d chars, voice %s)" % (len(spoken), voice))


def _safe_rm(path):
    try:
        os.remove(path)
    except OSError:
        pass


def find_image(suffix):
    for ext in IMAGE_EXTS:
        candidate = os.path.join(TMP, "gw2-claude-input-%s%s" % (suffix, ext))
        if os.path.exists(candidate):
            return candidate
    return None


def run_daemon(client, model, cfg):
    log("daemon starting (pid=%d model=%s tts=%s)" % (
        os.getpid(), model, cfg.get("GW2_CLAUDE_TTS", "on")))
    while True:
        # A stop request kills speech immediately (the addon touches this on a
        # second double-press of the read button).
        if os.path.exists(STOP_MARKER):
            stop_speaking()
            _safe_rm(STOP_MARKER)
        reap_tts()

        for ready in glob.glob(os.path.join(TMP, "gw2-claude-input-*.ready")):
            suffix = os.path.basename(ready)[len("gw2-claude-input-"):-len(".ready")]
            done_mark = os.path.join(TMP, "gw2-claude-done-%s" % suffix)
            out_path = os.path.join(TMP, "gw2-claude-output-%s.txt" % suffix)
            prompt_path = os.path.join(TMP, "gw2-claude-input-%s.prompt" % suffix)

            if os.path.exists(done_mark):
                _safe_rm(ready)
                continue

            image_path = find_image(suffix)
            if image_path is None:
                log("WARN: no image for request %s — skipping" % suffix)
                _safe_rm(ready)
                continue

            question = None
            try:
                with open(prompt_path) as f:
                    question = f.read().strip()
            except FileNotFoundError:
                pass

            # A fresh read supersedes whatever is being spoken.
            stop_speaking()

            ok = True
            try:
                result = analyze(client, model, image_path, question)
            except Exception as e:  # noqa: BLE001
                result = "ERROR: %s" % e
                ok = False
                log("ERROR request=%s %s" % (suffix, e))

            with open(out_path, "w") as f:
                f.write(result)
            open(done_mark, "w").close()
            _safe_rm(ready)

            if ok:
                # If the player cancelled mid-read (the stop marker appeared
                # during the API call), honour it instead of speaking.
                if os.path.exists(STOP_MARKER):
                    _safe_rm(STOP_MARKER)
                    log("read %s cancelled before speech" % suffix)
                else:
                    speak(result, cfg)

        time.sleep(POLL_SECONDS)


def main():
    cfg = load_config()
    model = cfg.get("GW2_CLAUDE_MODEL") or "claude-opus-4-7"
    os.environ["ANTHROPIC_API_KEY"] = cfg["ANTHROPIC_API_KEY"]

    args = sys.argv[1:]

    if args and args[0] == "--say":
        speak(" ".join(args[1:]), cfg)
        if g_tts_proc is not None:
            g_tts_proc.wait()
        return

    import anthropic
    client = anthropic.Anthropic()

    if args and args[0] == "--analyze":
        if len(args) < 2:
            print("usage: gw2-claude-daemon.py --analyze <image> [question]",
                  file=sys.stderr)
            sys.exit(2)
        question = " ".join(args[2:]) if len(args) > 2 else None
        text = analyze(client, model, args[1], question)
        print(text)
        speak(text, cfg)
        if g_tts_proc is not None:
            g_tts_proc.wait()
        return

    if args and args[0] != "--daemon":
        print("usage: gw2-claude-daemon.py [--daemon | --analyze <img> [q] | --say <text>]",
              file=sys.stderr)
        sys.exit(2)

    run_daemon(client, model, cfg)


if __name__ == "__main__":
    main()
