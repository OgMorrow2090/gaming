#!/usr/bin/env bash
# setup-gw2-autologin.sh — bazzite GW2 auto-login + optional auto-launch.
#
# Idempotent installer that:
#   1. Prints the Steam launch options you must paste into GW2 → Properties →
#      Launch Options.  (Set once; persists in localconfig.vdf forever.)
#   2. Optionally installs ~/.config/systemd/user/gw2-autolaunch.service so
#      gamescope Game-Mode boots straight into GW2 (Steam URI fired after the
#      gamescope session is up).  Disable with --no-autolaunch.
#   3. Drops the matching ~/.config/autostart/gw2-autolaunch.desktop for
#      Desktop-mode users (KDE Plasma boot).  Safe to coexist with the service;
#      Steam de-dups concurrent steam:// URIs.
#
# Re-run any time — overwrites the deployed copies from the repo, never
# touches Steam's own configs.
#
# Usage:
#   bash scripts/setup-gw2-autologin.sh                # install both triggers
#   bash scripts/setup-gw2-autologin.sh --no-autolaunch # just print the args
#   bash scripts/setup-gw2-autologin.sh --uninstall    # remove triggers

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_SERVICE="$REPO_DIR/configs/bazzite/gw2-autolaunch.service"
SRC_DESKTOP="$REPO_DIR/configs/bazzite/gw2-autolaunch.desktop"
SRC_OPTS="$REPO_DIR/configs/bazzite/gw2-launch-options.txt"

DST_SERVICE="$HOME/.config/systemd/user/gw2-autolaunch.service"
DST_DESKTOP="$HOME/.config/autostart/gw2-autolaunch.desktop"

LAUNCH_OPTS='-autologin -nopatchui -maploadinfo'
GW2_APPID=1284210

mode="install"
want_autolaunch=1
for arg in "$@"; do
    case "$arg" in
        --uninstall)        mode="uninstall" ;;
        --no-autolaunch)    want_autolaunch=0 ;;
        -h|--help)
            sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) echo "unknown arg: $arg" >&2; exit 2 ;;
    esac
done

if [ "$mode" = "uninstall" ]; then
    systemctl --user disable --now gw2-autolaunch.service 2>/dev/null || true
    rm -f "$DST_SERVICE" "$DST_DESKTOP"
    systemctl --user daemon-reload
    echo "removed: $DST_SERVICE"
    echo "removed: $DST_DESKTOP"
    echo
    echo "Note: GW2 launch options in Steam UI are NOT cleared automatically."
    echo "Clear them via Steam → Right-click GW2 → Properties → Launch Options."
    exit 0
fi

echo "==> GW2 Steam launch options (paste into Steam UI):"
echo
echo "    $LAUNCH_OPTS"
echo
echo "    Steam → Library → Guild Wars 2 → right-click → Properties → Launch Options"
echo

if [ "$want_autolaunch" -eq 1 ]; then
    mkdir -p "$(dirname "$DST_SERVICE")" "$(dirname "$DST_DESKTOP")"
    install -m 0644 "$SRC_SERVICE" "$DST_SERVICE"
    install -m 0644 "$SRC_DESKTOP" "$DST_DESKTOP"
    systemctl --user daemon-reload
    systemctl --user enable gw2-autolaunch.service
    echo "==> installed: $DST_SERVICE  (Game-Mode trigger)"
    echo "==> installed: $DST_DESKTOP  (Desktop-Mode trigger)"
    echo
    echo "    Next boot into Game Mode → GW2 launches automatically after Steam."
    echo "    To skip the auto-launch for one boot:"
    echo "       systemctl --user mask gw2-autolaunch.service   # off"
    echo "       systemctl --user unmask gw2-autolaunch.service # on"
else
    echo "(--no-autolaunch given; not installing the systemd unit / autostart)"
fi

echo
echo "Full notes: $SRC_OPTS"
