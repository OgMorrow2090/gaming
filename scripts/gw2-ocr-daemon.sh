#!/bin/bash
#
# gw2-ocr-daemon.sh — host-side OCR worker for Mystic Clicker.
#
# Why this exists:
#   GW2 runs inside Steam Linux Runtime's pressure-vessel sandbox under Proton.
#   Wine's `start /unix` is broken in Proton 11.0 ("Could not translate the
#   specified Unix filename to a DOS filename"), so the DLL cannot directly
#   spawn /usr/bin/tesseract from inside the sandbox.
#
#   Workaround: this script runs OUTSIDE the sandbox as a long-lived systemd
#   user unit (gw2-ocr-daemon.service). It watches /tmp for new BMPs that the
#   DLL drops, runs tesseract, and writes the result back to /tmp where the
#   DLL can read it. /tmp is bind-mounted into the sandbox so both sides see
#   the same files.
#
# Protocol:
#   DLL → writes /tmp/gw2-ocr-input-<TS>.bmp (the captured screen region)
#   daemon → runs tesseract on it, writes /tmp/gw2-ocr-output-<TS>.txt
#   daemon → touches /tmp/gw2-ocr-done-<TS> as the "I'm finished" marker
#   DLL → polls for the done marker, reads the .txt, cleans up
#
# Tesseract args are hardcoded here (lang=eng, psm=6). If we ever need
# per-call params, switch to a sidecar request file (.json or k=v) and parse.

set -euo pipefail

LOG="${HOME}/.local/state/gw2-ocr-daemon.log"
mkdir -p "$(dirname "$LOG")"

if ! command -v inotifywait >/dev/null 2>&1; then
    echo "$(date '+%F %T') FATAL: inotifywait not installed (rpm-ostree install inotify-tools)" >> "$LOG"
    exit 1
fi
if ! [ -x /usr/bin/tesseract ]; then
    echo "$(date '+%F %T') FATAL: /usr/bin/tesseract missing (rpm-ostree install tesseract)" >> "$LOG"
    exit 1
fi

echo "$(date '+%F %T') daemon starting (pid=$$)" >> "$LOG"

# Process any BMPs that already exist (e.g. from a request that landed before
# the daemon started up). Without this we'd ignore them until a new one arrives.
shopt -s nullglob
for bmp in /tmp/gw2-ocr-input-*.bmp; do
    suffix="${bmp#/tmp/gw2-ocr-input-}"
    suffix="${suffix%.bmp}"
    [ -f "/tmp/gw2-ocr-done-${suffix}" ] && continue
    /usr/bin/tesseract "$bmp" "/tmp/gw2-ocr-output-${suffix}" -l eng --psm 6 >>"$LOG" 2>&1 || true
    touch "/tmp/gw2-ocr-done-${suffix}"
done
shopt -u nullglob

# Stream events from /tmp. close_write fires when the DLL finishes writing the
# BMP; moved_to handles atomic-rename-into-place writers.
inotifywait -m -q -e close_write -e moved_to /tmp 2>/dev/null | while read -r dir event filename; do
    case "$filename" in
        gw2-ocr-input-*.bmp)
            suffix="${filename#gw2-ocr-input-}"
            suffix="${suffix%.bmp}"
            /usr/bin/tesseract "/tmp/$filename" "/tmp/gw2-ocr-output-${suffix}" -l eng --psm 6 >>"$LOG" 2>&1 || true
            touch "/tmp/gw2-ocr-done-${suffix}"
            ;;
    esac
done
