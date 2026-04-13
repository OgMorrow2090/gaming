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
EXTRA=""
case "$MODE" in
  4k120)    EXTRA="-W 3840 -H 2160 -r 120 -o 120 --immediate-flips" ;;
  4k60)     EXTRA="-W 3840 -H 2160 -r 60  -o 60"  ;;
  deck)     EXTRA="-W 1280 -H 800  -r 90  -o 90"  ;;
  1080p144) EXTRA="-W 1920 -H 1080 -r 144 -o 144 --immediate-flips" ;;
  default)  EXTRA="" ;;
  *)        EXTRA="" ;;
esac
exec /usr/bin/gamescope "$@" $EXTRA
