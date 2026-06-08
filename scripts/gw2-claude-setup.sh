#!/bin/bash
#
# gw2-claude-setup.sh — one-time setup for the Claude-vision daemon on bazzite.
#
# bazzite is an immutable rpm-ostree OS, but that only blocks SYSTEM-wide
# package changes — a per-user Python venv in $HOME is unaffected and survives
# OS updates. We use a venv so the official `anthropic` SDK (and Pillow, for
# BMP->PNG conversion) can be installed without touching the base image.
#
# Idempotent: safe to re-run after OS updates or to pull SDK updates.
set -euo pipefail

VENV="$HOME/.local/share/gw2-claude/venv"
CFGDIR="$HOME/.config/gw2-claude"

mkdir -p "$(dirname "$VENV")" "$CFGDIR"

if [ ! -d "$VENV" ]; then
    echo "Creating venv at $VENV"
    python3 -m venv "$VENV"
fi

echo "Installing/updating anthropic SDK + Pillow + Piper TTS into the venv..."
"$VENV/bin/pip" install --quiet --upgrade pip
"$VENV/bin/pip" install --quiet --upgrade anthropic pillow piper-tts

VOICES="$HOME/.local/share/gw2-claude/voices"
mkdir -p "$VOICES"
if [ ! -f "$VOICES/en_GB-jenny_dioco-medium.onnx" ]; then
    echo "Downloading Piper voice en_GB-jenny_dioco-medium (British-female fallback)..."
    "$VENV/bin/python" -m piper.download_voices en_GB-jenny_dioco-medium --data-dir "$VOICES"
fi

if [ ! -f "$CFGDIR/config.env" ]; then
    cat > "$CFGDIR/config.env" <<'EOF'
# gw2-claude-daemon config — KEY=VALUE, no quotes.

# Credential (set ONE). The daemon prefers the OAuth token if both are present.
#
# Preferred: Max-subscription OAuth token — billed against the subscription, no
# metered API credits. Mint with `claude setup-token` (valid ~1yr); this is the
# same token portal.itinyk.app uses (op://wednesday-pi/claude_code_oauth_token).
CLAUDE_CODE_OAUTH_TOKEN=
# Fallback: pay-per-use Anthropic API key (sk-ant-api03-...).
ANTHROPIC_API_KEY=

# Optional. Model used to read the screen.
#   claude-haiku-4-5  — cheapest/fastest, solid vision (default — ~0.2c/read)
#   claude-sonnet-4-6 — stronger reading, mid cost
#   claude-opus-4-7   — most capable; use when coin-digit precision matters
GW2_CLAUDE_MODEL=claude-haiku-4-5

# Optional. Text-to-speech — the daemon reads Claude's answer aloud.
#   GW2_CLAUDE_TTS:        on / off
#   GW2_CLAUDE_TTS_ENGINE: auto / piper / elevenlabs (auto = ElevenLabs for books, Piper otherwise)
#   GW2_CLAUDE_VOICE:      any Piper voice in ~/.local/share/gw2-claude/voices/
GW2_CLAUDE_TTS=on
GW2_CLAUDE_TTS_ENGINE=auto
GW2_CLAUDE_VOICE=en_GB-jenny_dioco-medium

# Loudness boost (dB) so the voice rides above the game audio. Boost is
# limiter-protected, so it stays clean — raise it for an even louder read.
GW2_CLAUDE_TTS_GAIN_DB=12

# Optional. ElevenLabs cloud TTS — far more natural than Piper. Leave the key
# blank to stay on Piper. ELEVENLABS_VOICE_ID picks the voice (browse
# elevenlabs.io); ELEVENLABS_MODEL defaults to eleven_turbo_v2_5.
ELEVENLABS_API_KEY=
ELEVENLABS_VOICE_ID=
ELEVENLABS_MODEL=eleven_turbo_v2_5
EOF
    chmod 600 "$CFGDIR/config.env"
    echo ""
    echo ">>> Created $CFGDIR/config.env — set CLAUDE_CODE_OAUTH_TOKEN (preferred)"
    echo ">>> or ANTHROPIC_API_KEY before starting gw2-claude-daemon.service."
fi

echo ""
"$VENV/bin/python" - <<'PY'
import anthropic, PIL, piper  # noqa: F401
print("OK: anthropic", anthropic.__version__, "+ Pillow", PIL.__version__, "+ Piper")
PY
echo "venv ready: $VENV"
