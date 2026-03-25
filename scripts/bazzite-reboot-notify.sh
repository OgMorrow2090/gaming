#!/usr/bin/env bash
# bazzite-reboot-notify.sh — Send alert to alerts.itinyk.app on system boot
# Runs via systemd user service (After=network-online.target)

ALERT_URL="http://172.16.2.2:8000/api/alerts/ingest"
ALERT_SECRET_FILE="/var/home/Og/.config/bazzite-update/alert-secret"
DEVICE="shaun-bazzite"

if [[ ! -f "$ALERT_SECRET_FILE" ]]; then
    echo "ERROR: Alert secret file not found"
    exit 1
fi
ALERT_SECRET=$(cat "$ALERT_SECRET_FILE")

# Get uptime and boot reason
boot_time=$(uptime -s 2>/dev/null || date)
kernel=$(uname -r)
os_version=$(grep -oP 'VERSION="\K[^"]+' /etc/os-release 2>/dev/null || echo "unknown")

message="Boot time: ${boot_time} | Kernel: ${kernel} | OS: Bazzite ${os_version}"

# Retry a few times in case network isn't fully ready
for attempt in 1 2 3; do
    result=$(curl -s -w "%{http_code}" -o /dev/null -X POST "$ALERT_URL" \
        -H "Content-Type: application/json" \
        -d "$(printf '{"secret":"%s","device":"%s","severity":"info","title":"Bazzite rebooted","message":"%s","alert_type":"reboot"}' \
            "$ALERT_SECRET" "$DEVICE" "$message")" 2>/dev/null)
    if [[ "$result" == "200" ]]; then
        echo "Reboot alert sent successfully"
        exit 0
    fi
    echo "Attempt $attempt failed (HTTP $result), retrying in 5s..."
    sleep 5
done

echo "Failed to send reboot alert after 3 attempts"
exit 1
