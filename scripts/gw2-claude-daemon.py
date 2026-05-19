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
    GW2_CLAUDE_MODEL=claude-haiku-4-5       (optional)
    GW2_CLAUDE_RESEARCH_MODEL=claude-sonnet-4-6  (optional — Research action)
    GW2_CLAUDE_TTS=on                       (optional — on/off, default on)
    GW2_CLAUDE_TTS_ENGINE=auto              (optional — auto/piper/elevenlabs)
    GW2_CLAUDE_TTS_GAIN_DB=12               (optional — loudness boost, dB)
    GW2_CLAUDE_VOICE=en_GB-...-medium       (optional — Piper voice name)
    ELEVENLABS_API_KEY=...                  (optional — enables ElevenLabs TTS)
    ELEVENLABS_VOICE_ID=...                 (optional — ElevenLabs voice)
    ELEVENLABS_MODEL=eleven_turbo_v2_5      (optional)
"""

import base64
import glob
import io
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

CONFIG_PATH = os.path.expanduser("~/.config/gw2-claude/config.env")
LOG_PATH = os.path.expanduser("~/.local/state/gw2-claude-daemon.log")
VOICES_DIR = os.path.expanduser("~/.local/share/gw2-claude/voices")
TMP = "/tmp"
POLL_SECONDS = 0.2

STOP_MARKER = os.path.join(TMP, "gw2-claude-stop")
SPEAKING_MARKER = os.path.join(TMP, "gw2-claude-speaking")
TTS_WAV = os.path.join(TMP, "gw2-claude-tts.wav")
TTS_MP3 = os.path.join(TMP, "gw2-claude-tts.mp3")
TTS_RAW = os.path.join(TMP, "gw2-claude-tts-raw.wav")  # pre-boost synthesis target

# ElevenLabs cloud TTS — optional; enabled when ELEVENLABS_API_KEY is set.
EL_API_BASE = "https://api.elevenlabs.io/v1/text-to-speech/"
EL_DEFAULT_MODEL = "eleven_turbo_v2_5"
EL_DEFAULT_VOICE = "pNInz6obpgDQGcFmaJgB"  # placeholder preset; pick a playful voice once the key is in

MAX_LONG_EDGE = 2576
IMAGE_EXTS = (".png", ".jpg", ".jpeg", ".bmp", ".webp", ".gif")

# Phase 2 action endpoints / paths.
GW2_API = "https://api.guildwars2.com/v2"
BLTC_SEARCH = "https://www.gw2bltc.com/en/tp/search"  # item name -> id lookup
WIKI_API = "https://wiki.guildwars2.com/api.php"
WIKI_LIBRARY = os.path.expanduser(
    "~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/NexusGameWiki/library.json")

DEFAULT_PROMPT = (
    "Transcribe all visible text in this Guild Wars 2 screenshot. Preserve "
    "structure — list items one per line, keep tooltip text together, and "
    "report coin amounts (gold/silver/copper) and item quantities exactly."
)

SYSTEM_PROMPT = (
    "You are a screen-reading assistant for the game Guild Wars 2. You receive "
    "a screenshot or a cropped region of the GW2 interface. Your job is to "
    "read out the text that is on screen — nothing more.\n\n"
    "Rules:\n"
    "- Output ONLY the text content itself. Do NOT describe the image, do NOT "
    "say what kind of UI element it is, and do NOT add openers such as 'I "
    "see', 'This is', 'The screen shows', or 'Here is'. Just give the words.\n"
    "- If part of the text is cut off, tiny, or unreadable, simply skip it. "
    "Never mention or flag that anything is cut off, unclear, or partially "
    "visible — read what is legible and stop there.\n"
    "- GW2 uses stylised fonts; read item names, vendor and trading-post "
    "listings, skill names, dialog buttons and chat carefully.\n"
    "- Report gold/silver/copper coin amounts and item quantities exactly as "
    "shown.\n"
    "- Preserve structure: lists one item per line, a tooltip as one coherent "
    "block, tables keeping their columns.\n"
    "- When the user asks a specific question, answer it directly. Otherwise "
    "just read out everything that is visible.\n"
    "- If the image is blank or black, say so in a few words.\n"
    "- Read naturally and plainly, the way a person reads text aloud — no "
    "markdown, no bullet symbols, no commentary, no jokes or asides.\n"
    "- Your answer is shown on screen and may be read aloud, so keep it clean "
    "and compact."
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
    # Drop the Trading Post coin-data marker line — it drives the on-screen
    # panel, not speech (the prose to read aloud follows it).
    t = re.sub(r"^@TP\|[^\n]*\n", "", text)
    t = re.sub(r"`+", "", t)
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


def _play(wav_path, label):
    """Start killable playback of `wav_path` and set the speaking marker."""
    global g_tts_proc
    try:
        g_tts_proc = subprocess.Popen(["pw-play", wav_path],
                                      stdout=subprocess.DEVNULL,
                                      stderr=subprocess.DEVNULL)
    except Exception as e:  # noqa: BLE001
        log("WARN: pw-play failed: %s" % e)
        return False
    open(SPEAKING_MARKER, "w").close()
    log("speaking (%s)" % label)
    return True


def _af_filter(cfg):
    """ffmpeg -af value for the TTS loudness boost, or None when not boosting.

    A plain volume boost would clip ElevenLabs/Piper output on loud syllables,
    so the boost is followed by a limiter — loud, but never distorted. This is
    what makes the voice ride above the game audio."""
    try:
        gain = float(cfg.get("GW2_CLAUDE_TTS_GAIN_DB") or 0)
    except ValueError:
        gain = 0.0
    if gain <= 0:
        return None
    return "volume=%.1fdB,alimiter=limit=0.95" % gain


def _speak_piper(spoken, cfg):
    """Local Piper synthesis -> WAV -> playback. Returns True on success."""
    voice = cfg.get("GW2_CLAUDE_VOICE") or "en_GB-jenny_dioco-medium"
    model = os.path.join(VOICES_DIR, voice + ".onnx")
    if not os.path.exists(model):
        log("WARN: voice model missing (%s) — skipping TTS" % model)
        return False

    af = _af_filter(cfg)
    piper = os.path.join(os.path.dirname(sys.executable), "piper")
    synth_to = TTS_RAW if af else TTS_WAV
    try:
        subprocess.run([piper, "-m", model, "-f", synth_to],
                       input=spoken.encode("utf-8"),
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       timeout=120, check=True)
        if af:
            subprocess.run(["ffmpeg", "-y", "-i", TTS_RAW, "-af", af, TTS_WAV],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           timeout=30, check=True)
    except Exception as e:  # noqa: BLE001
        log("WARN: Piper synthesis failed: %s" % e)
        return False
    return _play(TTS_WAV, "%d chars, Piper %s" % (len(spoken), voice))


def _synth_elevenlabs(spoken, cfg):
    """Call the ElevenLabs TTS API; return MP3 bytes, or None on failure."""
    key = cfg.get("ELEVENLABS_API_KEY")
    voice = cfg.get("ELEVENLABS_VOICE_ID") or EL_DEFAULT_VOICE
    model = cfg.get("ELEVENLABS_MODEL") or EL_DEFAULT_MODEL
    body = json.dumps({
        "text": spoken,
        "model_id": model,
        # Lower stability + some style = a livelier, less monotone read.
        "voice_settings": {"stability": 0.4, "similarity_boost": 0.75,
                           "style": 0.45, "use_speaker_boost": True},
    }).encode("utf-8")
    req = urllib.request.Request(
        EL_API_BASE + voice + "?output_format=mp3_44100_128",
        data=body, method="POST",
        headers={"xi-api-key": key, "Content-Type": "application/json",
                 "Accept": "audio/mpeg"})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return resp.read()
    except urllib.error.HTTPError as e:
        detail = e.read()[:200].decode("utf-8", "replace")
        log("WARN: ElevenLabs HTTP %s — %s" % (e.code, detail))
    except Exception as e:  # noqa: BLE001
        log("WARN: ElevenLabs request failed: %s" % e)
    return None


def _speak_elevenlabs(spoken, cfg):
    """ElevenLabs cloud TTS -> MP3 -> decode -> playback. Returns True on success."""
    mp3 = _synth_elevenlabs(spoken, cfg)
    if not mp3:
        return False
    try:
        with open(TTS_MP3, "wb") as f:
            f.write(mp3)
        # pw-play wants WAV; ffmpeg decodes the MP3 (ffmpeg is always present
        # on bazzite — Sunshine streaming depends on it) and applies the
        # loudness boost so the voice rides above the game audio.
        cmd = ["ffmpeg", "-y", "-i", TTS_MP3]
        af = _af_filter(cfg)
        if af:
            cmd += ["-af", af]
        cmd.append(TTS_WAV)
        subprocess.run(cmd, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, timeout=30, check=True)
    except Exception as e:  # noqa: BLE001
        log("WARN: ElevenLabs audio decode failed: %s" % e)
        return False
    voice = cfg.get("ELEVENLABS_VOICE_ID") or EL_DEFAULT_VOICE
    return _play(TTS_WAV, "%d chars, ElevenLabs %s" % (len(spoken), voice))


def speak(text, cfg, is_book=False):
    """Speak `text` aloud. Supersedes any current speech.

    GW2_CLAUDE_TTS_ENGINE: elevenlabs = always ElevenLabs; piper = always local
    Piper; auto = ElevenLabs for book reads only, Piper for everything else (so
    the premium voice is spent on long-form books, not quick tooltip reads)."""
    stop_speaking()

    if (cfg.get("GW2_CLAUDE_TTS", "on") or "on").lower() in ("off", "0", "false", "no"):
        return
    spoken = clean_for_speech(text)
    if not spoken:
        return

    engine = (cfg.get("GW2_CLAUDE_TTS_ENGINE") or "auto").lower()
    use_el = (engine == "elevenlabs"
              or (engine == "auto" and is_book and cfg.get("ELEVENLABS_API_KEY")))

    if use_el:
        if _speak_elevenlabs(spoken, cfg):
            return
        log("ElevenLabs unavailable — falling back to local Piper")
    _speak_piper(spoken, cfg)


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


# ---------------------------------------------------------------------------
# Phase 2 actions. The addon prefixes the .prompt file with "@action:<name>"
# to ask for something richer than a plain read — a Trading Post price, a wiki
# favourite, or AI research with web search.
# ---------------------------------------------------------------------------

def _http_get(url, timeout=20):
    """GET a URL and return the raw body bytes."""
    req = urllib.request.Request(url, headers={"User-Agent": "gw2-claude-daemon"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read()


def _http_get_json(url, timeout=20):
    """GET a URL and parse the body as JSON."""
    return json.loads(_http_get(url, timeout).decode("utf-8", "replace"))


def _vision(client, model, image_path, instruction, max_tokens=200):
    """One vision call — returns Claude's raw text answer for `instruction`."""
    png = prepare_image(image_path)
    b64 = base64.standard_b64encode(png).decode("ascii")
    resp = client.messages.create(
        model=model,
        max_tokens=max_tokens,
        messages=[{
            "role": "user",
            "content": [
                {"type": "image", "source": {"type": "base64",
                 "media_type": "image/png", "data": b64}},
                {"type": "text", "text": instruction},
            ],
        }],
    )
    return "".join(b.text for b in resp.content
                   if getattr(b, "type", None) == "text").strip()


