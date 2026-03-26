#!/usr/bin/env bash
# bazzite-auto-update.sh — Automated OS + Flatpak updates with alert reporting
# Runs via systemd timer at 1am daily on Shaun-Bazzite
# Updates: rpm-ostree (OS/kernel/NVIDIA drivers), flatpak apps
# Reports results to alerts.itinyk.app, reboots if updates applied

set -euo pipefail

ALERT_URL="http://172.16.2.2:8000/api/alerts/ingest"
ALERT_SECRET_FILE="/var/home/Og/.config/bazzite-update/alert-secret"
DEVICE="shaun-bazzite"
LOG_FILE="/var/home/Og/.local/state/bazzite-update.log"
SUDO_PASS_FILE="/var/home/Og/.config/bazzite-update/sudo-pass"

mkdir -p "$(dirname "$LOG_FILE")"
exec > >(tee -a "$LOG_FILE") 2>&1
echo "=== Update started: $(date -Iseconds) ==="

# Read secrets
if [[ ! -f "$ALERT_SECRET_FILE" ]]; then
    echo "ERROR: Alert secret file not found at $ALERT_SECRET_FILE"
    exit 1
fi
ALERT_SECRET=$(cat "$ALERT_SECRET_FILE")

if [[ ! -f "$SUDO_PASS_FILE" ]]; then
    echo "ERROR: Sudo password file not found at $SUDO_PASS_FILE"
    exit 1
fi

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

sudo_cmd() {
    cat "$SUDO_PASS_FILE" | sudo -S "$@" 2>/dev/null
}

updates_applied=false
reboot_needed=false
summary=""

# --- rpm-ostree (OS, kernel, NVIDIA drivers) ---
echo "[rpm-ostree] Checking for updates..."
ostree_output=$(sudo_cmd rpm-ostree upgrade 2>&1) || true
echo "$ostree_output"

if echo "$ostree_output" | grep -q "No upgrade available"; then
    summary+="OS: No updates available\n"
elif echo "$ostree_output" | grep -q "Staging deployment"; then
    # Extract version info
    new_version=$(echo "$ostree_output" | grep -oP 'Version: \K[^\s]+' | tail -1 || echo "unknown")
    # Get list of changed packages
    changed_pkgs=$(echo "$ostree_output" | grep -E '^\s*(Upgraded|Added|Removed|Downgraded):' || echo "")
    pkg_summary=$(echo "$ostree_output" | grep -cE '^\s*(Upgraded|Added|Removed):' || echo "0")

    summary+="OS: Updated to ${new_version} (${pkg_summary} packages changed)\n"
    if [[ -n "$changed_pkgs" ]]; then
        # Truncate to first 10 lines to keep alert concise
        summary+="$(echo "$changed_pkgs" | head -10)\n"
        remaining=$(echo "$changed_pkgs" | wc -l)
        if (( remaining > 10 )); then
            summary+="...and $((remaining - 10)) more\n"
        fi
    fi
    updates_applied=true
    reboot_needed=true
elif echo "$ostree_output" | grep -qi "error\|fatal"; then
    send_alert "warning" "Bazzite OS update failed" "rpm-ostree upgrade failed: $(echo "$ostree_output" | tail -3 | tr '\n' ' ')"
    summary+="OS: Update FAILED\n"
else
    summary+="OS: Already up to date\n"
fi

# --- Flatpak updates ---
echo "[flatpak] Checking for updates..."
flatpak_updates=$(flatpak remote-ls --updates --columns=name,application,version 2>/dev/null || true)

if [[ -n "$flatpak_updates" ]]; then
    echo "[flatpak] Updates available:"
    echo "$flatpak_updates"

    flatpak_output=$(sudo_cmd flatpak update -y --noninteractive 2>&1) || true
    echo "$flatpak_output"

    # Build list of updated apps
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
    send_alert "info" "Bazzite updates applied" "$alert_message"

    if $reboot_needed; then
        echo "[reboot] OS update staged — rebooting in 30 seconds..."
        sleep 30
        sudo_cmd systemctl reboot || true
    fi
else
    echo "No updates to apply — skipping alert."
fi

echo "=== Update finished: $(date -Iseconds) ==="
