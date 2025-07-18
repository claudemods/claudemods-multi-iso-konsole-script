#!/bin/bash

# Check if running as root
if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run as root" >&2
    exit 1
fi

# Parameters
IMG_SIZE="6G"                # Size of the image file
IMG_NAME="system_backup.img" # Output image filename
MOUNT_POINT="/mnt/btrfs_temp" # Temporary mount point
SOURCE_DIR="/"               # Source directory to backup
COMPRESSION_LEVEL="22"       # Maximum compression level

# Create the image file
echo "Creating $IMG_SIZE BTRFS image file: $IMG_NAME"
if ! truncate -s "$IMG_SIZE" "$IMG_NAME"; then
    echo "Failed to create image file" >&2
    exit 1
fi

# Format as BTRFS with compression feature enabled
echo "Formatting as BTRFS (compression will be applied during copy)"
if ! mkfs.btrfs -f -L "SYSTEM_BACKUP" "$IMG_NAME"; then
    echo "Failed to format BTRFS filesystem" >&2
    rm -f "$IMG_NAME"
    exit 1
fi

# Create mount point
echo "Mounting the image file with maximum compression settings"
mkdir -p "$MOUNT_POINT" || exit 1
if ! mount -o loop,compress=zstd:$COMPRESSION_LEVEL,discard,noatime,space_cache=v2,autodefrag "$IMG_NAME" "$MOUNT_POINT"; then
    echo "Failed to mount image file" >&2
    rm -f "$IMG_NAME"
    rmdir "$MOUNT_POINT"
    exit 1
fi

# Rsync command to copy files with compression
echo "Copying files with rsync (this will apply zstd:$COMPRESSION_LEVEL compression)..."
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
    --exclude=btrfs_temp \
    --exclude=system_backup.img \
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

# Force compression on all files
echo "Ensuring all files are compressed..."
if ! btrfs filesystem defragment -r -v -czstd:$COMPRESSION_LEVEL "$MOUNT_POINT"; then
    echo "Defragmentation/compression failed for some files" >&2
fi

# Clean up
echo "Unmounting and finalizing..."
sync
if ! umount "$MOUNT_POINT"; then
    echo "Warning: Failed to unmount $MOUNT_POINT" >&2
fi
if ! rmdir "$MOUNT_POINT"; then
    echo "Warning: Failed to remove mount directory" >&2
fi

echo -e "\nCompressed BTRFS image created successfully: $IMG_NAME"
echo "Compression: zstd at level $COMPRESSION_LEVEL"
echo "Mount with:"
echo "  sudo mount -o loop,compress=zstd:$COMPRESSION_LEVEL $IMG_NAME /mnt/point"
