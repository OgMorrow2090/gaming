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

echo "Installing/updating anthropic SDK + Pillow into the venv..."
"$VENV/bin/pip" install --quiet --upgrade pip
"$VENV/bin/pip" install --quiet --upgrade anthropic pillow

if [ ! -f "$CFGDIR/config.env" ]; then
    cat > "$CFGDIR/config.env" <<'EOF'
# gw2-claude-daemon config — KEY=VALUE, no quotes.

# Required. Anthropic API key for the GW2 Claude-vision daemon.
ANTHROPIC_API_KEY=

# Optional. Model used to read the screen.
#   claude-opus-4-7   — most capable (default)
#   claude-sonnet-4-6 — cheaper, still strong vision
#   claude-haiku-4-5  — cheapest/fastest; good for high-frequency simple reads
GW2_CLAUDE_MODEL=claude-opus-4-7
EOF
    chmod 600 "$CFGDIR/config.env"
    echo ""
    echo ">>> Created $CFGDIR/config.env — add your ANTHROPIC_API_KEY before"
    echo ">>> starting gw2-claude-daemon.service."
fi

echo ""
"$VENV/bin/python" - <<'PY'
import anthropic, PIL
print("OK: anthropic", anthropic.__version__, "+ Pillow", PIL.__version__)
PY
echo "venv ready: $VENV"
