#!/bin/bash
# gamescope-wrapper: reads ~/.config/gamescope-mode to pick output params
# so users can switch between Apple TV 4K/120, Apple TV 4K/60, Deck 1280x800,
# and ultrawide 1080p@144 without editing this file.
#
# Default if state file missing: no forcing (uses EDID-preferred, 4K@60)
# Switch modes via: ~/bin/gamescope-mode <name>
#
# Modes:
#   4k120       3840x2160 @ 120Hz  — Apple TV Moonlight high fps
#   4k60        3840x2160 @ 60Hz   — Apple TV Moonlight, baseline
#   deck        1280x800  @ 90Hz   — Steam Deck Moonlight
#   1080p144    1920x1080 @ 144Hz  — legacy Moonlight client
#   default     (no override)      — EDID-preferred, lets Steam UI pick
MODE_FILE="$HOME/.config/gamescope-mode"
MODE="default"
[ -f "$MODE_FILE" ] && MODE=$(cat "$MODE_FILE" 2>/dev/null | tr -d '[:space:]')
EXTRA=""
case "$MODE" in
  4k120)    EXTRA="-W 3840 -H 2160 -r 120" ;;
  4k60)     EXTRA="-W 3840 -H 2160 -r 60"  ;;
  deck)     EXTRA="-W 1280 -H 800  -r 90"  ;;
  1080p144) EXTRA="-W 1920 -H 1080 -r 144" ;;
  default)  EXTRA="" ;;
  *)        EXTRA="" ;;
esac
exec /usr/bin/gamescope "$@" $EXTRA
