#!/bin/bash
# sunshine-set-resolution: prep-cmd that picks gamescope mode based on the
# requesting Moonlight client's width. Wired into Sunshine apps as `do` (run
# at stream start). Pair with `undo` = sunshine-set-resolution.sh 4k60 to
# restore LG-couch mode when stream ends.
#
# Post-AMD (2026-05-12) architecture:
#   deck mode  -> gamescope on virtual HDMI-A-2 at 1280x800@90 (native Deck stream)
#                  LG goes dark during stream
#   4k60       -> gamescope on HDMI-A-1 LG at 4K@60 (couch play / 4K stream)
#
# Client width mapping:
#   1280   -> deck    (Steam Deck native, virtual HDMI-A-2)
#   1920   -> 1080p144 (1080p clients)
#   2560   -> 2k120   (1440p clients)
#   3840   -> 4k60    (4K clients incl. Apple TV)
#   *      -> 4k60    (safe default)
#
# Sunshine env vars set when invoking prep-cmd:
#   SUNSHINE_CLIENT_WIDTH, _HEIGHT, _FPS, _HDR, _NAME
#   SUNSHINE_APP_ID, _NAME
#
# Mode can be forced via argument 1 (overrides auto-detect): used by the
# undo hook to force a return to 4k60.

set -u

LOG="/var/home/Og/.local/state/sunshine-set-resolution.log"
mkdir -p "$(dirname "$LOG")"
exec > >(tee -a "$LOG") 2>&1
echo "=== $(date -Iseconds) === client=${SUNSHINE_CLIENT_NAME:-?} ${SUNSHINE_CLIENT_WIDTH:-?}x${SUNSHINE_CLIENT_HEIGHT:-?}@${SUNSHINE_CLIENT_FPS:-?} app=${SUNSHINE_APP_NAME:-?} arg=${1:-}"

if [ -n "${1:-}" ]; then
    MODE="$1"
    echo "  mode forced via arg: $MODE"
else
    WIDTH="${SUNSHINE_CLIENT_WIDTH:-3840}"
    case "$WIDTH" in
        1280|''|0) MODE=deck     ;;
        1920)      MODE=1080p144 ;;
        2560)      MODE=2k120    ;;
        3840)      MODE=4k60     ;;
        *)
            if   [ "$WIDTH" -le 1280 ]; then MODE=deck
            elif [ "$WIDTH" -le 1920 ]; then MODE=1080p144
            elif [ "$WIDTH" -le 2560 ]; then MODE=2k120
            else                             MODE=4k60
            fi
            ;;
    esac
    echo "  width=$WIDTH -> mode=$MODE"
fi

CURRENT="$(cat "$HOME/.config/gamescope-mode" 2>/dev/null | tr -d '[:space:]' || true)"
if [ "$CURRENT" = "$MODE" ]; then
    echo "  already on $MODE — no restart needed"
    exit 0
fi

echo "  switching: $CURRENT -> $MODE"
/var/home/Og/bin/gamescope-mode "$MODE"
echo "=== $(date -Iseconds) === done"