def claude_identify(client, model, image_path, instruction):
    """One short vision call — pulls an item / topic name out of a crop.
    Strips quotes / trailing punctuation the model sometimes adds."""
    return _vision(client, model, image_path, instruction, 120).strip(
        "\"'.‘’“” ").strip()


def format_coins(copper):
    """Render a copper amount as 'N gold N silver N copper' (GW2 coin)."""
    try:
        c = int(copper)
    except (TypeError, ValueError):
        return "an unknown amount"
    g, c = divmod(c, 10000)
    s, c = divmod(c, 100)
    parts = []
    if g:
        parts.append("%d gold" % g)
    if s:
        parts.append("%d silver" % s)
    if c or not parts:
        parts.append("%d copper" % c)
    return " ".join(parts)


# A gw2bltc.com search-result row — the item's id and display name. Anchored
# on the td-name cell so it is independent of which price/stat columns show.
_BLTC_ROW = re.compile(
    r'td-name[^<]*<a\s+href="/en/item/(\d+)-[^"]*">([^<]+)</a>')


def bltc_find_item(name):
    """Resolve a GW2 item name to its numeric item id via a gw2bltc.com search.

    gw2tp's bulk name list (api.gw2tp.com) is dead, and the official GW2 API
    has no name search — so, like the itinyk portal's flips page, we search
    gw2bltc.com. Its name search is substring-based, so a partial name such as
    "Powerful Blood" still resolves ("Vial of Powerful Blood").

    Returns (item_id, canonical_name), or (None, None) if nothing matched."""
    try:
        req = urllib.request.Request(
            BLTC_SEARCH + "?name=" + urllib.parse.quote(name),
            headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=15) as resp:
            html = resp.read().decode("utf-8", "replace")
    except Exception as e:  # noqa: BLE001
        log("WARN: BLTC search failed for %r: %s" % (name, e))
        return None, None
    hits = [(int(m.group(1)), m.group(2).strip())
            for m in _BLTC_ROW.finditer(html)]
    if not hits:
        return None, None
    low = name.strip().lower()
    for iid, nm in hits:                # an exact name match wins
        if nm.lower() == low:
            return iid, nm
    hits.sort(key=lambda h: len(h[1]))   # else the closest (shortest) name
    return hits[0]


