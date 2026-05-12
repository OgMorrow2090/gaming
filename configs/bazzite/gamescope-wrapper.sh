#!/bin/bash
# gamescope-wrapper: TEMP SAFE-MODE for 2026-05-11 evening play
# - HDR flags stripped (eliminate SDR/HDR renegotiation flicker)
# - immediate-flips removed
# - default mode forces 1080p60 baseline (lowest stress on NVIDIA HDMI)
# Will be replaced after AMD GPU swap arrives 2026-05-12.

MODE_FILE="$HOME/.config/gamescope-mode"
MODE="default"
[ -f "$MODE_FILE" ] && MODE=$(cat "$MODE_FILE" 2>/dev/null | tr -d "[:space:]")

# Strip HDR + prefer-output from upstream args.
ORIG_ARGS=("$@")
ARGS=()
skip=0
for arg in "${ORIG_ARGS[@]}"; do
  if [ "$skip" -eq 1 ]; then skip=0; continue; fi
  case "$arg" in
    --hdr-enabled|--hdr-itm-enable|--hdr-debug-force-output|--hdr-debug-force-support|--hdr-debug-heatmap) continue ;;
    --prefer-output) skip=1; continue ;;
  esac
  ARGS+=("$arg")
done

case "$MODE" in
  4k165)     exec /usr/bin/gamescope "${ARGS[@]}" --prefer-output HDMI-A-1 -W 3840 -H 2160 -r 165 -o 165 --immediate-flips ;;
  4k120)     exec /usr/bin/gamescope "${ARGS[@]}" --prefer-output HDMI-A-1 -W 3840 -H 2160 -r 120 -o 120 --immediate-flips ;;
  2k120)     exec /usr/bin/gamescope "${ARGS[@]}" --prefer-output HDMI-A-1 -W 2560 -H 1440 -r 120 -o 120 --immediate-flips ;;
  4k60)      exec /usr/bin/gamescope "${ARGS[@]}" --prefer-output HDMI-A-1 -W 3840 -H 2160 -r 60 -o 60 ;;
  4k30)      exec /usr/bin/gamescope "${ARGS[@]}" --prefer-output HDMI-A-1 -W 3840 -H 2160 -r 30 -o 30 ;;
  2k60)      exec /usr/bin/gamescope "${ARGS[@]}" --prefer-output HDMI-A-1 -W 2560 -H 1440 -r 60 -o 60 ;;
  1080p120)  exec /usr/bin/gamescope "${ARGS[@]}" --prefer-output HDMI-A-1 -W 1920 -H 1080 -r 120 -o 120 ;;
  1080p60)   exec /usr/bin/gamescope "${ARGS[@]}" --prefer-output HDMI-A-1 -W 1920 -H 1080 -r 60 -o 60 ;;
  default|*) exec /usr/bin/gamescope "${ARGS[@]}" --prefer-output HDMI-A-1 -W 1920 -H 1080 -r 60 -o 60 ;;
esac
