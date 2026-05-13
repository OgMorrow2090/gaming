#!/bin/bash
# btrfs-nightly-clone.sh — incremental btrfs send/receive from nvme1 (OS) to nvme0 (clone)
# Also syncs EFI + boot partitions. Designed for systemd timer, runs as root.
set -euo pipefail

SRC_DEV=/dev/nvme1n1p3
DST_DEV=/dev/nvme0n1p3
SRC_MNT=/mnt/source-btrfs
DST_MNT=/mnt/clone-btrfs
SNAP_DIR=.snapshots
SUBVOLS=(root home var)

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*"; }

cleanup() {
    log "Unmounting..."
    umount "$DST_MNT" 2>/dev/null || true
    umount "$SRC_MNT" 2>/dev/null || true
    umount /mnt/clone-efi 2>/dev/null || true
    umount /mnt/clone-boot 2>/dev/null || true
}
trap cleanup EXIT

log "=== Btrfs nightly clone starting ==="

mkdir -p "$SRC_MNT" "$DST_MNT" /mnt/clone-efi /mnt/clone-boot
mount -o subvol=/ "$SRC_DEV" "$SRC_MNT"
mount "$DST_DEV" "$DST_MNT"
mkdir -p "$SRC_MNT/$SNAP_DIR"

for sv in "${SUBVOLS[@]}"; do
    log "--- $sv ---"

    if [ -d "$SRC_MNT/$SNAP_DIR/$sv.latest" ]; then
        # Incremental: snapshot, send delta, rotate
        btrfs subvolume snapshot -r "$SRC_MNT/$sv" "$SRC_MNT/$SNAP_DIR/$sv.new"
        log "Sending $sv (incremental)..."
        btrfs send -p "$SRC_MNT/$SNAP_DIR/$sv.latest" "$SRC_MNT/$SNAP_DIR/$sv.new" | btrfs receive "$DST_MNT/"
        # Rotate on clone
        btrfs subvolume delete "$DST_MNT/$sv.latest"
        mv "$DST_MNT/$sv.new" "$DST_MNT/$sv.latest"
        # Rotate on source
        btrfs subvolume delete "$SRC_MNT/$SNAP_DIR/$sv.latest"
        mv "$SRC_MNT/$SNAP_DIR/$sv.new" "$SRC_MNT/$SNAP_DIR/$sv.latest"
        log "$sv incremental send complete"
    else
        # First run / recovery: full send
        btrfs subvolume snapshot -r "$SRC_MNT/$sv" "$SRC_MNT/$SNAP_DIR/$sv.latest"
        log "Sending $sv (full)..."
        btrfs send "$SRC_MNT/$SNAP_DIR/$sv.latest" | btrfs receive "$DST_MNT/"
        log "$sv full send complete"
    fi
done

# Sync EFI + boot
mount /dev/nvme0n1p1 /mnt/clone-efi
mount /dev/nvme0n1p2 /mnt/clone-boot
rsync -a --delete /boot/efi/ /mnt/clone-efi/
rsync -a --delete /boot/ /mnt/clone-boot/ --exclude efi
log "EFI + boot synced"

log "=== Nightly clone complete ==="