def _fetch_item(item_id):
    """Best-effort GW2 item lookup. Returns (canonical_name, vendor_value,
    no_sell) — vendor_value is the sell-to-vendor price in copper."""
    try:
        item = _http_get_json("%s/items/%s" % (GW2_API, item_id), 15)
    except Exception as e:  # noqa: BLE001
        log("WARN: item lookup failed for %s: %s" % (item_id, e))
        return None, 0, False
    flags = item.get("flags") or []
    return item.get("name"), int(item.get("vendor_value") or 0), "NoSell" in flags


def action_trading_post(client, model, image_path):
    """Identify the item in the crop and report Trading Post + vendor prices.

    On success the result has two parts: a machine-readable "@TP|..." marker
    line that the Mystic AI panel parses into gold/silver/copper coin rows,
    then the spoken prose. clean_for_speech() drops the marker before TTS.
    Lookup failures return plain text (no marker) — the panel shows those as
    a normal message."""
    name = claude_identify(client, model, image_path,
        "This is a Guild Wars 2 screenshot of a single item — an inventory "
        "slot, a tooltip, or a listing. Reply with ONLY that item's exact "
        "in-game name: no quotes, no extra words. If you cannot tell, reply "
        "NONE.")
    if not name or name.upper() == "NONE":
        return "I couldn't make out an item name in that selection."

    item_id, bltc_name = bltc_find_item(name)
    if item_id is None:
        return ("I couldn't find %s on the Trading Post — it may be "
                "account-bound or not tradeable." % name)

    # Canonical name + sell-to-vendor value from the item API.
    canon, vendor, no_sell = _fetch_item(item_id)
    disp_name = canon or bltc_name or name
    if no_sell:
        vendor = -1

    # Trading Post buy/sell. A side with no orders reports unit_price 0 —
    # map that to -1 ("none") so the panel and speech say so plainly.
    buy = sell = -1
    try:
        price = _http_get_json("%s/commerce/prices/%s" % (GW2_API, item_id), 15)
        buy = int(price.get("buys", {}).get("unit_price") or 0) or -1
        sell = int(price.get("sells", {}).get("unit_price") or 0) or -1
    except urllib.error.HTTPError as e:
        if e.code != 404:   # 404 = not currently listed; vendor row still shown
            return "The Trading Post API didn't answer for %s (HTTP %s)." % (
                disp_name, e.code)
    except Exception as e:  # noqa: BLE001
        return "I couldn't reach the Trading Post for %s right now." % disp_name

    say = ["%s." % disp_name,
           "Highest buy order, %s." % (format_coins(buy) if buy >= 0
                                       else "no buy orders"),
           "Lowest sell listing, %s." % (format_coins(sell) if sell >= 0
                                          else "no sell listings")]
    if vendor > 0:
        say.append("Vendor value, %s." % format_coins(vendor))
    elif vendor < 0:
        say.append("It can't be sold to a vendor.")

    marker = "@TP|buy=%d|sell=%d|vendor=%d|name=%s" % (buy, sell, vendor, disp_name)
    return marker + "\n" + " ".join(say)


