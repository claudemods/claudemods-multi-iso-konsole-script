#!/bin/bash
set -eo pipefail

# ===== Configuration =====
SOURCE_ROOT="/"                      # Source directory (adjust if needed)
IMAGE_NAME="live-system.img"          # Output image filename
IMAGE_SIZE="8G"                      # Initial sparse image size (auto-shrinks later)
BTRFS_MOUNT="/mnt/btrfs_temp"        # Temporary mount point
COMPRESSION_TYPE="zstd"              # zstd (recommended), zlib, or lzo
BTRFS_LABEL="LIVE_SYSTEM"            # Filesystem label

# ===== Cleanup Previous Runs =====
echo "[1/5] Cleaning old files..."
sudo umount "$BTRFS_MOUNT" 2>/dev/null || true
sudo rm -rf "$IMAGE_NAME"
sudo mkdir -p "$BTRFS_MOUNT"

# ===== Create Sparse Image =====
echo "[2/5] Creating sparse image..."
sudo truncate -s "$IMAGE_SIZE" "$IMAGE_NAME"

# ===== Create Filesystem with Rootdir =====
echo "[3/5] Creating Btrfs filesystem..."
sudo mkdir -p "$SOURCE_ROOT/btrfs_rootdir"  # Temporary rootdir at source
sudo mkfs.btrfs -L "$BTRFS_LABEL" --compress="$COMPRESSION_TYPE" --rootdir="$SOURCE_ROOT/btrfs_rootdir" -f "$IMAGE_NAME"
sudo rmdir "$SOURCE_ROOT/btrfs_rootdir"  # Cleanup temporary rootdir

# ===== Mount and Copy Files =====
echo "[4/5] Copying files with rsync (this may take a while)..."
sudo mount "$IMAGE_NAME" "$BTRFS_MOUNT"

sudo rsync -aHAXSr --numeric-ids --info=progress2 \
    --exclude=/etc/udev/rules.d/70-persistent-cd.rules \
    --exclude=/etc/udev/rules.d/70-persistent-net.rules \
    --exclude=/etc/mtab \
    --exclude=/etc/fstab \
    --exclude=/dev/* \
    --exclude=/proc/* \
    --exclude=/sys/* \
    --exclude=/tmp/* \
    --exclude=/run/* \
    --exclude=/mnt/* \
    --exclude=/media/* \
    --exclude=/lost+found \
    --exclude="$IMAGE_NAME" \
    --exclude="$BTRFS_MOUNT" \
    "$SOURCE_ROOT/" "$BTRFS_MOUNT/"

# ===== Force Recompression =====
echo "[5/5] Optimizing compression..."
sudo btrfs filesystem defrag -r -v -c "$COMPRESSION_TYPE" "$BTRFS_MOUNT"

# ===== Shrink to Minimum Size =====
echo "[5/5] Finalizing image..."
sudo btrfs filesystem resize max "$BTRFS_MOUNT"
sudo umount "$BTRFS_MOUNT"
sudo fallocate -d "$IMAGE_NAME"  # Punch holes in sparse file

# ===== Verification =====
echo "Verifying filesystem..."
sudo fsck.btrfs -p "$IMAGE_NAME"

echo -e "\n\033[1;32mDone!\033[0m Compressed live system image: \033[1m$IMAGE_NAME\033[0m"
echo "Size: $(sudo du -h "$IMAGE_NAME" | cut -f1)"
