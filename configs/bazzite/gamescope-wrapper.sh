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
# -w/-h: nested xwayland init size. Without these, xwayland defaults to the
#   EDID max. Even with these, Steam/Wine xrandr-resize xwayland to the EDID
#   max once they start — which is why we ALSO need --force-windows-fullscreen.
# --force-windows-fullscreen: forces every window inside gamescope to the
#   nested display size (-w/-h). This prevents Steam Big Picture and GW2
#   from bumping xwayland up to the EDID max post-launch — without this,
#   gamescope downscales 2K renders into a 1280x800 output for the Deck
#   stream, producing tiny 2K-sized UI on the Deck panel ("stuck 2K").
# --generate-drm-mode fixed: pin DRM mode selection to EDID-listed modes
#   (so deck mode actually drives 1280x800@90 to the connector instead of
#   gamescope CVT-generating 2560x1440@144).
EXTRA="--force-windows-fullscreen --generate-drm-mode fixed"
case "$MODE" in
  4k120)    EXTRA="$EXTRA -W 3840 -H 2160 -w 3840 -h 2160 -r 120 -o 120 --immediate-flips" ;;
  4k60)     EXTRA="$EXTRA -W 3840 -H 2160 -w 3840 -h 2160 -r 60  -o 60"  ;;
  2k120)    EXTRA="$EXTRA -W 2560 -H 1440 -w 2560 -h 1440 -r 120 -o 120 --immediate-flips" ;;
  deck)     EXTRA="$EXTRA -W 1280 -H 800  -w 1280 -h 800  -r 90  -o 90"  ;;
  1080p144) EXTRA="$EXTRA -W 1920 -H 1080 -w 1920 -h 1080 -r 144 -o 144 --immediate-flips" ;;
  default)  EXTRA="" ;;
  *)        EXTRA="" ;;
esac
exec /usr/bin/gamescope "$@" $EXTRA