OVERVIEW_INSTRUCTION = (
    "This is a cropped region of the Guild Wars 2 screen.\n"
    "If it shows a single game ITEM (an inventory slot, an item tooltip, a "
    "trading-post listing), reply with EXACTLY these two lines and nothing "
    "else:\n"
    "NAME: <the item's exact in-game name>\n"
    "ABOUT: <one or two plain sentences on what the item is and what it is "
    "used for>\n"
    "If it is NOT a single item — dialogue, quest text, a book page, mail, a "
    "menu, chat — reply instead with one line:\n"
    "TEXT: <all of the visible text, in full>\n"
    "Do not add any other commentary."
)


def action_overview(client, model, image_path):
    """Default drag-select action. Identify the item and build an on-screen
    overview — name, what it is, Trading Post buy/sell and vendor value. The
    panel shows it silently; the Read button voices the SPOKEN line. A non-item
    selection comes back as a plain transcription instead."""
    raw = _vision(client, model, image_path, OVERVIEW_INSTRUCTION, 1500).strip()
    if raw.upper().startswith("TEXT:"):
        return raw[5:].strip()                  # not an item — plain text

    name = about = ""
    for line in raw.splitlines():
        s = line.strip()
        if s.upper().startswith("NAME:"):
            name = s[5:].strip().strip("\"'")
        elif s.upper().startswith("ABOUT:"):
            about = s[6:].strip()
    if not name:
        return raw                              # unparseable — show as-is

    # Trading Post + vendor — best-effort; the overview still shows without it.
    buy = sell = vendor = -1
    disp_name = name
    item_id, bltc_name = bltc_find_item(name)
    if item_id is not None:
        canon, vendor, no_sell = _fetch_item(item_id)
        disp_name = canon or bltc_name or name
        if no_sell:
            vendor = -1
        try:
            price = _http_get_json("%s/commerce/prices/%s" % (GW2_API, item_id), 15)
            buy = int(price.get("buys", {}).get("unit_price") or 0) or -1
            sell = int(price.get("sells", {}).get("unit_price") or 0) or -1
        except Exception:  # noqa: BLE001
            pass

    # SPOKEN line — what the Read button voices on demand.
    say = [disp_name + ".", about]
    if buy >= 0 or sell >= 0:
        say.append("Trading Post buy %s, sell %s."
                   % (format_coins(buy) if buy >= 0 else "no orders",
                      format_coins(sell) if sell >= 0 else "no listings"))
    if vendor > 0:
        say.append("Vendor value %s." % format_coins(vendor))
    spoken = " ".join(p for p in say if p)

    marker = "@OV|buy=%d|sell=%d|vendor=%d|name=%s" % (buy, sell, vendor, disp_name)
    return "%s\nABOUT:%s\nSPOKEN:%s" % (marker, about, spoken)


