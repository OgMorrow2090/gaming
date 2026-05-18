#!/usr/bin/env python3
"""gw2-claude-daemon.py — Claude-vision OCR worker for Mystic Clicker / GW2.

Companion to gw2-ocr-daemon.sh. Where that daemon runs tesseract for fast,
free, offline transcription, this one sends the captured frame to the Claude
API — which can both READ and UNDERSTAND the screen (item lists, tooltips,
dialog state, coin amounts) far better than tesseract manages on GW2's
stylised fonts.

Same sandbox-bridge design as the tesseract daemon: the Nexus addon drops a
captured frame into /tmp (bind-mounted into the Steam pressure-vessel
sandbox); this daemon runs OUTSIDE the sandbox as a `systemd --user` unit,
polls /tmp, calls the API, writes the result back.

Protocol (parallel to the tesseract daemon — separate `gw2-claude-*`
namespace so the two daemons never race for the same files):

    addon writes  /tmp/gw2-claude-input-<TS>.png      captured frame (png/jpg/bmp/webp)
    addon writes  /tmp/gw2-claude-input-<TS>.prompt   (optional) what to ask Claude
    addon touches /tmp/gw2-claude-input-<TS>.ready    atomic "frame complete" trigger
    daemon writes /tmp/gw2-claude-output-<TS>.txt     Claude's answer
    daemon touches /tmp/gw2-claude-done-<TS>          completion marker

We process the `.ready` marker (never the image directly) so we cannot race
the addon's still-open file handle and analyse a half-written frame.

Standalone test mode (no addon needed):

    gw2-claude-daemon.py --analyze <image> ["question"]

Config: ~/.config/gw2-claude/config.env  (KEY=VALUE lines, mode 600)
    ANTHROPIC_API_KEY=sk-ant-...      (required)
    GW2_CLAUDE_MODEL=claude-opus-4-7  (optional — see config.env comments)
"""

import base64
import io
import os
import sys
import time

CONFIG_PATH = os.path.expanduser("~/.config/gw2-claude/config.env")
LOG_PATH = os.path.expanduser("~/.local/state/gw2-claude-daemon.log")
TMP = "/tmp"
POLL_SECONDS = 0.2  # API round-trip dwarfs this; no need to spin faster

# Claude vision accepts JPEG/PNG/GIF/WebP — not BMP. Whatever the addon drops,
# we re-encode to PNG before sending. Opus 4.7 high-res caps at 2576px on the
# long edge; anything larger is downscaled to stay inside that and control cost.
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
    "- Keep answers compact — this output is consumed by an addon, not a human "
    "reading prose."
)


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
    """Read ~/.config/gw2-claude/config.env into the environment."""
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
            new_size = (round(im.width * scale), round(im.height * scale))
            im = im.resize(new_size, Image.LANCZOS)
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
        # System prompt is identical on every request — cache it so repeated
        # calls only pay full price for the (always-unique) image + question.
        # Caching only engages once the cached prefix is large enough for the
        # model; if it doesn't, this is simply a no-op, never a regression.
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


def find_image(suffix):
    """Locate the captured frame for a given request suffix."""
    for ext in IMAGE_EXTS:
        candidate = os.path.join(TMP, "gw2-claude-input-%s%s" % (suffix, ext))
        if os.path.exists(candidate):
            return candidate
    return None


def run_daemon(client, model):
    import glob

    log("daemon starting (pid=%d model=%s)" % (os.getpid(), model))
    while True:
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

            try:
                result = analyze(client, model, image_path, question)
            except Exception as e:  # noqa: BLE001 — surface every failure to the addon
                result = "ERROR: %s" % e
                log("ERROR request=%s %s" % (suffix, e))

            # Write the answer, then touch the done marker LAST so the addon
            # never reads a half-written output file.
            with open(out_path, "w") as f:
                f.write(result)
            open(done_mark, "w").close()
            _safe_rm(ready)

        time.sleep(POLL_SECONDS)


def _safe_rm(path):
    try:
        os.remove(path)
    except OSError:
        pass


def main():
    cfg = load_config()
    model = cfg.get("GW2_CLAUDE_MODEL") or "claude-opus-4-7"
    os.environ["ANTHROPIC_API_KEY"] = cfg["ANTHROPIC_API_KEY"]

    import anthropic
    client = anthropic.Anthropic()

    args = sys.argv[1:]
    if args and args[0] == "--analyze":
        if len(args) < 2:
            print("usage: gw2-claude-daemon.py --analyze <image> [question]",
                  file=sys.stderr)
            sys.exit(2)
        image = args[1]
        question = " ".join(args[2:]) if len(args) > 2 else None
        print(analyze(client, model, image, question))
        return

    if args and args[0] != "--daemon":
        print("usage: gw2-claude-daemon.py [--daemon | --analyze <image> [q]]",
              file=sys.stderr)
        sys.exit(2)

    run_daemon(client, model)


if __name__ == "__main__":
    main()
