#!/bin/bash
# deploy-nexus-config.sh — Deploy canonical Nexus configs from repo to all 4 profiles.
#
# Usage:
#   ./scripts/deploy-nexus-config.sh                  # default: InputBinds.json
#   ./scripts/deploy-nexus-config.sh --inputbinds
#   ./scripts/deploy-nexus-config.sh --addonconfig
#   ./scripts/deploy-nexus-config.sh --gamebinds
#   ./scripts/deploy-nexus-config.sh --all
#
# Refuses to deploy if GW2 is running anywhere. Backs up each target file with
# a timestamped suffix before overwriting. Verifies SHA256 hash matches after
# every deploy.
#
# Repo source-of-truth files live at: configs/gw2-keybinds/

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE_DIR="$REPO_ROOT/configs/gw2-keybinds"

[ -f "$SOURCE_DIR/nexus-inputbinds.json" ] || {
    echo "ERROR: missing canonical source $SOURCE_DIR/nexus-inputbinds.json" >&2
    exit 1
}

DEPLOY_INPUTBINDS=0
DEPLOY_ADDONCONFIG=0
DEPLOY_GAMEBINDS=0

if [ $# -eq 0 ]; then
    DEPLOY_INPUTBINDS=1
else
    while [ $# -gt 0 ]; do
        case "$1" in
            --inputbinds) DEPLOY_INPUTBINDS=1 ;;
            --addonconfig) DEPLOY_ADDONCONFIG=1 ;;
            --gamebinds) DEPLOY_GAMEBINDS=1 ;;
            --all) DEPLOY_INPUTBINDS=1; DEPLOY_ADDONCONFIG=1; DEPLOY_GAMEBINDS=1 ;;
            -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
            *) echo "unknown flag: $1" >&2; exit 1 ;;
        esac
        shift
    done
fi

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
if [ "${#UNREACHABLE[@]}" -gt 0 ]; then
    echo "  WARN: skipping unreachable host(s): ${UNREACHABLE[*]}"
fi
if [ "${#REACHABLE[@]}" -eq 0 ]; then
    echo "ERROR: no reachable targets" >&2
    exit 3
fi
echo "  OK — ${#REACHABLE[@]} reachable target(s)"
echo

TS=$(date +%Y%m%d-%H%M%S)
FAILED=0

deploy_one() {
    local source_file="$1"
    local remote_filename="$2"
    local target_full="$3"
    local label="$4"

    local host="${target_full%%:*}"
    local addons_dir="${target_full#*:}"
    local remote_path="$addons_dir/Nexus/$remote_filename"

    echo "  [$label] $host:$remote_path"

    if ssh -o ConnectTimeout=8 "$host" "test -f '$remote_path'" 2>/dev/null; then
        ssh -o ConnectTimeout=8 "$host" "cp -p '$remote_path' '$remote_path.bak-$TS'"
    fi

    scp -q "$source_file" "$host:$remote_path"

    local local_hash remote_hash
    local_hash=$(shasum -a 256 "$source_file" | cut -c1-16)
    remote_hash=$(ssh -o ConnectTimeout=4 "$host" "sha256sum '$remote_path' | cut -c1-16")
    if [ "$local_hash" = "$remote_hash" ]; then
        echo "    OK hash=$local_hash"
    else
        echo "    FAIL hash mismatch: local=$local_hash remote=$remote_hash" >&2
        FAILED=$((FAILED + 1))
    fi
}

run_deploy() {
    local src_basename="$1"
    local target_filename="$2"
    local src_path="$SOURCE_DIR/$src_basename"
    if [ ! -f "$src_path" ]; then
        echo "Skipping $target_filename ($src_path not found in repo)"
        return
    fi
    echo "Deploying $src_basename -> $target_filename on all reachable profiles..."
    for entry in "${REACHABLE[@]}"; do
        deploy_one "$src_path" "$target_filename" "${entry%%|*}" "${entry#*|}"
    done
    echo
}

[ "$DEPLOY_INPUTBINDS" = 1 ] && run_deploy "nexus-inputbinds.json" "InputBinds.json"
[ "$DEPLOY_ADDONCONFIG" = 1 ] && run_deploy "nexus-addonconfig.json" "AddonConfig.json"
[ "$DEPLOY_GAMEBINDS" = 1 ] && run_deploy "gamebinds.xml" "GameBinds.xml"

if [ "$FAILED" -gt 0 ]; then
    echo "Deploy finished with $FAILED FAILURE(S)" >&2
    exit 3
fi

echo "Deploy complete. Reload Nexus by relaunching GW2 on each profile to pick up the new config."
if [ "${#UNREACHABLE[@]}" -gt 0 ]; then
    echo "WARN: ${#UNREACHABLE[@]} host(s) skipped because unreachable: ${UNREACHABLE[*]}"
    echo "      Re-run this script when those host(s) are awake to finish deployment."
fi