def action_copy_name(client, model, image_path):
    """Smart Copy — pull ONLY the item's name out of the crop. GW2's
    destroy-item confirmation shows the name in rarity-coloured text and can
    wrap it over two lines; the addon puts the returned name on the clipboard."""
    name = _vision(client, model, image_path,
        "This is a Guild Wars 2 destroy-item confirmation box or an item "
        "tooltip. The item's name is the coloured text (orange = exotic, "
        "yellow = rare, blue = masterwork, green = fine, white = basic). Reply "
        "with ONLY that item's exact name as a single line of plain text — if "
        "it wraps onto two lines on screen, join it into one. No quotes, no "
        "other words. If there is no item name, reply NONE.", 80)
    name = name.strip().strip("\"'.‘’“” ").strip()
    if not name or name.upper() == "NONE":
        return "@COPY|"
    return "@COPY|" + name.splitlines()[0].strip()


def _add_wiki_favorite(page_id, title):
    """Append a favourite to NexusGameWiki's library.json. Returns False if it
    was already there, or on failure."""
    data = {"favorites": [], "recent": []}
    try:
        with open(WIKI_LIBRARY) as f:
            loaded = json.load(f)
        if isinstance(loaded, dict):
            data = loaded
    except (OSError, ValueError):
        pass
    favs = data.setdefault("favorites", [])
    if any(isinstance(f, dict) and f.get("pageId") == page_id for f in favs):
        return False
    favs.append({"pageId": page_id, "savedAt": int(time.time()), "title": title})
    try:
        os.makedirs(os.path.dirname(WIKI_LIBRARY), exist_ok=True)
        with open(WIKI_LIBRARY, "w") as f:
            json.dump(data, f, indent=2)
    except OSError as e:
        log("WARN: could not write %s: %s" % (WIKI_LIBRARY, e))
        return False
    return True


