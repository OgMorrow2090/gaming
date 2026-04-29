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

# Mode can be forced via argument (used by per-resolution Sunshine apps),
# otherwise auto-detect from client width (used by the generic Guild Wars 2 app).
if [ -n "${1:-}" ]; then
    MODE="$1"
    echo "  mode forced via argument: $MODE"
else
    # Apple TV (3840) maps to 2k120 not 4k120: NVIDIA driver caps 4K@120 over
    # the HDMI dongle (TMDS 600 MHz), so 4K never exceeds 60 fps anyway, while
    # 2k120 reliably delivers 90+ fps in GW2 and Moonlight upscales cleanly to
    # the Apple TV's 4K panel.
    WIDTH="${SUNSHINE_CLIENT_WIDTH:-3840}"
    case "$WIDTH" in
        1280|''|0) MODE=deck     ;;
        1920)      MODE=1080p144 ;;
        2560)      MODE=2k120    ;;
        3840)      MODE=2k120    ;;
        *)
            if   [ "$WIDTH" -le 1280 ]; then MODE=deck
            elif [ "$WIDTH" -le 1920 ]; then MODE=1080p144
            else                             MODE=2k120
            fi
            ;;
    esac
fi

# Wine DPI (LogPixels) per mode. GW2 reads Wine DPI when dpiScaling=true and
# scales its UI accordingly — this is the gamescope-2K-canvas compensation.
# We do NOT touch GFXSettings.xml here; the user manages GW2 settings manually.
case "$MODE" in
    deck)     DPI=96  ;;  # Steam Deck native, 100% UI
    1080p144) DPI=96  ;;  # 1080p, 100% UI
    2k120)    DPI=144 ;;  # 1440p, 150% UI
    4k120)    DPI=192 ;;  # 4K, 200% UI
    *)        DPI=96  ;;
esac

# Update Wine LogPixels in GW2's Proton prefix so GW2 picks up the right DPI
# on next launch. Pick whichever Proton version the user currently has installed.
update_wine_dpi() {
    local target="$1"
    local prefix="/var/home/Og/.local/share/Steam/steamapps/compatdata/1284210/pfx"
    local user_reg="$prefix/user.reg"
    local target_hex
    target_hex=$(printf "dword:%08x" "$target")

    local current_hex
    current_hex=$(grep -A30 "^\[Control Panel..Desktop\]" "$user_reg" 2>/dev/null | grep -oE "\"LogPixels\"=dword:[0-9a-f]+" | head -1 | cut -d= -f2)

    if [ "$current_hex" = "$target_hex" ]; then
        echo "  Wine LogPixels already $target ($target_hex) — skip"
        return 0
    fi

    echo "  Wine LogPixels $current_hex → $target_hex (DPI=$target)"
    # In-place edit user.reg — only updates the entry inside [Control Panel\\Desktop].
    # Match LogPixels lines and replace with target value (matches both occurrences;
    # safer than scoping to one section since both should agree anyway).
    sed -i -E "s/\"LogPixels\"=dword:[0-9a-f]+/\"LogPixels\"=$target_hex/g" "$user_reg"
}

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

# Update Wine DPI only — Wine reads user.reg fresh on each Gw2-64.exe launch.
# GFXSettings.xml is left alone (user manages GW2 settings manually).
update_wine_dpi "$DPI"

if [ "$CURRENT" = "$MODE" ]; then
    echo "  already on $MODE — no restart needed"
    wait_for_steam "no-restart"
    exit 0
fi

echo "  switching: $CURRENT → $MODE"
/var/home/Og/bin/gamescope-mode "$MODE"
wait_for_steam "post-restart"

# After gamescope-session restart, Sunshine's wayland display connection is
# stale — encoder pipeline falls back to vulkan/vaapi/software (all of which
# require XDG portal that gamescope doesn't expose) instead of nvenc.
# Schedule a Sunshine restart in the background so we don't kill ourselves.
# Current Moonlight connection will likely fail; client retries and gets a
# fresh Sunshine.
echo "  scheduling Sunshine restart (5s delay) to refresh display capture..."
(sleep 5 && systemctl --user restart sunshine-gaming.service) &
disown
echo "  done"
