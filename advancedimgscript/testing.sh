#!/bin/bash

# Script to create a Btrfs image with Zstd level 22 compression applied during rsync

# Check if running as root
if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run as root"
    exit 1
fi

# Configuration
SOURCE="/mnt/arch"              # Source system mount point
IMAGE_SIZE="6G"                # Initial image size (adjust as needed)
IMAGE_NAME="rootfs.img"      # Output image name
ZSTD_LEVEL=22                   # Zstd compression level
MOUNT_POINT="/mnt/img"          # Temporary mount point

# Install required packages if missing
pacman -Sy --noconfirm btrfs-progs zstd rsync >/dev/null 2>&1

# Create the image file
echo "Creating image file of size $IMAGE_SIZE..."
fallocate -l "$IMAGE_SIZE" "$IMAGE_NAME"
mkfs.btrfs -L "Arch Linux" "$IMAGE_NAME"

# Mount with Zstd compression
mkdir -p "$MOUNT_POINT"
mount -o compress-force=zstd:$ZSTD_LEVEL "$IMAGE_NAME" "$MOUNT_POINT"

# Rsync with exclusions (compression happens at filesystem level)
echo "Copying files with rsync (Zstd:$ZSTD_12 compression applied during transfer)..."
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
    "$SOURCE/" "$MOUNT_POINT/"

sync

# Shrink the image to minimum size
echo "Shrinking image to minimum size..."
umount "$MOUNT_POINT"
mount "$IMAGE_NAME" "$MOUNT_POINT"
btrfs filesystem resize max "$MOUNT_POINT"
umount "$MOUNT_POINT"

# Verify the image
echo "Verifying Btrfs image..."
btrfs check "$IMAGE_NAME"

echo "Process completed!"
echo "Final image size: $(du -h "$IMAGE_NAME")"
echo "Compression was applied during transfer via Btrfs Zstd:$ZSTD_LEVEL"
