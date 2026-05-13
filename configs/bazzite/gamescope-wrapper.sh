#!/bin/bash
# gamescope-wrapper: reads ~/.config/gamescope-mode and sets matching
# gamescope output + resolution flags. Called in place of /usr/bin/gamescope
# by the bazzite session.
#
# Modes must match those in ~/bin/gamescope-mode.

MODE_FILE="$HOME/.config/gamescope-mode"
MODE="default"
[ -f "$MODE_FILE" ] && MODE=$(cat "$MODE_FILE" 2>/dev/null | tr -d "[:space:]")

case "$MODE" in
  4k165)     exec /usr/bin/gamescope "$@" --prefer-output HDMI-A-1 -W 3840 -H 2160 -r 165 -o 165 --immediate-flips ;;
  4k120)     exec /usr/bin/gamescope "$@" --prefer-output HDMI-A-1 -W 3840 -H 2160 -r 120 -o 120 --immediate-flips ;;
  4k60)      exec /usr/bin/gamescope "$@" --prefer-output HDMI-A-1 -W 3840 -H 2160 -r 60  -o 60  --immediate-flips ;;
  2k165)     exec /usr/bin/gamescope "$@" --prefer-output HDMI-A-1 -W 2560 -H 1440 -r 165 -o 165 --immediate-flips ;;
  2k120)     exec /usr/bin/gamescope "$@" --prefer-output HDMI-A-1 -W 2560 -H 1440 -r 120 -o 120 --immediate-flips ;;
  1080p165)  exec /usr/bin/gamescope "$@" --prefer-output HDMI-A-1 -W 1920 -H 1080 -r 165 -o 165 --immediate-flips ;;
  1080p144)  exec /usr/bin/gamescope "$@" --prefer-output HDMI-A-1 -W 1920 -H 1080 -r 144 -o 144 --immediate-flips ;;
  deck)      exec /usr/bin/gamescope "$@" --prefer-output HDMI-A-2 -W 1280 -H 800  -r 90  -o 90  --immediate-flips ;;
  deck-vkms) exec /usr/bin/gamescope "$@" -W 1280 -H 800  -r 90  -o 90  --immediate-flips ;;
  default|*) exec /usr/bin/gamescope "$@" --prefer-output HDMI-A-1 -W 3840 -H 2160 -r 60  -o 60  --immediate-flips ;;
esac
