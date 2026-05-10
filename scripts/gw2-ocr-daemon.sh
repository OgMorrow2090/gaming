#!/bin/bash
#
# gw2-ocr-daemon.sh — host-side OCR worker for Mystic Clicker.
#
# Why this exists:
#   GW2 runs inside Steam Linux Runtime's pressure-vessel sandbox under
#   Proton 11.0. Wine's `start /unix` is broken in that release ("Could not
#   translate the specified Unix filename to a DOS filename"), so the DLL
#   cannot directly spawn /usr/bin/tesseract from inside the sandbox.
#
#   This script runs OUTSIDE the sandbox as a long-lived systemd user unit
#   (gw2-ocr-daemon.service). It polls /tmp for new OCR requests dropped by
#   the DLL, runs tesseract, and writes results back to /tmp. /tmp is bind-
#   mounted into the sandbox so both sides see the same files.
#
# Protocol:
#   DLL writes /tmp/gw2-ocr-input-<TS>.bmp        — captured screen region
#   DLL touches /tmp/gw2-ocr-input-<TS>.ready     — atomic "BMP complete" trigger
#   daemon writes /tmp/gw2-ocr-output-<TS>.txt    — tesseract output
#   daemon touches /tmp/gw2-ocr-done-<TS>         — completion marker
#
# Polling at 100ms keeps CPU minimal (<0.1% on idle) and round-trip latency
# at ~150-400ms for a typical 700×500 capture. inotify would be slightly
# faster but requires inotify-tools (not on bazzite by default, would need
# rpm-ostree install + reboot).

set -uo pipefail

LOG="${HOME}/.local/state/gw2-ocr-daemon.log"
mkdir -p "$(dirname "$LOG")"

if ! [ -x /usr/bin/tesseract ]; then
    echo "$(date '+%F %T') FATAL: /usr/bin/tesseract missing (rpm-ostree install tesseract)" >> "$LOG"
    exit 1
fi

echo "$(date '+%F %T') daemon starting (pid=$$)" >> "$LOG"

shopt -s nullglob
while true; do
    # The .ready marker is the DLL's signal that the .bmp is fully written.
    # Without this we could race the DLL's std::ofstream and OCR a partial
    # BMP. We process .ready files (not .bmp directly) for that reason.
    for ready in /tmp/gw2-ocr-input-*.ready; do
        suffix="${ready#/tmp/gw2-ocr-input-}"
        suffix="${suffix%.ready}"
        bmp="/tmp/gw2-ocr-input-${suffix}.bmp"
        donemark="/tmp/gw2-ocr-done-${suffix}"

        if [ ! -f "$bmp" ]; then
            rm -f "$ready"
            continue
        fi
        if [ -f "$donemark" ]; then
            rm -f "$ready"
            continue
        fi

        /usr/bin/tesseract "$bmp" "/tmp/gw2-ocr-output-${suffix}" -l eng --psm 6 >>"$LOG" 2>&1 || true
        touch "$donemark"
        rm -f "$ready"
    done
    sleep 0.1
done
