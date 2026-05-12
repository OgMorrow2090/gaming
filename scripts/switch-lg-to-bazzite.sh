#!/bin/bash
# switch-lg-to-bazzite: send HDMI-CEC command to LG TV telling it
# to switch HDMI input to bazzite (this PC). Mirrors what the Apple TV
# remote does when it wakes the Apple TV.
#
# Requires:
#   - HDMI-CEC adapter exposing /dev/cec0 (e.g. UGREEN CAC-1085-class
#     DP-to-HDMI 2.1 active adapter, or Pulse-Eight USB-CEC)
#   - cec-utils (cec-ctl) — already installed via v4l-utils
#   - LG Simplink enabled on the LG (Settings → General → Simplink)
#
# Test by running this from SSH while LG is showing Apple TV — the
# LG should flip to bazzite-s HDMI input.

set -e

LOG="/var/home/Og/.local/state/cec-switch.log"
mkdir -p "$(dirname "$LOG")"
exec > >(tee -a "$LOG") 2>&1
echo "=== $(date -Iseconds) === switch-lg-to-bazzite invoked"

if ! ls /dev/cec0 > /dev/null 2>&1; then
    echo "  ERROR: no /dev/cec0 — CEC adapter not detected"
    echo "  Check: ls /dev/cec*"
    echo "  Likely need UGREEN DP-to-HDMI 2.1 adapter (not direct HDMI on 9070 XT)"
    exit 1
fi

# Optional: print current active source for debugging
echo "  current state:"
cec-ctl --to 0 --give-active-source 2>&1 | grep -i active | head -3

# Make this device the active source — LG receives, switches input to us
echo "  sending image-view-on..."
cec-ctl --to 0 --image-view-on
echo "  sending active-source..."
cec-ctl --to 0 --active-source phys-addr=0.0.0.0

echo "=== $(date -Iseconds) === done — LG should now show bazzite"
