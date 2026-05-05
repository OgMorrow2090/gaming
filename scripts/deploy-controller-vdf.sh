#!/bin/bash
# deploy-controller-vdf.sh — Deploy canonical Steam Input controller VDF from
# repo to ALL VDF files in target dirs across both bazzite and Deck native.
#
# Usage:
#   ./scripts/deploy-controller-vdf.sh                  # deploy
#   ./scripts/deploy-controller-vdf.sh --dry-run        # show what would change
#
# Source of truth:
#   configs/steam-controller/moonlight-gw2-og-template.vdf
#
# Why patch every VDF, not just the configset-referenced one:
#   Steam Input may load any historical layout file (og v19.0_0.vdf,
#   og v19.4_0.vdf, etc.) if the user picked it via Big Picture's UI, or the
#   "GW2 - SteamOS" Sunshine flow's autosave file
#   (moonlight - gw2 steamos/controller_neptune.vdf on Deck). Patching only
#   the canonical og v18.4.9_0.vdf leaves the user hitting old behavior via
#   stale variants.
#   See memory/controller-vdf-deploy-checklist.md for full rationale.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="$REPO_ROOT/configs/steam-controller/moonlight-gw2-og-template.vdf"

[ -f "$SOURCE" ] || { echo "ERROR: missing $SOURCE" >&2; exit 1; }

DRY_RUN=0
case "${1:-}" in
    --dry-run) DRY_RUN=1 ;;
    -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
    "") ;;
    *) echo "unknown flag: $1" >&2; exit 1 ;;
esac

TS=$(date +%Y%m%d-%H%M%S)

# Profile subdirs we deploy into. Non-existent dirs on the remote are skipped.
SUBDIRS=(
    "moonlight"
    "guild wars 2 (apple tv)"
    "guild wars 2 (steam deck)"
    "moonlight - gw2 steamos"
    "1284210"
    "2959110740"
)

# Per-machine: ssh_host:Steam Controller Configs base path
declare -a MACHINES=(
    "Og@172.16.100.212|/home/Og/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config"
    "deck@172.16.100.95|/home/deck/.local/share/Steam/steamapps/common/Steam Controller Configs/64793831/config"
)

echo "Pre-flight: reachability + GW2 not running on any reachable target..."
declare -a REACHABLE=()
declare -a UNREACHABLE=()
for entry in "${MACHINES[@]}"; do
    host="${entry%%|*}"
    if ! ssh -o ConnectTimeout=5 -o BatchMode=yes "$host" 'true' 2>/dev/null; then
        UNREACHABLE+=("$host")
        continue
    fi
    if ssh -o ConnectTimeout=4 -o BatchMode=yes "$host" 'pgrep -x Gw2-64.exe >/dev/null' 2>/dev/null; then
        echo "ERROR: GW2 running on $host — close it before deploying" >&2
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

# Stage canonical template on each machine, then run a remote Python script
# that does the actual file-by-file deploy. Python on remote keeps URL patching
# and CRLF/encoding consistent.
for entry in "${REACHABLE[@]}"; do
    host="${entry%%|*}"
    base="${entry#*|}"
    label="${host%%@*}"

    echo "=== $label ($host) ==="

    # Push the canonical template to a known temp path on remote
    scp -q "$SOURCE" "$host:/tmp/canonical-controller.vdf"

    # Build the SUBDIRS array as Python literals
    py_subdirs=$(printf '"%s",' "${SUBDIRS[@]}")

    ssh -o ConnectTimeout=8 "$host" "DRY_RUN=$DRY_RUN TS='$TS' BASE='$base' SUBDIRS_LIST='$py_subdirs' python3 - <<'PYEOF'
import os, re, sys, shutil, hashlib

DRY_RUN = os.environ['DRY_RUN'] == '1'
TS = os.environ['TS']
BASE = os.environ['BASE']
SUBDIRS = [s for s in eval('[' + os.environ['SUBDIRS_LIST'] + ']')]

with open('/tmp/canonical-controller.vdf') as f:
    template = f.read()

URL_RE = re.compile(r'\"url\"\s+\"[^\"]*\"')

def url_for(rel_path, abs_path):
    parent = os.path.dirname(rel_path)
    fname  = os.path.splitext(os.path.basename(rel_path))[0]
    if parent == 'moonlight - gw2 steamos':
        return f'autosave://{abs_path}'
    return f'usercloud://{parent}/{fname}'

def patched_for(rel_path, abs_path):
    return URL_RE.sub(f'\"url\"\\t\\t\"{url_for(rel_path, abs_path)}\"', template, count=1)

# Which filenames count as 'active' Steam Input layouts.
# Skip:
#   - .bak-* (our own backups)
#   - 'guild wars 2 - og_<N>.vdf' (pre-2025 legacy exports, no semantic version)
#   - 'guild wars 2 - og v<X>_<N>.vdf' in 1284210/2959110740 (old per-app exports)
ACTIVE_RE = re.compile(r'^(controller_neptune\.vdf|og v[\d.]+_\d+\.vdf)$')

touched = 0
unchanged = 0
skipped = 0
for sub in SUBDIRS:
    sub_dir = os.path.join(BASE, sub)
    if not os.path.isdir(sub_dir):
        continue
    for entry in sorted(os.listdir(sub_dir)):
        if not entry.endswith('.vdf'): continue
        if '.bak-' in entry: continue
        if not ACTIVE_RE.match(entry):
            skipped += 1
            continue
        abs_path = os.path.join(sub_dir, entry)
        rel_path = os.path.relpath(abs_path, BASE)
        new = patched_for(rel_path, abs_path)
        with open(abs_path) as f: cur = f.read()
        if cur == new:
            unchanged += 1
            continue
        if DRY_RUN:
            print(f'  WOULD PATCH {rel_path}')
            touched += 1
            continue
        # Backup once per timestamp
        bak = abs_path + f'.bak-pre-deploy-{TS}'
        if not os.path.exists(bak):
            shutil.copy2(abs_path, bak)
        with open(abs_path, 'w') as f:
            f.write(new)
        h = hashlib.sha256(open(abs_path,'rb').read()).hexdigest()[:16]
        print(f'  PATCHED {rel_path}  hash={h}')
        touched += 1

if DRY_RUN:
    print(f'(dry run) would patch {touched} file(s); {unchanged} already match; {skipped} historical/legacy skipped')
else:
    print(f'patched {touched} file(s); {unchanged} already match; {skipped} historical/legacy skipped')
PYEOF
"
    echo
done

if [ "$DRY_RUN" = 1 ]; then
    echo "Dry run complete. Re-run without --dry-run to apply."
    if [ "${#UNREACHABLE[@]}" -gt 0 ]; then
        echo "(note: ${#UNREACHABLE[@]} unreachable host(s) skipped — re-run when reachable)"
    fi
    exit 0
fi

echo "Deploy complete."
if [ "${#UNREACHABLE[@]}" -gt 0 ]; then
    echo "WARN: ${#UNREACHABLE[@]} host(s) skipped because unreachable: ${UNREACHABLE[*]}"
    echo "      Re-run this script when those host(s) are awake to finish deployment."
fi
echo
echo "To force Steam Input to reload the layout (it caches in memory):"
echo "  ssh Og@172.16.100.212 'pkill steamwebhelper'   # bazzite"
echo "  ssh deck@172.16.100.95 'pkill steamwebhelper'  # Deck native"
