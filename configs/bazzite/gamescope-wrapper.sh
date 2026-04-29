#!/bin/bash
# gamescope-wrapper: reads ~/.config/gamescope-mode to pick output params.
# Switch modes via: ~/bin/gamescope-mode <name>
#
# --immediate-flips: enables tearing, bypassing VSync. Required when the
#   EDID only advertises 4K@60 as a DTD — gamescope's ValidRefreshRates
#   list is built from the EDID's DTD pixel clocks, and 4K@120 (1188 MHz)
#   exceeds EDID 1.3's 655 MHz DTD pixel clock limit. Without
#   --immediate-flips, DXVK FIFO swap chain waits for a 60Hz vblank and
#   caps GW2 at 60fps even though gamescope composites at 120Hz.
#
# -r / -o: focused / unfocused nested refresh (must match for dual
#   Xwayland mode — GW2 runs on the second xwayland).
MODE_FILE="$HOME/.config/gamescope-mode"
MODE="default"
[ -f "$MODE_FILE" ] && MODE=$(cat "$MODE_FILE" 2>/dev/null | tr -d '[:space:]')
# -w/-h (nested internal size) must be set in addition to -W/-H (output size)
# or xwayland exposes the EDID max as the desktop size, defeating per-mode
# resizing. With both set, windowed_fullscreen apps (like GW2) inherit the
# correct per-mode desktop dimensions.
EXTRA=""
case "$MODE" in
  4k120)    EXTRA="-W 3840 -H 2160 -w 3840 -h 2160 -r 120 -o 120 --immediate-flips" ;;
  4k60)     EXTRA="-W 3840 -H 2160 -w 3840 -h 2160 -r 60  -o 60"  ;;
  2k120)    EXTRA="-W 2560 -H 1440 -w 2560 -h 1440 -r 120 -o 120 --immediate-flips" ;;
  deck)     EXTRA="-W 1280 -H 800  -w 1280 -h 800  -r 90  -o 90"  ;;
  1080p144) EXTRA="-W 1920 -H 1080 -w 1920 -h 1080 -r 144 -o 144 --immediate-flips" ;;
  default)  EXTRA="" ;;
  *)        EXTRA="" ;;
esac
exec /usr/bin/gamescope "$@" $EXTRA
