#!/usr/bin/env bash
# steamdeck-auto-update.sh — Automated OS + Flatpak updates with alert reporting
# Runs via systemd timer on Steam Deck
# Updates: SteamOS (via atomupd-manager), Flatpak apps
# Reports results to alerts.itinyk.app, reboots if OS update applied
# Timer uses Persistent=true so missed runs (sleep/power off) execute on next wake

set -euo pipefail

ALERT_URL="http://172.16.2.2:8000/api/alerts/ingest"
ALERT_SECRET_FILE="/home/deck/.config/steamdeck-update/alert-secret"
DEVICE="steam-deck"
LOG_FILE="/home/deck/.local/state/steamdeck-update.log"

mkdir -p "$(dirname "$LOG_FILE")"
exec > >(tee -a "$LOG_FILE") 2>&1
echo "=== Update started: $(date -Iseconds) ==="

# Read secrets
if [[ ! -f "$ALERT_SECRET_FILE" ]]; then
    echo "ERROR: Alert secret file not found at $ALERT_SECRET_FILE"
    exit 1
fi
ALERT_SECRET=$(cat "$ALERT_SECRET_FILE")

send_alert() {
    local severity="$1" title="$2" message="$3"
    local payload
    payload=$(jq -n \
        --arg secret "$ALERT_SECRET" \
        --arg device "$DEVICE" \
        --arg severity "$severity" \
        --arg title "$title" \
        --arg message "$message" \
        '{secret: $secret, device: $device, severity: $severity, title: $title, message: $message, alert_type: "system-update"}')
    local result
    result=$(curl -s -w "\n%{http_code}" -X POST "$ALERT_URL" \
        -H "Content-Type: application/json" \
        -d "$payload" 2>&1)
    local http_code
    http_code=$(echo "$result" | tail -1)
    echo "[alert] HTTP $http_code — $title"
}

updates_applied=false
reboot_needed=false
summary=""

# --- SteamOS update (OS, kernel, drivers) ---
echo "[steamos] Checking for updates..."
check_output=$(sudo steamos-update check 2>&1) && check_rc=$? || check_rc=$?

if [[ $check_rc -eq 7 ]]; then
    summary+="OS: No update available\n"
elif [[ $check_rc -eq 0 ]]; then
    buildid="$check_output"
    echo "[steamos] Update available: build $buildid — applying..."
    update_output=$(sudo steamos-update 2>&1) && update_rc=$? || update_rc=$?
    echo "$update_output"

    if [[ $update_rc -eq 0 ]]; then
        summary+="OS: Updated to build ${buildid}\n"
        updates_applied=true
        reboot_needed=true
    else
        send_alert "warning" "Steam Deck OS update failed" "steamos-update failed (exit $update_rc): $(echo "$update_output" | tail -3 | tr '\n' ' ')"
        summary+="OS: Update FAILED\n"
    fi
elif [[ $check_rc -eq 8 ]]; then
    summary+="OS: Update already applied, pending reboot\n"
    reboot_needed=true
else
    send_alert "warning" "Steam Deck OS update check failed" "steamos-update check failed (exit $check_rc): $(echo "$check_output" | tail -3 | tr '\n' ' ')"
    summary+="OS: Update check FAILED\n"
fi

# --- Flatpak updates ---
echo "[flatpak] Checking for updates..."
flatpak_updates=$(flatpak remote-ls --updates --columns=name,application,version 2>/dev/null || true)

if [[ -n "$flatpak_updates" ]]; then
    echo "[flatpak] Updates available:"
    echo "$flatpak_updates"

    flatpak_output=$(sudo flatpak update -y --noninteractive 2>&1) || true
    echo "$flatpak_output"

    app_list=$(echo "$flatpak_updates" | awk '{print $1}' | head -10 | tr '\n' ', ' | sed 's/,$//')
    app_count=$(echo "$flatpak_updates" | wc -l | tr -d ' ')
    summary+="Flatpak: ${app_count} apps updated (${app_list})\n"
    remaining_apps=$((app_count - 10))
    if (( remaining_apps > 0 )); then
        summary+="...and ${remaining_apps} more apps\n"
    fi
    updates_applied=true
else
    summary+="Flatpak: No updates available\n"
fi

# --- Send result alert ---
echo ""
echo "=== Summary ==="
echo -e "$summary"

if $updates_applied; then
    alert_message=$(echo -e "$summary" | tr '\n' ' ' | sed 's/  */ /g; s/ *$//')
    send_alert "info" "Steam Deck updates applied" "$alert_message"

    if $reboot_needed; then
        echo "[reboot] OS update applied — rebooting in 30 seconds..."
        sleep 30
        sudo systemctl reboot || true
    fi
else
    echo "No updates to apply — skipping alert."
fi

echo "=== Update finished: $(date -Iseconds) ==="
