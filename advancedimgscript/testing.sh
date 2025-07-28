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

echo -e "\033[36mCreating Btrfs filesystem with maximum compression (zstd:$ZSTD_LEVEL)...\033[0m"

# Create image file
echo "1/4 Allocating $IMAGE_SIZE image file..."
sudo truncate -s "$IMAGE_SIZE" "$IMAGE_NAME"

# First create filesystem without compression
echo "Creating Btrfs filesystem..."
sudo mkfs.btrfs -f -L "ROOT" "$IMAGE_NAME"

# Mount and enable compression property
echo "2/4 Setting compression property..."
sudo mkdir -p "$MOUNT_POINT"
sudo mount "$IMAGE_NAME" "$MOUNT_POINT"
sudo btrfs property set "$MOUNT_POINT" compression "zstd:$ZSTD_LEVEL"
sudo umount "$MOUNT_POINT"

# Remount with compression options
echo "3/4 Mounting with compression..."
sudo mount -o "compress=zstd:$ZSTD_LEVEL,compress-force=zstd:$ZSTD_LEVEL,nodatacow,noatime,space_cache=v2" "$IMAGE_NAME" "$MOUNT_POINT"

# Copy files with rsync
echo "4/4 Copying files with rsync..."
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
    --exclude=img \
    --exclude=arch \
    --exclude=rootfs.img \
    "/" "$MOUNT_POINT/"

# Force compression of existing files
echo "Compressing all files..."
sudo find "$MOUNT_POINT" -type f -exec sudo btrfs filesystem defrag -v -czstd:$ZSTD_LEVEL {} +

# Verification
echo "Verifying compression..."
sync
sudo btrfs filesystem du "$MOUNT_POINT"

# Final cleanup
echo "Finalizing image..."
sudo umount "$MOUNT_POINT"
sudo btrfs check "$IMAGE_NAME"

echo -e "\033[32mOperation completed successfully\033[0m"
echo "Created compressed image: $IMAGE_NAME"
echo "Compression stats:"
echo "Original size: $(sudo du -h --apparent-size "$IMAGE_NAME" | cut -f1)"
echo "Disk usage:    $(sudo du -h "$IMAGE_NAME" | cut -f1)"
