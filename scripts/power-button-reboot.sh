#!/bin/bash
# Wait for gamescope to start, then grab the power button exclusively
sleep 10
evtest --grab /dev/input/event2 2>/dev/null | while read line; do
    if echo "$line" | grep -q "KEY_POWER.*value 1"; then
        logger "Power button pressed - triggering reboot"
        systemctl reboot
        exit 0
    fi
done