def action_wiki_fav(client, model, image_path):
    """Identify the subject of the crop and add its wiki page to the
    NexusGameWiki favourites."""
    term = claude_identify(client, model, image_path,
        "This is a Guild Wars 2 screenshot. Reply with ONLY the name of the "
        "main item, skill, NPC, or place shown — the thing worth looking up on "
        "the wiki — with no quotes or extra words. If unclear, reply NONE.")
    if not term or term.upper() == "NONE":
        return "I couldn't tell what to look up on the wiki."

    try:
        url = ("%s?action=query&list=search&srsearch=%s&srlimit=1&format=json"
               % (WIKI_API, urllib.parse.quote(term)))
        hits = _http_get_json(url, 15).get("query", {}).get("search", [])
    except Exception as e:  # noqa: BLE001
        return "I couldn't reach the GW2 wiki to look up %s." % term
    if not hits:
        return "I found no wiki page for %s." % term

    page_id = hits[0].get("pageid")
    title = hits[0].get("title", term)
    if _add_wiki_favorite(page_id, title):
        return "Added %s to your wiki favourites." % title
    return "%s is already in your wiki favourites." % title


RESEARCH_SYSTEM = (
    "You are a Guild Wars 2 expert. The player has shown you a region of their "
    "screen. Identify the main item, skill, trait, NPC, event, or place, then "
    "research it thoroughly with the GW2 wiki and current web sources and give "
    "a genuinely in-depth explanation — well beyond prices. Cover what it is, "
    "where and how it is obtained, what it is used for (crafting, builds, "
    "collections, achievements), how it fits the current game and economy, "
    "and a few practical tips or things players often get wrong. Be thorough "
    "and specific with real detail. Answer as flowing spoken prose — several "
    "paragraphs of plain conversational text, with no markdown, no bullet "
    "points, and no preamble such as 'I'll look that up'."
)


