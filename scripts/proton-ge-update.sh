#!/bin/bash
# Proton GE Auto-Updater v1.1
# Checks GitHub for latest GE-Proton release and installs if newer

INSTALL_DIR="/var/home/Og/.local/share/Steam/compatibilitytools.d"
STEAM_CONFIG="/var/home/Og/.local/share/Steam/config/config.vdf"
ALERT_URL="http://172.16.2.2:8000/api/alerts/ingest"
ALERT_SECRET=$(cat /var/home/Og/.config/bazzite-update/alert-secret 2>/dev/null)

log() { echo "[$(date +%H:%M:%S)] $1"; }

alert() {
    local status="$1" message="$2"
    curl -sf -X POST "$ALERT_URL" \
        -H "Content-Type: application/json" \
        -H "X-Alert-Secret: $ALERT_SECRET" \
        -d "{\"device\": \"shaun-bazzite\", \"service\": \"proton-ge-update\", \"status\": \"$status\", \"message\": \"$message\"}" \
        > /dev/null 2>&1
}

# Get latest release tag from GitHub API
LATEST=$(curl -sf "https://api.github.com/repos/GloriousEggroll/proton-ge-custom/releases/latest" | grep -oP '"tag_name":\s*"\K[^"]+')
if [ -z "$LATEST" ]; then
    log "ERROR: Failed to fetch latest release from GitHub"
    alert "error" "Failed to fetch latest Proton GE release from GitHub"
    exit 1
fi

# Check current installed version
CURRENT=$(ls -1 "$INSTALL_DIR" | grep "^GE-Proton" | sort -V | tail -1)
log "Current: ${CURRENT:-none}, Latest: $LATEST"

if [ "$CURRENT" = "$LATEST" ]; then
    log "Already up to date"
    exit 0
fi

# Download and install
log "Downloading $LATEST..."
TARBALL_URL="https://github.com/GloriousEggroll/proton-ge-custom/releases/download/${LATEST}/${LATEST}.tar.gz"
TMPDIR=$(mktemp -d)
if ! curl -sfL "$TARBALL_URL" -o "$TMPDIR/$LATEST.tar.gz"; then
    log "ERROR: Download failed"
    alert "error" "Failed to download $LATEST"
    rm -rf "$TMPDIR"
    exit 1
fi

log "Installing $LATEST..."
if ! tar -xzf "$TMPDIR/$LATEST.tar.gz" -C "$INSTALL_DIR"; then
    log "ERROR: Extraction failed"
    alert "error" "Failed to extract $LATEST"
    rm -rf "$TMPDIR"
    exit 1
fi
rm -rf "$TMPDIR"

# Remove old versions (keep new one only)
for dir in "$INSTALL_DIR"/GE-Proton*; do
    dirname=$(basename "$dir")
    if [ "$dirname" != "$LATEST" ] && [ "$dirname" != "LegacyRuntime" ]; then
        log "Removing old version: $dirname"
        rm -rf "$dir"
    fi
done

# Update Steam config to point to new version
if [ -n "$CURRENT" ] && [ -f "$STEAM_CONFIG" ]; then
    sed -i "s/\"$CURRENT\"/\"$LATEST\"/g" "$STEAM_CONFIG"
    log "Steam config updated: $CURRENT -> $LATEST"
fi

log "Updated to $LATEST"
alert "success" "Proton GE updated: ${CURRENT:-none} -> $LATEST"
exit 0
