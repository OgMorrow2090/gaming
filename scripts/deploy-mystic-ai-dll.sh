#!/bin/bash
# deploy-mystic-ai-dll.sh — Pull latest mystic-ai.dll from GitHub Actions and
# deploy it to bazzite + Deck-native. Sibling of deploy-mystic-clicker-dll.sh.
#
# Usage:
#   ./scripts/deploy-mystic-ai-dll.sh                   # latest successful build on main
#   ./scripts/deploy-mystic-ai-dll.sh <run-id>          # specific GitHub Actions run id
#   ./scripts/deploy-mystic-ai-dll.sh --local <path>    # use a locally-built DLL
#
# Refuses if GW2 is running anywhere reachable. Backs up each existing target
# DLL with timestamp before overwrite. SHA256-verified per target.
#
# Note: Mystic AI lives in addons/MysticAI/ (subdir), unlike Mystic Clicker
# which sits directly in addons/. The Deck also runs the gw2-claude-daemon
# host-side so AI features work on Deck-native GW2, not just bazzite.

set -euo pipefail

REPO="OgMorrow2090/gaming"
WORKFLOW="build.yml"
ARTIFACT_NAME="mystic-ai"
DLL_NAME="mystic-ai.dll"
# Nexus loads DLLs from addons/ ROOT only — subdirectories are scanned for
# per-addon assets (config files, etc.) via Paths_GetAddonDirectory, NOT for
# DLLs. Every working sibling addon (mystic-clicker.dll, mystic-trading.dll,
# ArcDPS.dll, ...) sits at addons/<name>.dll. We previously deployed to
# addons/MysticAI/mystic-ai.dll and Nexus silently never loaded it — see
# memory/nexus-dll-shadow-load-from-addons-root.md.

LOCAL_DLL=""
RUN_ID=""

while [ $# -gt 0 ]; do
    case "$1" in
        --local) LOCAL_DLL="$2"; shift 2 ;;
        -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
        --*) echo "unknown flag: $1" >&2; exit 1 ;;
        *) RUN_ID="$1"; shift ;;
    esac
done

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

if [ -n "$LOCAL_DLL" ]; then
    [ -f "$LOCAL_DLL" ] || { echo "ERROR: $LOCAL_DLL not found" >&2; exit 1; }
    cp "$LOCAL_DLL" "$WORKDIR/$DLL_NAME"
    echo "Using local DLL: $LOCAL_DLL"
else
    if [ -z "$RUN_ID" ]; then
        echo "Looking up latest successful build of $WORKFLOW on main..."
        RUN_ID=$(gh run list --repo "$REPO" --workflow="$WORKFLOW" --branch=main --status=success --limit=1 --json databaseId -q '.[0].databaseId')
        [ -n "$RUN_ID" ] || { echo "ERROR: no successful build found" >&2; exit 1; }
        echo "  -> run id $RUN_ID"
    fi
    echo "Downloading $ARTIFACT_NAME artifact from run $RUN_ID..."
    (cd "$WORKDIR" && gh run download "$RUN_ID" --repo "$REPO" --name "$ARTIFACT_NAME") \
        || { echo "ERROR: artifact download failed" >&2; exit 1; }
fi

[ -f "$WORKDIR/$DLL_NAME" ] || { echo "ERROR: $DLL_NAME not in artifact" >&2; exit 1; }

LOCAL_HASH=$(shasum -a 256 "$WORKDIR/$DLL_NAME" | cut -c1-16)
LOCAL_SIZE=$(wc -c < "$WORKDIR/$DLL_NAME" | tr -d ' ')
echo "  DLL hash=$LOCAL_HASH size=$LOCAL_SIZE bytes"
echo

# Format: ssh_host:addons_dir|profile_label
TARGETS=(
    "Og@172.16.100.212:/home/Og/.local/share/Steam/steamapps/common/Guild Wars 2/addons|bazzite"
    "deck@172.16.100.95:/home/deck/.local/share/Steam/steamapps/common/Guild Wars 2/addons|deck-native"
)

echo "Pre-flight: reachability + GW2 not running on any reachable target..."
declare -a REACHABLE=()
declare -a UNREACHABLE=()
for entry in "${TARGETS[@]}"; do
    host="${entry%%:*}"
    if ! ssh -o ConnectTimeout=5 -o BatchMode=yes "$host" 'true' 2>/dev/null; then
        UNREACHABLE+=("$host")
        continue
    fi
    if ssh -o ConnectTimeout=4 -o BatchMode=yes "$host" 'pgrep -x Gw2-64.exe >/dev/null 2>&1' 2>/dev/null; then
        echo "ERROR: GW2 is running on $host — close it before deploying" >&2
        exit 2
    fi
    REACHABLE+=("$entry")
done
[ "${#UNREACHABLE[@]}" -eq 0 ] || echo "  WARN: skipping unreachable host(s): ${UNREACHABLE[*]}"
[ "${#REACHABLE[@]}" -gt 0 ] || { echo "ERROR: no reachable targets" >&2; exit 3; }
echo "  OK — ${#REACHABLE[@]} reachable target(s)"
echo

TS=$(date +%Y%m%d-%H%M%S)
FAILED=0

echo "Deploying $DLL_NAME on all reachable profiles..."
for entry in "${REACHABLE[@]}"; do
    target="${entry%%|*}"
    label="${entry#*|}"
    host="${target%%:*}"
    addons_dir="${target#*:}"
    remote_path="$addons_dir/$DLL_NAME"

    echo "  [$label] $host:$remote_path"

    # Backup any existing DLL before overwrite.
    ssh -o ConnectTimeout=8 "$host" "test -f '$remote_path' && cp -p '$remote_path' '$remote_path.bak-$TS' || true"

    scp -q "$WORKDIR/$DLL_NAME" "$host:$remote_path"

    remote_hash=$(ssh -o ConnectTimeout=4 "$host" "sha256sum '$remote_path' | cut -c1-16")
    if [ "$LOCAL_HASH" = "$remote_hash" ]; then
        echo "    OK hash=$LOCAL_HASH"
    else
        echo "    FAIL hash mismatch: local=$LOCAL_HASH remote=$remote_hash" >&2
        FAILED=$((FAILED + 1))
    fi
done

echo
if [ "$FAILED" -gt 0 ]; then
    echo "Deploy finished with $FAILED FAILURE(S)" >&2
    exit 3
fi

echo "Deploy complete. Restart GW2 on each profile to load the new DLL."
[ "${#UNREACHABLE[@]}" -eq 0 ] || {
    echo "WARN: ${#UNREACHABLE[@]} host(s) skipped because unreachable: ${UNREACHABLE[*]}"
    echo "      Re-run this script when those host(s) are awake."
}
