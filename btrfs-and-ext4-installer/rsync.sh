#!/bin/bash

# Color definitions
COLOR_CYAN='\033[38;2;0;255;255m'
COLOR_RED='\033[31m'
COLOR_GREEN='\033[32m'
COLOR_YELLOW='\033[33m'
COLOR_RESET='\033[0m'

# Function to execute commands with error handling
execute_command() {
    local cmd="$1"
    echo -e "${COLOR_CYAN}"
    local full_cmd="sudo $cmd"
    eval "$full_cmd"
    local status=$?
    echo -e "${COLOR_RESET}"
    if [ $status -ne 0 ]; then
        echo -e "${COLOR_RED}Error executing: $full_cmd${COLOR_RESET}" >&2
        exit 1
    fi
}

# Function to check if path is a block device
is_block_device() {
    local path="$1"
    if [ ! -b "$path" ]; then
        return 1
    fi
    return 0
}

# Function to check if directory exists
directory_exists() {
    local path="$1"
    if [ ! -d "$path" ]; then
        return 1
    fi
    return 0
}

# Function to get UK date time
get_uk_date_time() {
    date +"%d-%m-%Y_%I:%M%P"
}

# Function to display header
display_header() {
    echo -e "${COLOR_RED}"
    cat << "EOF"
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
EOF
    echo -e "${COLOR_CYAN}claudemods cmi rsync installer v1.01${COLOR_RESET}"
    echo -e "${COLOR_CYAN}Supports Btrfs (with Zstd compression) and Ext4 filesystems${COLOR_RESET}"
    echo
}

# Function to prepare target partitions
prepare_target_partitions() {
    local drive="$1"
    local fs_type="$2"

    execute_command "umount -f ${drive}* 2>/dev/null || true"
    execute_command "wipefs -a $drive"
    execute_command "parted -s $drive mklabel gpt"
    execute_command "parted -s $drive mkpart primary fat32 1MiB 551MiB"
    execute_command "parted -s $drive mkpart primary $fs_type 551MiB 100%"
    execute_command "parted -s $drive set 1 esp on"
    execute_command "partprobe $drive"
    sleep 2

    local efi_part="${drive}1"
    local root_part="${drive}2"

    if ! is_block_device "$efi_part" || ! is_block_device "$root_part"; then
        echo -e "${COLOR_RED}Error: Failed to create partitions${COLOR_RESET}" >&2
        exit 1
    fi

    execute_command "mkfs.vfat -F32 $efi_part"
    if [ "$fs_type" = "btrfs" ]; then
        execute_command "mkfs.btrfs -f -L ROOT $root_part"
    else
        execute_command "mkfs.ext4 -F -L ROOT $root_part"
    fi
}

# Function to setup Btrfs subvolumes
setup_btrfs_subvolumes() {
    local root_part="$1"

    # Mount root partition temporarily to create subvolumes
    execute_command "mount $root_part /mnt"

    # Create subvolumes with compression enabled
    execute_command "btrfs subvolume create /mnt/@"
    execute_command "btrfs subvolume create /mnt/@home"
    execute_command "btrfs subvolume create /mnt/@root"
    execute_command "btrfs subvolume create /mnt/@srv"
    execute_command "btrfs subvolume create /mnt/@cache"
    execute_command "btrfs subvolume create /mnt/@tmp"
    execute_command "btrfs subvolume create /mnt/@log"
    execute_command "mkdir -p /mnt/@/var/lib"
    execute_command "btrfs subvolume create /mnt/@/var/lib/portables"
    execute_command "btrfs subvolume create /mnt/@/var/lib/machines"

    # Unmount temporary mount
    execute_command "umount /mnt"

    # Mount all subvolumes with Zstd compression (level 22)
    execute_command "mount -o subvol=@,compress=zstd:22,compress-force=zstd:22 $root_part /mnt"
    execute_command "mkdir -p /mnt/{home,root,srv,tmp,var/{cache,log},var/lib/{portables,machines},boot/efi}"

    # Mount other subvolumes with same compression settings
    execute_command "mount -o subvol=@home,compress=zstd:22,compress-force=zstd:22 $root_part /mnt/home"
    execute_command "mount -o subvol=@root,compress=zstd:22,compress-force=zstd:22 $root_part /mnt/root"
    execute_command "mount -o subvol=@srv,compress=zstd:22,compress-force=zstd:22 $root_part /mnt/srv"
    execute_command "mount -o subvol=@cache,compress=zstd:22,compress-force=zstd:22 $root_part /mnt/var/cache"
    execute_command "mount -o subvol=@tmp,compress=zstd:22,compress-force=zstd:22 $root_part /mnt/tmp"
    execute_command "mount -o subvol=@log,compress=zstd:22,compress-force=zstd:22 $root_part /mnt/var/log"
    execute_command "mount -o subvol=@/var/lib/portables,compress=zstd:22,compress-force=zstd:22 $root_part /mnt/var/lib/portables"
    execute_command "mount -o subvol=@/var/lib/machines,compress=zstd:22,compress-force=zstd:22 $root_part /mnt/var/lib/machines"
}

# Function to setup Ext4 filesystem
setup_ext4_filesystem() {
    local root_part="$1"
    execute_command "mount $root_part /mnt"
    execute_command "mkdir -p /mnt/{home,boot/efi,etc,usr,var,proc,sys,dev,tmp,run}"
}

