#!/usr/bin/env bash
# setup-gw2-autologin.sh — bazzite GW2 auto-login + optional auto-launch.
#
# Idempotent installer. Self-contained: embeds the systemd user unit and the
# XDG autostart .desktop inline, so deploying to bazzite is just an scp of
# this single file into ~/scripts/.
#
# What it does:
#   1. Prints the Steam launch options you must paste into GW2 → Properties →
#      Launch Options. (Set once; Steam persists in localconfig.vdf forever.)
#   2. Installs ~/.config/systemd/user/gw2-autolaunch.service so gamescope
#      Game-Mode boots straight into GW2 (Steam URI fired after the gamescope
#      session is up).  Disable with --no-autolaunch.
#   3. Drops ~/.config/autostart/gw2-autolaunch.desktop for Desktop-Mode users
#      (KDE Plasma boot). Safe alongside the service — Steam de-dups URIs.
#
# Usage:
#   bash scripts/setup-gw2-autologin.sh                # install both triggers
#   bash scripts/setup-gw2-autologin.sh --no-autolaunch # just print the args
#   bash scripts/setup-gw2-autologin.sh --uninstall    # remove triggers
#
# Source copies of the embedded files live in configs/bazzite/ in the repo —
# edit BOTH places when changing them.

set -euo pipefail

DST_SERVICE="$HOME/.config/systemd/user/gw2-autolaunch.service"
DST_DESKTOP="$HOME/.config/autostart/gw2-autolaunch.desktop"

LAUNCH_OPTS='-autologin -nopatchui -maploadinfo'

mode="install"
want_autolaunch=1
for arg in "$@"; do
    case "$arg" in
        --uninstall)     mode="uninstall" ;;
        --no-autolaunch) want_autolaunch=0 ;;
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
echo "First launch only: tick \"Remember email\" AND enter your password."
echo "GW2 stores both in Local.dat; every subsequent -autologin boot is silent."
echo

if [ "$want_autolaunch" -eq 1 ]; then
    mkdir -p "$(dirname "$DST_SERVICE")" "$(dirname "$DST_DESKTOP")"

    cat > "$DST_SERVICE" <<'UNIT'
[Unit]
Description=Auto-launch Guild Wars 2 once gamescope-session is ready
After=gamescope-session-plus@steam.service
Requires=gamescope-session-plus@steam.service

[Service]
Type=oneshot
RemainAfterExit=yes
# Wait for the Steam process to be alive before firing the steam:// URI —
# otherwise the URI gets dropped and the boot lands in Big Picture.
ExecStartPre=/bin/sh -c 'for i in $(seq 1 60); do pgrep -x steam >/dev/null && break; sleep 1; done; sleep 4'
ExecStart=/usr/bin/steam steam://rungameid/1284210

[Install]
WantedBy=default.target
UNIT

    cat > "$DST_DESKTOP" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Guild Wars 2 (auto-launch)
Comment=Launch GW2 via Steam URI on Desktop-mode session start
Exec=steam -silent steam://rungameid/1284210
Icon=steam
Terminal=false
X-GNOME-Autostart-enabled=true
OnlyShowIn=KDE;GNOME;XFCE;
DESKTOP

    systemctl --user daemon-reload
    systemctl --user enable gw2-autolaunch.service
    echo "==> installed: $DST_SERVICE  (Game-Mode trigger)"
    echo "==> installed: $DST_DESKTOP  (Desktop-Mode trigger)"
    echo
    echo "    Next boot into Game Mode → GW2 launches automatically after Steam."
    echo "    To skip the auto-launch for one boot:"
    echo "       systemctl --user mask gw2-autolaunch.service     # off"
    echo "       systemctl --user unmask gw2-autolaunch.service   # on"
else
    echo "(--no-autolaunch given; not installing the systemd unit / autostart)"
fi
