#!/bin/bash
# sunshine-wait-gw2: Sunshine cmd that lands the user in Big Picture and
# ends the stream when they exit Guild Wars 2.
#
# Used as Sunshine's `cmd` for the "GW2 - SteamOS" app. Unlike
# sunshine-launch-gw2.sh, this script does NOT fire a steam:// URL — it
# just waits. The user lands in Big Picture (where they can change Steam
# display settings, change res, browse other apps) and launches GW2
# manually. Once GW2 starts and later exits, this script returns and
# Sunshine ends the stream.
#
# The "GW2 - SteamOS" app exists so Steam Input can map that Sunshine app
# name to the GW2 controller layout via configset_controller_neptune.vdf,
# while still giving the user the Big Picture middle-step.
#
# Behavior:
#   1. Wait up to GRACE_MIN minutes for Gw2-64.exe to appear
#   2. Once it appears, poll until it exits
#   3. Return -> Sunshine ends stream
#   4. If GW2 never appears within GRACE_MIN, return early so Sunshine
#      doesn't hang the slot indefinitely
#
# Tweak GRACE_MIN if you want to spend more time browsing Big Picture
# before launching GW2.

set -u

GRACE_MIN=30

LOG="/var/home/Og/.local/state/sunshine-wait-gw2.log"
mkdir -p "$(dirname "$LOG")"
exec > >(tee -a "$LOG") 2>&1
echo "=== $(date -Iseconds) === GW2 wait wrapper starting (grace=${GRACE_MIN}m)"

echo "  waiting for user to launch Gw2-64.exe from Big Picture..."
for i in $(seq 1 $((GRACE_MIN * 60 / 2))); do
    if pgrep -f Gw2-64.exe > /dev/null 2>&1; then
        echo "  Gw2-64.exe detected after $((i * 2))s"
        break
    fi
    sleep 2
done

if ! pgrep -f Gw2-64.exe > /dev/null 2>&1; then
    echo "  WARN: GW2 never launched within ${GRACE_MIN}m grace — exiting (sunshine will end stream)"
    exit 0
fi

echo "  GW2 running — waiting for it to exit..."
while pgrep -f Gw2-64.exe > /dev/null 2>&1; do
    sleep 3
done

echo "=== $(date -Iseconds) === GW2 exited — sunshine will end stream"