# Function to copy system
copy_system() {
    local efi_part="$1"
    local rsync_cmd="sudo rsync -aHAXSr --numeric-ids --info=progress2 "
    rsync_cmd+="--exclude=/etc/udev/rules.d/70-persistent-cd.rules "
    rsync_cmd+="--exclude=/etc/udev/rules.d/70-persistent-net.rules "
    rsync_cmd+="--exclude=/etc/mtab "
    rsync_cmd+="--exclude=/etc/fstab "
    rsync_cmd+="--exclude=/dev/* "
    rsync_cmd+="--exclude=/proc/* "
    rsync_cmd+="--exclude=/sys/* "
    rsync_cmd+="--exclude=/tmp/* "
    rsync_cmd+="--exclude=/run/* "
    rsync_cmd+="--exclude=/mnt/* "
    rsync_cmd+="--exclude=/media/* "
    rsync_cmd+="--exclude=/lost+found "
    rsync_cmd+="--include=/dev "
    rsync_cmd+="--include=/proc "
    rsync_cmd+="--include=/tmp "
    rsync_cmd+="--include=/sys "
    rsync_cmd+="--include=/run "
    rsync_cmd+="--include=/usr "
    rsync_cmd+="--include=/etc "
    rsync_cmd+="/ /mnt"

    execute_command "$rsync_cmd"
    execute_command "mount $efi_part /mnt/boot/efi"
    execute_command "mkdir -p /mnt/{proc,sys,dev,run,tmp}"
}

# Function to install GRUB for Ext4
install_grub_ext4() {
    local drive="$1"
    execute_command "mount --bind /dev /mnt/dev"
    execute_command "mount --bind /dev/pts /mnt/dev/pts"
    execute_command "mount --bind /proc /mnt/proc"
    execute_command "mount --bind /sys /mnt/sys"
    execute_command "mount --bind /run /mnt/run"
    execute_command "chroot /mnt /bin/bash -c \"mount -t efivarfs efivarfs /sys/firmware/efi/efivars \""
    execute_command "chroot /mnt /bin/bash -c \"genfstab -U /\""
    execute_command "chroot /mnt /bin/bash -c \"grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB --recheck\""
    execute_command "chroot /mnt /bin/bash -c \"grub-mkconfig -o /boot/grub/grub.cfg\""
    execute_command "chroot /mnt /bin/bash -c \"mkinitcpio -P\""
}

# Function to install GRUB for Btrfs
install_grub_btrfs() {
    local drive="$1"
    execute_command "touch /mnt/etc/fstab"
    execute_command "mount --bind /dev /mnt/dev"
    execute_command "mount --bind /dev/pts /mnt/dev/pts"
    execute_command "mount --bind /proc /mnt/proc"
    execute_command "mount --bind /sys /mnt/sys"
    execute_command "mount --bind /run /mnt/run"
    execute_command "chroot /mnt /bin/bash -c \"mount -t efivarfs efivarfs /sys/firmware/efi/efivars \""
    execute_command "chroot /mnt /bin/bash -c \"grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB --recheck\""
    execute_command "chroot /mnt /bin/bash -c \"grub-mkconfig -o /boot/grub/grub.cfg\""
    execute_command "chroot /mnt /bin/bash -c \"mkinitcpio -P\""
}

# Main script
main() {
    display_header

    echo -e "${COLOR_CYAN}Enter target drive (e.g., /dev/sda): ${COLOR_RESET}"
    read -r drive
    if ! is_block_device "$drive"; then
        echo -e "${COLOR_RED}Error: $drive is not a valid block device${COLOR_RESET}" >&2
        exit 1
    fi

    echo -e "${COLOR_CYAN}Choose filesystem type (btrfs/ext4): ${COLOR_RESET}"
    read -r fs_type

    if [ "$fs_type" != "btrfs" ] && [ "$fs_type" != "ext4" ]; then
        echo -e "${COLOR_RED}Error: Invalid filesystem type. Choose either 'btrfs' or 'ext4'${COLOR_RESET}" >&2
        exit 1
    fi

    echo -e "${COLOR_YELLOW}\nWARNING: This will erase ALL data on $drive and install a new system.\n"
    echo -e "Are you sure you want to continue? (yes/no): ${COLOR_RESET}"
    read -r confirmation

    if [ "$confirmation" != "yes" ]; then
        echo -e "${COLOR_CYAN}Operation cancelled.${COLOR_RESET}"
        exit 0
    fi

    echo -e "${COLOR_CYAN}\nPreparing target partitions...${COLOR_RESET}"
    prepare_target_partitions "$drive" "$fs_type"

    local root_part="${drive}2"

    echo -e "${COLOR_CYAN}Setting up $fs_type filesystem...${COLOR_RESET}"
    if [ "$fs_type" = "btrfs" ]; then
        setup_btrfs_subvolumes "$root_part"
    else
        setup_ext4_filesystem "$root_part"
    fi

    echo -e "${COLOR_CYAN}Copying system files (this may take a while)...${COLOR_RESET}"
    copy_system "${drive}1"

    echo -e "${COLOR_CYAN}Installing bootloader...${COLOR_RESET}"
    if [ "$fs_type" = "btrfs" ]; then
        install_grub_btrfs "$drive"
    else
        install_grub_ext4 "$drive"
    fi

    echo -e "${COLOR_CYAN}Cleaning up...${COLOR_RESET}"
    execute_command "umount -R /mnt"

    echo -e "${COLOR_GREEN}\nInstallation complete! You can now reboot into your new system.${COLOR_RESET}"
}

# Run main function
main "$@"
