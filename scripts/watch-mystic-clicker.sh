#!/bin/bash
# watch-mystic-clicker.sh — Mac-side watcher for new mystic-clicker.dll CI builds.
#
# Polls GitHub Actions for newer successful builds than what's on bazzite, and
# auto-deploys via deploy-mystic-clicker-dll.sh when GW2 isn't running. Designed
# to be run from a launchd agent every ~5 minutes.
#
# Why a Mac watcher and not a GitHub Actions deploy step:
#   GitHub Actions cloud runners can't reach 172.16.100.x private LAN IPs.
#   The Mac IS on the LAN, so we close the gap here.
#
# State file: ~/.cache/itinyk/gw2-mystic-clicker-last-sha
# Log:       /tmp/gw2-mystic-clicker-watcher.log (appends, rotated by launchd)
# Repo:      auto-detected from this script's path

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GH_REPO="OgMorrow2090/guildwars2"
WORKFLOW="build.yml"
STATE_DIR="$HOME/.cache/itinyk"
STATE_FILE="$STATE_DIR/gw2-mystic-clicker-last-sha"
LOG_FILE="/tmp/gw2-mystic-clicker-watcher.log"

mkdir -p "$STATE_DIR"

log() { echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$LOG_FILE"; }

# Need gh CLI (with auth) and ssh keys configured for Og@172.16.100.212 + deck@172.16.100.95.
command -v gh >/dev/null || { log "gh not in PATH"; exit 0; }

# Look up latest successful build's commit SHA.
LATEST_SHA=$(gh run list --repo "$GH_REPO" --workflow="$WORKFLOW" --branch=main --status=success --limit=1 --json headSha -q '.[0].headSha' 2>/dev/null || true)
[ -n "$LATEST_SHA" ] || { log "no successful build available"; exit 0; }

LAST_SHA=$(cat "$STATE_FILE" 2>/dev/null || echo "")

if [ "$LATEST_SHA" = "$LAST_SHA" ]; then
    # Already deployed this build — nothing to do.
    exit 0
fi

log "new build detected: $LATEST_SHA (was: ${LAST_SHA:-none})"

# Reachability + GW2 state on bazzite. The deploy script also pre-flights, but
# we short-circuit here to avoid noisy logs when nothing can happen.
if ! ssh -o ConnectTimeout=4 -o BatchMode=yes Og@172.16.100.212 'true' 2>/dev/null; then
    log "bazzite unreachable, skipping"
    exit 0
fi
if ssh -o ConnectTimeout=4 -o BatchMode=yes Og@172.16.100.212 'pgrep -x Gw2-64.exe >/dev/null 2>&1' 2>/dev/null; then
    log "GW2 running on bazzite, skipping (will retry next tick)"
    exit 0
fi

log "deploying $LATEST_SHA..."
if "$REPO_ROOT/scripts/deploy-mystic-clicker-dll.sh" >> "$LOG_FILE" 2>&1; then
    echo "$LATEST_SHA" > "$STATE_FILE"
    log "deploy OK: $LATEST_SHA"
else
    log "deploy FAILED for $LATEST_SHA — will retry next tick"
fi
