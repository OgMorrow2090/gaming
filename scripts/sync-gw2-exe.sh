#!/bin/bash
# sync-gw2-exe: Re-copy Gw2-64.exe from the Steam-managed GW2 install to the
# Apple TV profile install if Steam patched it.
#
# Background: the Apple TV profile lives at ~/Games/gw2-appletv/ with Gw2.dat
# and bin64/ symlinked to the Steam install (so content patches flow through
# automatically), but Gw2-64.exe is a real copy. When ArenaNet patches the
# launcher exe via Steam, the Apple TV profile would otherwise run the old
# binary. This script re-copies the exe when it's been patched.
#
# Idempotent — safe to run anytime. No-op if the Apple TV launcher is current.

set -euo pipefail
SRC="$HOME/.local/share/Steam/steamapps/common/Guild Wars 2/Gw2-64.exe"
DST="$HOME/Games/gw2-appletv/Gw2-64.exe"

if [ ! -f "$SRC" ]; then
    echo "ERROR: Steam GW2 launcher not found at $SRC"
    exit 1
fi

if [ ! -f "$DST" ]; then
    echo "Apple TV profile install not present (Gw2-64.exe missing at $DST) — skipping"
    exit 0
fi

if [ "$SRC" -nt "$DST" ]; then
    echo "Steam GW2 launcher is newer — re-copying to Apple TV install"
    cp -f "$SRC" "$DST"
    echo "  done: $(stat -c %y "$DST")"
else
    echo "Apple TV launcher is current — no copy needed"
fi
