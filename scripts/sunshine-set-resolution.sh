#!/bin/bash
# sunshine-set-resolution: Sunshine prep-cmd that picks a gamescope mode based
# on the requesting Moonlight client's width.
#
# Used as Sunshine prep-cmd for the Guild Wars 2 app:
#   "prep-cmd": [{"do": "/var/home/Og/scripts/sunshine-set-resolution.sh"}]
#
# Mapping:
#   client width <= 1280  → deck     (1280x800@90, Steam Deck native)
#   client width <= 2560  → 2k120    (2560x1440@120, iPad / 2K Apple TV)
#   client width >= 3840  → 4k120    (3840x2160@120, 4K Apple TV)
#   anything else         → 4k120    (default)
#
# If the current mode already matches, the script exits immediately — no
# gamescope restart, no extra latency. If a switch IS needed, gamescope-session
# is restarted (~5–8s) and Steam comes back up before the prep-cmd returns.
#
# Sunshine sets these env vars when calling prep-cmd:
#   SUNSHINE_CLIENT_WIDTH, SUNSHINE_CLIENT_HEIGHT, SUNSHINE_CLIENT_FPS,
#   SUNSHINE_CLIENT_HDR, SUNSHINE_APP_ID, SUNSHINE_APP_NAME, SUNSHINE_CLIENT_NAME

set -e

LOG="/var/home/Og/.local/state/sunshine-set-resolution.log"
mkdir -p "$(dirname "$LOG")"
exec > >(tee -a "$LOG") 2>&1
echo "=== $(date -Iseconds) === client=${SUNSHINE_CLIENT_NAME:-?} ${SUNSHINE_CLIENT_WIDTH:-?}x${SUNSHINE_CLIENT_HEIGHT:-?}@${SUNSHINE_CLIENT_FPS:-?} app=${SUNSHINE_APP_NAME:-?}"

WIDTH="${SUNSHINE_CLIENT_WIDTH:-3840}"

case "$WIDTH" in
    1280|''|0) MODE=deck  ;;
    1920)      MODE=1080p144 ;;
    2560)      MODE=2k120 ;;
    3840)      MODE=4k120 ;;
    *)
        if   [ "$WIDTH" -le 1280 ]; then MODE=deck
        elif [ "$WIDTH" -le 1920 ]; then MODE=1080p144
        elif [ "$WIDTH" -le 2560 ]; then MODE=2k120
        else                             MODE=4k120
        fi
        ;;
esac

CURRENT="$(cat "$HOME/.config/gamescope-mode" 2>/dev/null | tr -d '[:space:]' || true)"
echo "  width=$WIDTH → mode=$MODE  (current=${CURRENT:-default})"

# Always make sure Steam is responsive before returning — even if mode already
# matches, Sunshine may have triggered prep-cmd in the middle of a previous
# gamescope restart cycle and Steam might not be ready yet.
wait_for_steam() {
    local desc="$1"
    echo "  waiting for Steam to be ready ($desc)..."
    local i
    for i in $(seq 1 45); do
        if pgrep -x steamwebhelper > /dev/null 2>&1; then
            # steamwebhelper running — give it grace for URL handler registration
            sleep 5
            if pgrep -x steamwebhelper > /dev/null 2>&1; then
                echo "  Steam ready after ${i}s poll + 5s grace"
                return 0
            fi
        fi
        sleep 1
    done
    echo "  WARN: Steam not detected ready after 45s — proceeding anyway"
    return 1
}

if [ "$CURRENT" = "$MODE" ]; then
    echo "  already on $MODE — no restart needed"
    wait_for_steam "no-restart"
    exit 0
fi

echo "  switching: $CURRENT → $MODE"
/var/home/Og/bin/gamescope-mode "$MODE"
wait_for_steam "post-restart"
echo "  done"
