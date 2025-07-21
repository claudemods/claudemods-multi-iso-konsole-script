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
COMPRESSION_LEVEL="22"       # Maximum compression level for BTRFS
SQUASHFS_COMPRESSION="zstd"  # SquashFS compression algorithm
SQUASHFS_COMPRESSION_ARGS=("-Xcompression-level" "22") # SquashFS compression args

# Ask user for filesystem type
echo "Choose the filesystem type:"
echo "1) btrfs"
echo "2) ext4"
read -p "Enter your choice (1 or 2): " choice

if [ "$choice" = "1" ]; then
    FS_TYPE="btrfs"
elif [ "$choice" = "2" ]; then
    FS_TYPE="ext4"
else
    echo "Invalid choice. Defaulting to btrfs."
    FS_TYPE="btrfs"
fi

# Create the image file
echo "Creating $IMG_SIZE $FS_TYPE image file: $IMG_NAME"
if ! truncate -s "$IMG_SIZE" "$IMG_NAME"; then
    echo "Failed to create image file" >&2
    exit 1
fi

# Format the image based on filesystem type
if [ "$FS_TYPE" = "btrfs" ]; then
    echo "Formatting as BTRFS with forced zstd:$COMPRESSION_LEVEL compression"
    if ! mkfs.btrfs -f --compress-force=zstd:$COMPRESSION_LEVEL -L "SYSTEM_BACKUP" "$IMG_NAME"; then
        echo "Failed to format BTRFS filesystem" >&2
        rm -f "$IMG_NAME"
        exit 1
    fi
else
    echo "Formatting as ext4"
    if ! mkfs.ext4 -F -L "SYSTEM_BACKUP" "$IMG_NAME"; then
        echo "Failed to format ext4 filesystem" >&2
        rm -f "$IMG_NAME"
        exit 1
    fi
fi

# Create mount point
echo "Mounting the image file"
mkdir -p "$MOUNT_POINT" || exit 1

if [ "$FS_TYPE" = "btrfs" ]; then
    if ! mount -o loop,compress-force=zstd:$COMPRESSION_LEVEL,discard,noatime,space_cache=v2,autodefrag "$IMG_NAME" "$MOUNT_POINT"; then
        echo "Failed to mount BTRFS image file" >&2
        rm -f "$IMG_NAME"
        rmdir "$MOUNT_POINT"
        exit 1
    fi
else
    if ! mount -o loop,discard,noatime "$IMG_NAME" "$MOUNT_POINT"; then
        echo "Failed to mount ext4 image file" >&2
        rm -f "$IMG_NAME"
        rmdir "$MOUNT_POINT"
        exit 1
    fi
fi

# Rsync command to copy files
echo "Copying files with rsync..."
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
    "$SOURCE_DIR" "$MOUNT_POINT"; then
    echo "Rsync failed" >&2
    umount "$MOUNT_POINT" || echo "Warning: Failed to unmount $MOUNT_POINT" >&2
    rm -f "$IMG_NAME"
    rmdir "$MOUNT_POINT" || echo "Warning: Failed to remove mount directory" >&2
    exit 1
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

# Compress the final image with SquashFS
echo "Compressing final image with SquashFS..."
SQUASHFS_OUTPUT="${IMG_NAME%.img}.squashfs.img"
if ! mksquashfs "$IMG_NAME" "$SQUASHFS_OUTPUT" \
    -comp "$SQUASHFS_COMPRESSION" "${SQUASHFS_COMPRESSION_ARGS[@]}" \
    -noappend -no-progress; then
    echo "Failed to create SquashFS image" >&2
    exit 1
fi

# Create checksum
echo "Creating checksum..."
if ! md5sum "$SQUASHFS_OUTPUT" > "${SQUASHFS_OUTPUT}.md5"; then
    echo "Failed to create checksum" >&2
fi

# Final message based on filesystem type
if [ "$FS_TYPE" = "btrfs" ]; then
    echo -e "\nCompressed BTRFS image created successfully: $IMG_NAME"
    echo "Final SquashFS compressed image: $SQUASHFS_OUTPUT"
    echo "Compression: zstd at level $COMPRESSION_LEVEL (forced during write)"
    echo "SquashFS compression: $SQUASHFS_COMPRESSION with level ${SQUASHFS_COMPRESSION_ARGS[1]}"
    echo "Mount original with:"
    echo "  sudo mount -o loop,compress=zstd:$COMPRESSION_LEVEL $IMG_NAME /mnt/point"
    echo "Mount SquashFS with:"
    echo "  sudo mount -t squashfs $SQUASHFS_OUTPUT /mnt/point -o loop"
else
    echo -e "\nExt4 image created successfully: $IMG_NAME"
    echo "Final SquashFS compressed image: $SQUASHFS_OUTPUT"
    echo "SquashFS compression: $SQUASHFS_COMPRESSION with level ${SQUASHFS_COMPRESSION_ARGS[1]}"
    echo "Mount original with:"
    echo "  sudo mount -o loop $IMG_NAME /mnt/point"
    echo "Mount SquashFS with:"
    echo "  sudo mount -t squashfs $SQUASHFS_OUTPUT /mnt/point -o loop"
fi
