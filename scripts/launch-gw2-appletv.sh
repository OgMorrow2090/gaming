#!/bin/bash
# launch-gw2-appletv: non-Steam GW2 Apple TV profile (compatdata appid 2879321470)
# Used as Sunshine cmd. Bypasses Steam non-Steam-shortcut compat-tool-override
# bug by invoking Proton directly with the right per-profile prefix.

set -u

LOG="/var/home/Og/.local/state/launch-gw2-appletv.log"
mkdir -p "$(dirname "$LOG")"
exec > >(tee -a "$LOG") 2>&1
echo "=== $(date -Iseconds) === GW2 Apple TV launch starting"

PROTON="$HOME/.steam/steam/steamapps/common/Proton 11.0/proton"
APPID="2879321470"
GAME_DIR="$HOME/Games/gw2-appletv"
GAME_EXE="$GAME_DIR/Gw2-64.exe"

if [ ! -x "$PROTON" ]; then
    echo "  ERROR: Proton not found at $PROTON"
    exit 1
fi
if [ ! -f "$GAME_EXE" ]; then
    echo "  ERROR: Gw2-64.exe not found at $GAME_EXE"
    exit 1
fi

export STEAM_COMPAT_DATA_PATH="$HOME/.local/share/Steam/steamapps/compatdata/$APPID"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
export WINEDLLOVERRIDES="d3d11=n,b"
export DXVK_ASYNC=1
export PROTON_NO_ESYNC=1
export PROTON_USE_FSYNC=1
mkdir -p "$STEAM_COMPAT_DATA_PATH"

cd "$GAME_DIR"
echo "  Invoking Proton -> Gw2-64.exe -provider Portal (prefix: $APPID)"
"$PROTON" run "$GAME_EXE" -provider Portal
echo "=== $(date -Iseconds) === GW2 Apple TV exited"
