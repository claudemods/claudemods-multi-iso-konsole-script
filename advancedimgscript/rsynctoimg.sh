#!/bin/bash

# Check if running as root
if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run as root" >&2
    exit 1
fi

# Parameters
IMG_SIZE="8G"               # Size of the image file
IMG_NAME="system_backup.img" # Output image filename
MOUNT_POINT="/mnt/btrfs_temp" # Temporary mount point
SOURCE_DIR="/"              # Source directory to backup
COMPRESSION="zstd:22"       # Maximum compression level

# Create the image file
echo "Creating $IMG_SIZE BTRFS image file with maximum compression: $IMG_NAME"
if ! truncate -s "$IMG_SIZE" "$IMG_NAME"; then
    echo "Failed to create image file" >&2
    exit 1
fi

# Format as BTRFS with maximum compression
echo "Formatting as BTRFS with $COMPRESSION compression"
if ! mkfs.btrfs -f -L "SYSTEM_BACKUP" "$IMG_NAME"; then
    echo "Failed to format BTRFS filesystem" >&2
    rm -f "$IMG_NAME"
    exit 1
fi

# Create mount point
echo "Mounting the image file with maximum compression"
mkdir -p "$MOUNT_POINT" || exit 1
if ! mount -o loop,compress="$COMPRESSION" "$IMG_NAME" "$MOUNT_POINT"; then
    echo "Failed to mount image file" >&2
    rm -f "$IMG_NAME"
    rmdir "$MOUNT_POINT"
    exit 1
fi

# Rsync command to copy files
echo "Copying files with rsync (this may take a while)..."
if ! rsync -aHAXSr --numeric-ids --info=progress2 \
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
    --exclude=clone_system_temp \
    --include=dev \
    --include=proc \
    --include=tmp \
    --include=sys \
    --include=run \
    --include=dev \
    --include=proc \
    --include=tmp \
    --include=sys \
    --include=usr \
    --include=etc \
    "$SOURCE_DIR" "$MOUNT_POINT"; then
    echo "Rsync failed" >&2
    umount "$MOUNT_POINT"
    rm -f "$IMG_NAME"
    rmdir "$MOUNT_POINT"
    exit 1
fi

# Clean up
echo "Unmounting and cleaning up"
if ! umount "$MOUNT_POINT"; then
    echo "Warning: Failed to unmount $MOUNT_POINT" >&2
fi
if ! rmdir "$MOUNT_POINT"; then
    echo "Warning: Failed to remove mount directory" >&2
fi

echo -e "\nCompressed BTRFS image created successfully: $IMG_NAME"
echo "Compression used: $COMPRESSION"
echo "You can mount it with:"
echo "  sudo mount -o loop,compress=$COMPRESSION $IMG_NAME /mnt/your_mount_point"
