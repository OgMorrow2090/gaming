#!/bin/bash
# sync-gw2-exe: Re-copy Gw2-64.exe from the Steam-managed GW2 install to the
# non-Steam profile installs (~/Games/gw2-appletv/, ~/Games/gw2-deck/) if Steam
# patched the launcher.
#
# Background: each non-Steam profile lives at ~/Games/gw2-{appletv,deck}/ with
# Gw2.dat, bin64/, and d3d11.dll symlinked to the Steam install (so content +
# Nexus loader patches flow through automatically), but Gw2-64.exe is a real
# copy per profile. When ArenaNet patches the launcher exe via Steam, the
# non-Steam profiles would otherwise run the old binary. This script re-copies
# the exe to each non-Steam profile when it's been patched.
#
# Idempotent — safe to run anytime. No-op if all profile launchers are current.

set -euo pipefail
SRC="$HOME/.local/share/Steam/steamapps/common/Guild Wars 2/Gw2-64.exe"
DSTS=(
    "$HOME/Games/gw2-appletv/Gw2-64.exe"
    "$HOME/Games/gw2-deck/Gw2-64.exe"
)

if [ ! -f "$SRC" ]; then
    echo "ERROR: Steam GW2 launcher not found at $SRC"
    exit 1
fi

for DST in "${DSTS[@]}"; do
    profile=$(dirname "$DST"); profile=${profile##*/}
    if [ ! -f "$DST" ]; then
        echo "[$profile] Profile install not present (Gw2-64.exe missing at $DST) — skipping"
        continue
    fi
    if [ "$SRC" -nt "$DST" ]; then
        echo "[$profile] Steam GW2 launcher is newer — re-copying"
        cp -f "$SRC" "$DST"
        echo "  done: $(stat -c %y "$DST")"
    else
        echo "[$profile] Launcher is current — no copy needed"
    fi
done
