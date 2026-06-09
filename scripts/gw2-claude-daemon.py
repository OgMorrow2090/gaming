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
    CLAUDE_CODE_OAUTH_TOKEN=sk-ant-oat01-... (preferred — Max-subscription
                                            billing, no metered API credits;
                                            same token as portal.itinyk.app)
    ANTHROPIC_API_KEY=sk-ant-...           (fallback — pay-per-use API credits)
    (one of the two above is required; the OAuth token wins if both are set)
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
CRAFTY_LIBRARY = os.path.expanduser(
    "~/.local/share/Steam/steamapps/common/Guild Wars 2/addons/CraftyLegend/favourites.json")

# Deferred favourites queue. NexusGameWiki (and CraftyLegend) rewrite their
# library files from memory on game-close, so a live append while GW2 runs is
# wiped. Instead we queue favourites here and flush them into the addon JSON
# files only while GW2 is NOT running — so the addons read them fresh on the
# next launch. Shape: {"wiki": [{"pageId","title","savedAt"}], "crafty": [int]}.
PENDING_FAVS = os.path.expanduser("~/.cache/gw2-claude/pending-favs.json")

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
    if not cfg.get("CLAUDE_CODE_OAUTH_TOKEN") and not cfg.get("ANTHROPIC_API_KEY"):
        log("FATAL: set CLAUDE_CODE_OAUTH_TOKEN (preferred) or ANTHROPIC_API_KEY "
            "in %s" % CONFIG_PATH)
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
    # Drop a leading "@SECT" line — it flags a colour-coded Research result for
    # the panel; the prose under it is what gets read aloud.
    t = re.sub(r"^@SECT\n", "", t)
    # Drop the "#HEADER#" section markers Research uses, so TTS does not read
    # "hash about hash" — keep the prose under each header.
    t = re.sub(r"^#[A-Z ]+#\s*$", "", t, flags=re.MULTILINE)
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

    The vision model often reads a pluralised stack label ("134 Quartz
    Crystals") while the real item is singular ("Quartz Crystal"), so a
    trailing-'s' singular form is tried as a fallback when the name as read
    finds nothing.

    Returns (item_id, canonical_name), or (None, None) if nothing matched."""
    base = name.strip()
    candidates = [base]
    if len(base) > 3 and base[-1] in ("s", "S"):
        candidates.append(base[:-1])     # plural -> singular fallback
    for cand in candidates:
        try:
            req = urllib.request.Request(
                BLTC_SEARCH + "?name=" + urllib.parse.quote(cand),
                headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=15) as resp:
                html = resp.read().decode("utf-8", "replace")
        except Exception as e:  # noqa: BLE001
            log("WARN: BLTC search failed for %r: %s" % (cand, e))
            continue
        hits = [(int(m.group(1)), m.group(2).strip())
                for m in _BLTC_ROW.finditer(html)]
        if not hits:
            continue
        low = cand.lower()
        for iid, nm in hits:                 # an exact name match wins
            if nm.lower() == low:
                return iid, nm
        hits.sort(key=lambda h: len(h[1]))    # else the closest (shortest) name
        return hits[0]
    return None, None


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
    "This is a cropped region of the Guild Wars 2 screen — almost always a "
    "hovered inventory item and its tooltip.\n"
    "\n"
    "DEFAULT: treat the crop as a single game ITEM and reply with EXACTLY "
    "these three lines, nothing else:\n"
    "NAME: <the item's exact name — the coloured title at the TOP of the "
    "tooltip, singular, with no stack-count number; e.g. 'Quartz Crystal', "
    "not '134 Quartz Crystals'>\n"
    "ABOUT: <one or two plain sentences on what the item is>\n"
    "USES: <one short line: what it is mainly used for>\n"
    "\n"
    "A long, flavourful or descriptive tooltip is STILL an item — containers, "
    "trophies, currencies, crafting materials, gizmos and trading-post "
    "listings all count as an ITEM. The tooltip may also show a stack count, "
    "an 'in Bank' / 'in Inventory' line and a gold/silver/copper value: "
    "IGNORE all of those, they are not part of the name, the description or "
    "the uses.\n"
    "\n"
    "Reply with TEXT instead ONLY when the crop contains no item at all — it "
    "is purely an open book page, an NPC dialogue box, mail, or a chat "
    "window:\n"
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

    name = about = uses = ""
    for line in raw.splitlines():
        s = line.strip()
        if s.upper().startswith("NAME:"):
            name = s[5:].strip().strip("\"'")
        elif s.upper().startswith("ABOUT:"):
            about = s[6:].strip()
        elif s.upper().startswith("USES:"):
            uses = s[5:].strip()
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
    if uses:
        say.append("Used for: " + uses)
    if buy >= 0 or sell >= 0:
        say.append("Trading Post buy %s, sell %s."
                   % (format_coins(buy) if buy >= 0 else "no orders",
                      format_coins(sell) if sell >= 0 else "no listings"))
    if vendor > 0:
        say.append("Vendor value %s." % format_coins(vendor))
    spoken = " ".join(p for p in say if p)

    marker = "@OV|buy=%d|sell=%d|vendor=%d|name=%s" % (buy, sell, vendor, disp_name)
    return "%s\nABOUT:%s\nUSES:%s\nSPOKEN:%s" % (marker, about, uses, spoken)


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


def _gw2_running():
    """True while a Gw2-64.exe process is alive — pending favourites are only
    flushed to the addon JSON files when the game is closed."""
    try:
        return subprocess.run(["pgrep", "-f", "Gw2-64.exe"],
                              stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL).returncode == 0
    except Exception:  # noqa: BLE001
        return False


def _load_pending_favs():
    """Read the deferred-favourites queue. Always returns a well-formed dict."""
    data = {"wiki": [], "crafty": []}
    try:
        with open(PENDING_FAVS) as f:
            loaded = json.load(f)
        if isinstance(loaded, dict):
            if isinstance(loaded.get("wiki"), list):
                data["wiki"] = loaded["wiki"]
            if isinstance(loaded.get("crafty"), list):
                data["crafty"] = loaded["crafty"]
    except (OSError, ValueError):
        pass
    return data


def _save_pending_favs(data):
    """Write the deferred-favourites queue back to disk."""
    try:
        os.makedirs(os.path.dirname(PENDING_FAVS), exist_ok=True)
        with open(PENDING_FAVS, "w") as f:
            json.dump(data, f, indent=2)
    except OSError as e:
        log("WARN: could not write %s: %s" % (PENDING_FAVS, e))


def action_wiki_fav(client, model, image_path):
    """Identify the subject of the crop, resolve its wiki page, and QUEUE it as
    a deferred wiki favourite — flushed to NexusGameWiki's library.json only
    while GW2 is not running, so the addon doesn't overwrite it on game-close."""
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

    pending = _load_pending_favs()
    if any(isinstance(e, dict) and e.get("pageId") == page_id
           for e in pending["wiki"]):
        return "%s is already queued for your wiki favourites." % title
    pending["wiki"].append({"pageId": page_id, "savedAt": int(time.time()),
                            "title": title})
    _save_pending_favs(pending)
    return "Queued %s for your wiki favourites." % title


def action_legendary_fav(client, model, image_path):
    """Identify the item in the crop and QUEUE it as a deferred CraftyLegend
    favourite — flushed to CraftyLegend's favourites.json only while GW2 is not
    running, so the addon doesn't overwrite it on game-close."""
    name = claude_identify(client, model, image_path,
        "This is a Guild Wars 2 screenshot of a single item. Reply with ONLY "
        "that item's exact in-game name: no quotes, no extra words. If you "
        "cannot tell, reply NONE.")
    if not name or name.upper() == "NONE":
        return "I couldn't make out an item name in that selection."

    item_id, bltc_name = bltc_find_item(name)
    if item_id is None:
        return "I couldn't find %s to add to CraftyLegend." % name

    pending = _load_pending_favs()
    if item_id in pending["crafty"]:
        return "%s is already queued for CraftyLegend." % (bltc_name or name)
    pending["crafty"].append(item_id)
    _save_pending_favs(pending)
    return "Queued %s for CraftyLegend." % (bltc_name or name)


def flush_pending_favs():
    """Merge queued favourites into the addon JSON files — but only while GW2 is
    closed. NexusGameWiki / CraftyLegend rewrite those files from memory on
    game-close, so a flush mid-session would be wiped; flushing while the game
    is shut means the addons load the new favourites on the next launch."""
    pending = _load_pending_favs()
    if not pending["wiki"] and not pending["crafty"]:
        return
    if _gw2_running():
        return

    flushed_wiki = flushed_crafty = 0

    # Wiki favourites -> NexusGameWiki library.json (favorites[], dedup pageId).
    if pending["wiki"]:
        data = {"favorites": [], "recent": []}
        try:
            with open(WIKI_LIBRARY) as f:
                loaded = json.load(f)
            if isinstance(loaded, dict):
                data = loaded
        except (OSError, ValueError):
            pass
        favs = data.setdefault("favorites", [])
        data.setdefault("recent", [])
        have = {f.get("pageId") for f in favs if isinstance(f, dict)}
        for entry in pending["wiki"]:
            pid = entry.get("pageId")
            if pid in have:
                continue
            favs.append({"pageId": pid,
                         "savedAt": entry.get("savedAt") or int(time.time()),
                         "title": entry.get("title", "")})
            have.add(pid)
            flushed_wiki += 1
        try:
            os.makedirs(os.path.dirname(WIKI_LIBRARY), exist_ok=True)
            with open(WIKI_LIBRARY, "w") as f:
                json.dump(data, f, indent=2)
        except OSError as e:
            log("WARN: could not write %s: %s" % (WIKI_LIBRARY, e))
            return

    # Crafty favourites -> CraftyLegend favourites.json (favourites[], dedup id).
    if pending["crafty"]:
        data = {"favourites": []}
        try:
            with open(CRAFTY_LIBRARY) as f:
                loaded = json.load(f)
            if isinstance(loaded, dict):
                data = loaded
        except (OSError, ValueError):
            pass
        favs = data.setdefault("favourites", [])
        have = set(f for f in favs if isinstance(f, int))
        for iid in pending["crafty"]:
            if iid in have:
                continue
            favs.append(iid)
            have.add(iid)
            flushed_crafty += 1
        try:
            os.makedirs(os.path.dirname(CRAFTY_LIBRARY), exist_ok=True)
            with open(CRAFTY_LIBRARY, "w") as f:
                json.dump(data, f, indent=2)
        except OSError as e:
            log("WARN: could not write %s: %s" % (CRAFTY_LIBRARY, e))
            return

    # All merged — clear the queue.
    _save_pending_favs({"wiki": [], "crafty": []})
    log("flushed pending favourites: %d wiki, %d crafty"
        % (flushed_wiki, flushed_crafty))


RESEARCH_SYSTEM = (
    "You are a Guild Wars 2 expert. The player has shown you a region of their "
    "screen. Identify the main item, skill, trait, NPC, event, or place, then "
    "research it thoroughly with the GW2 wiki and current web sources and give "
    "a genuinely in-depth explanation — well beyond prices.\n\n"
    "Reply in labelled sections so it is easy to scan. Use ONLY the sections "
    "below that actually apply, and put each header on its own line, written "
    "EXACTLY as shown:\n"
    "#ABOUT#\n"
    "#HOW TO GET#\n"
    "#USES#\n"
    "#RECIPES#\n"
    "#PRICES#\n"
    "#TIPS#\n"
    "Under each header write plain prose — several sentences of conversational "
    "text. Be thorough and specific with real detail. Use no markdown, no "
    "bullet points, and no preamble such as 'I'll look that up'. Output nothing "
    "but the headers and their prose."
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

    main_model = cfg.get("GW2_CLAUDE_MODEL") or "claude-haiku-4-5"
    web_tool = [{"type": "web_search_20250305", "name": "web_search",
                 "max_uses": 5}]

    def _call(tools, model):
        kwargs = {
            "model": model,
            "max_tokens": 2048,
            "system": RESEARCH_SYSTEM,
            "messages": [{"role": "user", "content": content}],
        }
        if tools:
            kwargs["tools"] = tools
        resp = client.messages.create(**kwargs)
        return "".join(b.text for b in resp.content
                        if getattr(b, "type", None) == "text").strip()

    # Degrade gracefully: a subscription OAuth token can be rate-limited (429)
    # on larger models while haiku works, and web search may not be enabled at
    # all. Try the research model with web search, then the (known-good) main
    # model with web search, then the main model with no tools.
    text = ""
    for attempt_model in dict.fromkeys((research_model, main_model)):
        try:
            text = _call(web_tool, attempt_model)
            if text:
                break
        except Exception as e:  # noqa: BLE001
            log("WARN: research (model=%s, web search) failed (%s)" %
                (attempt_model, e))
    if not text:
        try:
            text = _call(None, main_model)
        except Exception as e:  # noqa: BLE001
            log("ERROR: research fallback (model=%s) failed (%s)" %
                (main_model, e))
    if not text:
        return "(no research result)"
    # "@SECT" flags the result as colour-coded sections for the Mystic AI
    # panel; clean_for_speech strips it (and the #...# headers) before TTS.
    return "@SECT\n" + text


def run_daemon(client, model, cfg):
    log("daemon starting (pid=%d model=%s tts=%s)" % (
        os.getpid(), model, cfg.get("GW2_CLAUDE_TTS", "on")))
    last_flush = 0.0
    while True:
        # A stop request kills speech immediately (the addon touches this on a
        # second double-press of the read button).
        if os.path.exists(STOP_MARKER):
            stop_speaking()
            _safe_rm(STOP_MARKER)
        reap_tts()

        # Flush queued favourites into the addon JSON files — throttled, and a
        # no-op unless the queue has entries and GW2 is closed.
        now = time.time()
        if now - last_flush >= 10.0:
            last_flush = now
            flush_pending_favs()

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
                elif action == "legendary-fav":
                    result = action_legendary_fav(client, model, image_path)
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

            # Speak only a plain or book read. Research, overview, TP, copy-name
            # and the favourite actions all show on the panel silently — the
            # Read button re-sends their content as "say" when the player wants
            # to hear it.
            if ok:
                if os.path.exists(STOP_MARKER):
                    _safe_rm(STOP_MARKER)
                    log("read %s cancelled before speech" % suffix)
                elif action == "read":
                    speak(result, cfg, is_book)

        time.sleep(POLL_SECONDS)


def main():
    cfg = load_config()
    model = cfg.get("GW2_CLAUDE_MODEL") or "claude-opus-4-7"

    args = sys.argv[1:]

    if args and args[0] == "--say":
        speak(" ".join(args[1:]), cfg)
        if g_tts_proc is not None:
            g_tts_proc.wait()
        return

    import anthropic
    oauth = cfg.get("CLAUDE_CODE_OAUTH_TOKEN")
    if oauth:
        # Max-subscription OAuth token → Bearer auth, billed against the
        # subscription (no metered API credits). Drop ANTHROPIC_API_KEY from the
        # environment so the SDK can't prefer the x-api-key path.
        os.environ.pop("ANTHROPIC_API_KEY", None)
        client = anthropic.Anthropic(auth_token=oauth)
        log("auth: CLAUDE_CODE_OAUTH_TOKEN (subscription)")
    else:
        os.environ["ANTHROPIC_API_KEY"] = cfg["ANTHROPIC_API_KEY"]
        client = anthropic.Anthropic()
        log("auth: ANTHROPIC_API_KEY (pay-per-use)")

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
