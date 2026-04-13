#!/bin/bash
# gamescope-wrapper: reads ~/.config/gamescope-mode to pick output params.
# Switch modes via: ~/bin/gamescope-mode <name>
#
# -r  focused game refresh rate
# -o  unfocused game refresh rate — MUST match -r or games running on the
#     second Xwayland (in --xwayland-count 2 mode) will see 60Hz and cap
#     their fps. GW2 in particular attaches to the second Xwayland.
MODE_FILE="$HOME/.config/gamescope-mode"
MODE="default"
[ -f "$MODE_FILE" ] && MODE=$(cat "$MODE_FILE" 2>/dev/null | tr -d '[:space:]')
EXTRA=""
case "$MODE" in
  4k120)    EXTRA="-W 3840 -H 2160 -r 120 -o 120" ;;
  4k60)     EXTRA="-W 3840 -H 2160 -r 60  -o 60"  ;;
  deck)     EXTRA="-W 1280 -H 800  -r 90  -o 90"  ;;
  1080p144) EXTRA="-W 1920 -H 1080 -r 144 -o 144" ;;
  default)  EXTRA="" ;;
  *)        EXTRA="" ;;
esac
exec /usr/bin/gamescope "$@" $EXTRA