def action_research(client, cfg, image_path, question):
    """Web-search-backed deep explanation of whatever is in the crop."""
    research_model = cfg.get("GW2_CLAUDE_RESEARCH_MODEL") or "claude-sonnet-4-6"
    png = prepare_image(image_path)
    b64 = base64.standard_b64encode(png).decode("ascii")
    user_text = "Research what is shown here and explain it."
    if question:
        user_text += " The player also asks: " + question
    content = [
        {"type": "image", "source": {"type": "base64",
         "media_type": "image/png", "data": b64}},
        {"type": "text", "text": user_text},
    ]

    def _call(tools):
        kwargs = {
            "model": research_model,
            "max_tokens": 2048,
            "system": RESEARCH_SYSTEM,
            "messages": [{"role": "user", "content": content}],
        }
        if tools:
            kwargs["tools"] = tools
        resp = client.messages.create(**kwargs)
        return "".join(b.text for b in resp.content
                        if getattr(b, "type", None) == "text").strip()

    try:
        text = _call([{"type": "web_search_20250305", "name": "web_search",
                       "max_uses": 5}])
    except Exception as e:  # noqa: BLE001
        # Web search may not be enabled for the org — fall back to a plain
        # knowledge-based answer rather than failing the whole request.
        log("WARN: research web search failed (%s) — answering without it" % e)
        text = _call(None)
    return text or "(no research result)"


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

            # Read the prompt and split off any "@action:<name>" prefix first —
            # the "say" action carries text only and needs no captured image.
            question = None
            try:
                with open(prompt_path) as f:
                    question = f.read()
            except FileNotFoundError:
                pass

            action = "read"
            if question and question.lstrip().startswith("@action:"):
                first, _, rest = question.lstrip().partition("\n")
                action = first.strip()[len("@action:"):].strip().lower() or "read"
                question = rest
            if question is not None:
                question = question.strip()
            if action != "read":
                log("request=%s action=%s" % (suffix, action))

            # "say" — voice the supplied text aloud; no image, no Claude call,
            # fire-and-forget (the Read button sends this to read whatever is
            # already on the panel, with no fresh API round-trip and no reply).
            if action == "say":
                stop_speaking()
                if os.path.exists(STOP_MARKER):
                    _safe_rm(STOP_MARKER)
                else:
                    speak(question or "", cfg)
                _safe_rm(prompt_path)
                _safe_rm(ready)
                continue

            image_path = find_image(suffix)
            if image_path is None:
                log("WARN: no image for request %s — skipping" % suffix)
                _safe_rm(ready)
                continue

            # Book reads (Read-Book keybind) — the marker phrase must match
            # overlay.cpp BOOK_PROMPT.
            is_book = (action == "read" and bool(question)
                       and "in-game Guild Wars 2 book" in question)

            # A fresh request supersedes whatever is being spoken.
            stop_speaking()

            ok = True
            try:
                if action == "overview":
                    result = action_overview(client, model, image_path)
                elif action == "trading-post":
                    result = action_trading_post(client, model, image_path)
                elif action == "copy-name":
                    result = action_copy_name(client, model, image_path)
                elif action == "wiki-fav":
                    result = action_wiki_fav(client, model, image_path)
                elif action == "research":
                    result = action_research(client, cfg, image_path, question)
                else:
                    result = analyze(client, model, image_path, question)
            except Exception as e:  # noqa: BLE001
                result = "ERROR: %s" % e
                ok = False
                log("ERROR request=%s action=%s %s" % (suffix, action, e))

            with open(out_path, "w") as f:
                f.write(result)
            open(done_mark, "w").close()
            _safe_rm(ready)

            # Speak only what is meant to be heard — a plain or book read, and
            # Research. The overview, TP, copy-name and favourite actions show
            # on the panel silently; the Read button re-sends them as "say".
            if ok:
                if os.path.exists(STOP_MARKER):
                    _safe_rm(STOP_MARKER)
                    log("read %s cancelled before speech" % suffix)
                elif action in ("read", "research"):
                    speak(result, cfg, is_book)

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
