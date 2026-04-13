#!/bin/bash
# gamescope-mode: switch gamescope output mode and restart the session.
# Usage: gamescope-mode <4k120|4k60|deck|1080p144|default>
set -e
MODES=(4k120 4k60 deck 1080p144 default)
if [ $# -ne 1 ] || [[ ! " ${MODES[*]} " =~ " $1 " ]]; then
    echo "Usage: gamescope-mode <mode>" >&2
    echo "  Modes: ${MODES[*]}" >&2
    current=$(cat ~/.config/gamescope-mode 2>/dev/null | tr -d '[:space:]')
    echo "  Current: ${current:-default}" >&2
    exit 1
fi
mkdir -p ~/.config
echo "$1" > ~/.config/gamescope-mode
echo "Switching to mode: $1"
systemctl --user restart gamescope-session-plus@steam.service
echo "Waiting for session..."
sleep 4
systemctl --user is-active gamescope-session-plus@steam.service && echo "gamescope-session: active"
systemctl --user is-active sunshine-gaming.service && echo "sunshine: active"
PID=$(pgrep -f '^/usr/bin/gamescope' | head -1)
if [ -n "$PID" ]; then
    echo "gamescope cmdline:"
    ps -p $PID -o args= | fold -s -w 80
fi
