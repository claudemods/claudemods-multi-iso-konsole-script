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

echo "Creating Btrfs image with enforced Zstd:$ZSTD_LEVEL compression..."

# Create image file
echo "1/5 Allocating $IMAGE_SIZE image file..."
sudo truncate -s "$IMAGE_SIZE" "$IMAGE_NAME"
sudo mkfs.btrfs -f --checksum crc32c -L "Arch Linux" "$IMAGE_NAME"

# Mount with STRICT compression settings
echo "2/5 Mounting image with enforced Zstd:$ZSTD_LEVEL compression..."
sudo mkdir -p "$MOUNT_POINT"
sudo mount -o compress-force=zstd:$ZSTD_LEVEL,nodatacow,noatime,space_cache=v2,autodefrag,discard=async "$IMAGE_NAME" "$MOUNT_POINT"

# Set compression property at filesystem level
sudo btrfs property set "$MOUNT_POINT" compression zstd

# Create subvolume for root
echo "3/5 Creating root subvolume..."
sudo btrfs subvolume create "$MOUNT_POINT/@root"
sudo umount "$MOUNT_POINT"

# Remount with subvolume
sudo mount -o compress-force=zstd:$ZSTD_LEVEL,nodatacow,noatime,space_cache=v2,autodefrag,discard=async,subvol=@root "$IMAGE_NAME" "$MOUNT_POINT"

# Copy files with verification of compression
echo "4/5 Copying files with compression verification..."
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
    --exclude=*rootfs1.img \
    --exclude=btrfs_temp \
    --exclude=rootfs.img \
    "/" "$MOUNT_POINT/"

# Force compression of existing files
echo "5/5 Compressing all files..."
sudo find "$MOUNT_POINT" -type f -exec sudo btrfs filesystem defrag -v -czstd {} +

# Verification
echo "Verifying compression..."
sync
sudo btrfs filesystem du "$MOUNT_POINT"
sudo btrfs filesystem defrag -r -v -czstd "$MOUNT_POINT"

# Final cleanup
echo "Finalizing image..."
sudo umount "$MOUNT_POINT"
sudo btrfs check "$IMAGE_NAME"

echo "Operation completed successfully"
echo "Created compressed image: $IMAGE_NAME"
echo "Compression stats:"
echo "Original size: $(sudo du -h --apparent-size "$IMAGE_NAME" | cut -f1)"
echo "Disk usage:    $(sudo du -h "$IMAGE_NAME" | cut -f1)"
