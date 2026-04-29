#!/bin/bash
# sunshine-launch-gw2: launch GW2 via Steam, then block until Gw2-64.exe exits.
#
# Used as Sunshine's `cmd` for the Guild Wars 2 app. Without this wrapper,
# `steam steam://rungameid/...` exits immediately after firing the URL and
# Sunshine treats the app as detached — so Alt+F4'ing GW2 leaves the stream
# attached to Steam Big Picture instead of disconnecting the client.
#
# This wrapper:
#   1. Fires the steam:// launch URL
#   2. Waits up to 90s for Gw2-64.exe to appear (first-launch shader compile)
#   3. Polls until Gw2-64.exe is gone
#   4. Exits → Sunshine ends the stream session

set -u

LOG="/var/home/Og/.local/state/sunshine-launch-gw2.log"
mkdir -p "$(dirname "$LOG")"
exec > >(tee -a "$LOG") 2>&1
echo "=== $(date -Iseconds) === GW2 launch wrapper starting"

steam steam://rungameid/1284210 &
STEAM_URL_PID=$!

# Wait for Gw2-64.exe to actually start. First launch can take a while while
# DXVK pipeline cache compiles; subsequent launches are <10s.
echo "  waiting for Gw2-64.exe to appear..."
for i in $(seq 1 90); do
    if pgrep -f Gw2-64.exe > /dev/null 2>&1; then
        echo "  Gw2-64.exe started after ${i}s"
        break
    fi
    sleep 1
done

if ! pgrep -f Gw2-64.exe > /dev/null 2>&1; then
    echo "  WARN: Gw2-64.exe never appeared after 90s — exiting (sunshine will end stream)"
    exit 0
fi

# Block until GW2 exits. Polling at 3s — short enough to feel responsive, long
# enough to avoid CPU churn.
echo "  GW2 running — waiting for it to exit..."
while pgrep -f Gw2-64.exe > /dev/null 2>&1; do
    sleep 3
done

echo "=== $(date -Iseconds) === GW2 exited — sunshine will end stream"
