#!/bin/bash
# deploy-mystic-clicker-dll.sh — Pull latest mystic-clicker.dll from GitHub
# Actions and deploy to all 4 GW2 profiles.
#
# Usage:
#   ./scripts/deploy-mystic-clicker-dll.sh                   # latest successful build on main
#   ./scripts/deploy-mystic-clicker-dll.sh <run-id>          # specific GitHub Actions run id
#   ./scripts/deploy-mystic-clicker-dll.sh --local <path>    # use a locally-built DLL (skip download)
#
# Refuses if GW2 is running anywhere reachable. Backs up each existing target
# DLL with timestamp before overwrite. SHA256-verified per target.
#
# Same pre-flight + reachable-host pattern as deploy-nexus-config.sh and
# deploy-controller-vdf.sh.

set -euo pipefail

REPO="OgMorrow2090/guildwars2"
WORKFLOW="build.yml"
ARTIFACT_NAME="mystic-clicker"

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
    cp "$LOCAL_DLL" "$WORKDIR/mystic-clicker.dll"
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

[ -f "$WORKDIR/mystic-clicker.dll" ] || { echo "ERROR: mystic-clicker.dll not in artifact" >&2; exit 1; }

LOCAL_HASH=$(shasum -a 256 "$WORKDIR/mystic-clicker.dll" | cut -c1-16)
LOCAL_SIZE=$(wc -c < "$WORKDIR/mystic-clicker.dll" | tr -d ' ')
echo "  DLL hash=$LOCAL_HASH size=$LOCAL_SIZE bytes"
echo

# Format: ssh_host:addons_dir|profile_label
# Multi-install profiles (apple-tv, deck-bazzite) were consolidated to a single
# main install on 2026-05-12 — see memory/multi-gw2-installs.md (ARCHIVED). If
# those profiles ever return, re-add the corresponding entries here.
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

echo "Deploying mystic-clicker.dll on all reachable profiles..."
for entry in "${REACHABLE[@]}"; do
    target="${entry%%|*}"
    label="${entry#*|}"
    host="${target%%:*}"
    addons_dir="${target#*:}"
    remote_path="$addons_dir/mystic-clicker.dll"

    echo "  [$label] $host:$remote_path"

    if ssh -o ConnectTimeout=8 "$host" "test -f '$remote_path'" 2>/dev/null; then
        ssh -o ConnectTimeout=8 "$host" "cp -p '$remote_path' '$remote_path.bak-$TS'"
    fi

    scp -q "$WORKDIR/mystic-clicker.dll" "$host:$remote_path"

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
