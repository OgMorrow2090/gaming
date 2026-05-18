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
if [ ! -f "$VOICES/en_GB-northern_english_male-medium.onnx" ]; then
    echo "Downloading Piper voice en_GB-northern_english_male-medium..."
    "$VENV/bin/python" -m piper.download_voices en_GB-northern_english_male-medium --data-dir "$VOICES"
fi

if [ ! -f "$CFGDIR/config.env" ]; then
    cat > "$CFGDIR/config.env" <<'EOF'
# gw2-claude-daemon config — KEY=VALUE, no quotes.

# Required. Anthropic API key for the GW2 Claude-vision daemon.
ANTHROPIC_API_KEY=

# Optional. Model used to read the screen.
#   claude-haiku-4-5  — cheapest/fastest, solid vision (default — ~0.2c/read)
#   claude-sonnet-4-6 — stronger reading, mid cost
#   claude-opus-4-7   — most capable; use when coin-digit precision matters
GW2_CLAUDE_MODEL=claude-haiku-4-5

# Optional. Text-to-speech — the daemon reads Claude's answer aloud.
#   GW2_CLAUDE_TTS:   on / off
#   GW2_CLAUDE_VOICE: any Piper voice in ~/.local/share/gw2-claude/voices/
GW2_CLAUDE_TTS=on
GW2_CLAUDE_VOICE=en_GB-northern_english_male-medium
EOF
    chmod 600 "$CFGDIR/config.env"
    echo ""
    echo ">>> Created $CFGDIR/config.env — add your ANTHROPIC_API_KEY before"
    echo ">>> starting gw2-claude-daemon.service."
fi

echo ""
"$VENV/bin/python" - <<'PY'
import anthropic, PIL, piper  # noqa: F401
print("OK: anthropic", anthropic.__version__, "+ Pillow", PIL.__version__, "+ Piper")
PY
echo "venv ready: $VENV"
