#!/bin/bash
set -euo pipefail

# Configuration
SOURCE="/mnt/arch"
IMAGE_NAME="rooffs.img"
IMAGE_SIZE="6G"
MOUNT_POINT="/mnt/img"
ZSTD_LEVEL=22

# Verify root privileges
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root" >&2
    exit 1
fi

# Verify source exists
if [[ ! -d "$SOURCE" ]]; then
    echo "Source directory $SOURCE does not exist" >&2
    exit 1
fi

# Check if image already exists
if [[ -f "$IMAGE_NAME" ]]; then
    echo "Image file $IMAGE_NAME already exists. Please remove it first." >&2
    exit 1
fi

echo "Creating Btrfs image with Zstd:$ZSTD_LEVEL compression..."

# Create image file (using truncate instead of fallocate for sparse file)
echo "1/4 Allocating $IMAGE_SIZE image file..."
truncate -s "$IMAGE_SIZE" "$IMAGE_NAME"
mkfs.btrfs -O compress-force -L "Arch Linux" "$IMAGE_NAME"

# Mount with compression
echo "2/4 Mounting image with Zstd:$ZSTD_LEVEL compression..."
mkdir -p "$MOUNT_POINT"
mount -o compress-force=zstd:"$ZSTD_LEVEL" "$IMAGE_NAME" "$MOUNT_POINT"

# Execute rsync with your exact parameters
echo "3/4 Copying files with rsync (this may take a while)..."
rsync -aHAXSr --numeric-ids --info=progress2 \
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
    --exclude=*rootfs1.img \
    --exclude=btrfs_temp \
    --exclude=rootfs.img \
    "/" "$MOUNT_POINT/"

sync

# Cleanup and verification
echo "4/4 Finalizing image..."
umount "$MOUNT_POINT"
btrfs check "$IMAGE_NAME"

echo "Operation completed successfully"
echo "Created image: $IMAGE_NAME with Zstd:$ZSTD_LEVEL compression"
