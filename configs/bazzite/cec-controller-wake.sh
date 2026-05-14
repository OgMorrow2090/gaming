#!/bin/bash
# Watch for Steam Controller hidraw data and trigger CEC wake + input switch.
# Steam grabs evdev exclusively but leaves hidraw readable.
# The Puck hidraw produces data only when the controller is connected and active.

HIDRAW="/dev/hidraw0"
COOLDOWN=30

while true; do
    dd if="$HIDRAW" of=/dev/null bs=64 count=1 status=none 2>/dev/null
    cec-ctl -d /dev/cec0 --playback --image-view-on -t 0 -s 2>/dev/null
    sleep 0.5
    cec-ctl -d /dev/cec0 --playback --active-source phys-addr=1.0.0.0 -s 2>/dev/null
    logger -t cec-controller-wake "Steam Controller connected — sent CEC wake + active source"
    timeout "$COOLDOWN" dd if="$HIDRAW" of=/dev/null bs=4096 status=none 2>/dev/null || true
done
