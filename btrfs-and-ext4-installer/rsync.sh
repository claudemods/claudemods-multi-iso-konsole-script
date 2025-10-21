#!/bin/bash

# Color definitions
COLOR_CYAN='\033[38;2;0;255;255m'
COLOR_RED='\033[31m'
COLOR_GREEN='\033[32m'
COLOR_YELLOW='\033[33m'
COLOR_BLUE='\033[34m'
COLOR_MAGENTA='\033[35m'
COLOR_ORANGE='\033[38;5;208m'
COLOR_PURPLE='\033[38;5;93m'
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

# Function to display available drives
display_available_drives() {
    echo -e "${COLOR_YELLOW}"
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║                    Available Drives                         ║"
    echo "╠══════════════════════════════════════════════════════════════╣"
    echo -e "${COLOR_RESET}"
    
    # Display block devices using lsblk with better formatting
    echo -e "${COLOR_CYAN}Block Devices:${COLOR_RESET}"
    lsblk -o NAME,SIZE,TYPE,MOUNTPOINT,FSTYPE,MODEL | grep -v "loop" | while read line; do
        echo -e "${COLOR_GREEN}  $line${COLOR_RESET}"
    done
    
    echo
    echo -e "${COLOR_CYAN}Disk Usage (df -h):${COLOR_RESET}"
    df -h | grep -E "^/dev/" | while read line; do
        echo -e "${COLOR_BLUE}  $line${COLOR_RESET}"
    done
    
    # Show Btrfs filesystems specifically
    echo
    echo -e "${COLOR_MAGENTA}Btrfs Filesystems:${COLOR_RESET}"
    if command -v btrfs &> /dev/null; then
        btrfs filesystem show 2>/dev/null || echo "  No Btrfs filesystems found or btrfs command not available"
    else
        echo "  btrfs command not available"
    fi
    
    echo -e "${COLOR_YELLOW}"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo -e "${COLOR_RESET}"
    echo
}

# Function to display header
display_header() {
    echo -e "${COLOR_RED}"
    cat << "EOF"
░█████╗░██╗░░░░░░█████╗░██║░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
EOF
    echo -e "${COLOR_CYAN}claudemods cmi rsync installer v1.03${COLOR_RESET}"
    echo -e "${COLOR_CYAN}Supports Btrfs (with Zstd compression) and Ext4 filesystems${COLOR_RESET}"
    echo -e "${COLOR_MAGENTA}Now with Hyprland Wayland compositor support!${COLOR_RESET}"
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
    
    # Generate Btrfs fstab entries directly instead of using external script
    echo -e "${COLOR_CYAN}Generating Btrfs fstab entries...${COLOR_RESET}"
    
    # Get root UUID from current / mount
    local ROOT_UUID=$(sudo findmnt -no UUID /mnt) || { echo -e "${COLOR_RED}Error: Could not get root UUID${COLOR_RESET}" >&2; exit 1; }
    
    # Create fstab file with Btrfs entries
    sudo tee /mnt/etc/fstab > /dev/null << EOF
# /etc/fstab: static file system information.
#
# Use 'blkid' to print the universally unique identifier for a device; this may
# be used with UUID= as a more robust way to name devices that works even if
# disks are added and removed. See fstab(5).
#
# <file system> <mount point>   <type>  <options>       <dump>  <pass>

# Btrfs subvolumes with Zstd compression
UUID=$ROOT_UUID /              btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@ 0 0
UUID=$ROOT_UUID /root          btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@root 0 0
UUID=$ROOT_UUID /home          btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@home 0 0
UUID=$ROOT_UUID /srv           btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@srv 0 0
UUID=$ROOT_UUID /var/cache     btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@cache 0 0
UUID=$ROOT_UUID /tmp           btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@tmp 0 0
UUID=$ROOT_UUID /var/log       btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@log 0 0
UUID=$ROOT_UUID /var/lib/portables btrfs rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@/var/lib/portables 0 0
UUID=$ROOT_UUID /var/lib/machines btrfs rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@/var/lib/machines 0 0

# EFI System Partition
$(sudo blkid -s UUID -o value ${drive}1) /boot/efi vfat defaults 0 0
EOF

    execute_command "chroot /mnt /bin/bash -c \"mount -t efivarfs efivarfs /sys/firmware/efi/efivars \""
    execute_command "chroot /mnt /bin/bash -c \"grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB --recheck\""
    execute_command "chroot /mnt /bin/bash -c \"grub-mkconfig -o /boot/grub/grub.cfg\""
    execute_command "chroot /mnt /bin/bash -c \"mkinitcpio -P\""
}

# Function to chroot into the new system
chroot_into_system() {
    local fs_type="$1"
    local drive="$2"
    
    echo -e "${COLOR_CYAN}Mounting the new system for chroot...${COLOR_RESET}"
    
    # Mount the root partition based on filesystem type
    if [ "$fs_type" = "btrfs" ]; then
        execute_command "mount -o subvol=@,compress=zstd:22,compress-force=zstd:22 ${drive}2 /mnt"
        execute_command "mount -o subvol=@home,compress=zstd:22,compress-force=zstd:22 ${drive}2 /mnt/home"
        execute_command "mount -o subvol=@root,compress=zstd:22,compress-force=zstd:22 ${drive}2 /mnt/root"
        execute_command "mount -o subvol=@srv,compress=zstd:22,compress-force=zstd:22 ${drive}2 /mnt/srv"
        execute_command "mount -o subvol=@cache,compress=zstd:22,compress-force=zstd:22 ${drive}2 /mnt/var/cache"
        execute_command "mount -o subvol=@tmp,compress=zstd:22,compress-force=zstd:22 ${drive}2 /mnt/tmp"
        execute_command "mount -o subvol=@log,compress=zstd:22,compress-force=zstd:22 ${drive}2 /mnt/var/log"
    else
        execute_command "mount ${drive}2 /mnt"
    fi
    
    execute_command "mount ${drive}1 /mnt/boot/efi"
    execute_command "mount --bind /dev /mnt/dev"
    execute_command "mount --bind /dev/pts /mnt/dev/pts"
    execute_command "mount --bind /proc /mnt/proc"
    execute_command "mount --bind /sys /mnt/sys"
    execute_command "mount --bind /run /mnt/run"
    
    echo -e "${COLOR_GREEN}Entering chroot environment...${COLOR_RESET}"
    echo -e "${COLOR_YELLOW}Type 'exit' when done to return to the menu.${COLOR_RESET}"
    execute_command "chroot /mnt /bin/bash"
    
    echo -e "${COLOR_CYAN}Cleaning up chroot environment...${COLOR_RESET}"
    execute_command "umount -R /mnt"
}

# Function to install desktop environments
install_desktop() {
    local fs_type="$1"
    local drive="$2"
    
    echo -e "${COLOR_CYAN}Mounting system for desktop installation...${COLOR_RESET}"
    
    # Mount the system
    if [ "$fs_type" = "btrfs" ]; then
        execute_command "mount -o subvol=@,compress=zstd:22,compress-force=zstd:22 ${drive}2 /mnt"
        execute_command "mount -o subvol=@home,compress=zstd:22,compress-force=zstd:22 ${drive}2 /mnt/home"
    else
        execute_command "mount ${drive}2 /mnt"
    fi
    execute_command "mount ${drive}1 /mnt/boot/efi"
    execute_command "mount --bind /dev /mnt/dev"
    execute_command "mount --bind /dev/pts /mnt/dev/pts"
    execute_command "mount --bind /proc /mnt/proc"
    execute_command "mount --bind /sys /mnt/sys"
    execute_command "mount --bind /run /mnt/run"
    
    # Display desktop options
    echo -e "${COLOR_MAGENTA}"
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║                   Desktop Environments                       ║"
    echo "╠══════════════════════════════════════════════════════════════╣"
    echo "║  1. GNOME                         12. Enlightenment         ║"
    echo "║  2. KDE Plasma                    13. Deepin                ║"
    echo "║  3. XFCE                          14. Pantheon (Elementary) ║"
    echo "║  4. LXQt                          15. CDE                   ║"
    echo "║  5. Cinnamon                      16. UKUI                  ║"
    echo "║  6. MATE                          17. Trinity               ║"
    echo "║  7. Budgie                        18. Sugar                 ║"
    echo "║  8. i3 (tiling WM)                19. Phosh (Mobile)        ║"
    echo "║  9. Sway (Wayland tiling)         20. Hyprland (Wayland)    ║"
    echo "║ 10. Openbox                       21. Return to Main Menu   ║"
    echo "║ 11. LXDE                                                     ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo -e "${COLOR_RESET}"
    
    echo -e "${COLOR_CYAN}Select desktop environment (1-21): ${COLOR_RESET}"
    read -r desktop_choice
    
    case $desktop_choice in
        1)
            echo -e "${COLOR_CYAN}Installing GNOME Desktop...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm gnome gnome-extra gdm\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable gdm\""
            ;;
        2)
            echo -e "${COLOR_CYAN}Installing KDE Plasma...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm plasma-desktop sddm dolphin konsole\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable sddm\""
            ;;
        3)
            echo -e "${COLOR_CYAN}Installing XFCE...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm xfce4 xfce4-goodies lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        4)
            echo -e "${COLOR_CYAN}Installing LXQt...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm lxqt sddm\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable sddm\""
            ;;
        5)
            echo -e "${COLOR_CYAN}Installing Cinnamon...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm cinnamon lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        6)
            echo -e "${COLOR_CYAN}Installing MATE...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm mate mate-extra lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        7)
            echo -e "${COLOR_CYAN}Installing Budgie...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm budgie-desktop lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        8)
            echo -e "${COLOR_CYAN}Installing i3 (tiling WM)...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm i3-wm i3status i3lock dmenu lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        9)
            echo -e "${COLOR_CYAN}Installing Sway (Wayland tiling)...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm sway swaybg waybar wofi lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        10)
            echo -e "${COLOR_CYAN}Installing Openbox...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm openbox obconf tint2 menumaker lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        11)
            echo -e "${COLOR_CYAN}Installing LXDE...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm lxde lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        12)
            echo -e "${COLOR_CYAN}Installing Enlightenment...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm enlightenment terminology lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        13)
            echo -e "${COLOR_CYAN}Installing Deepin...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm deepin deepin-extra lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        14)
            echo -e "${COLOR_CYAN}Installing Pantheon (Elementary OS)...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm pantheon lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        15)
            echo -e "${COLOR_CYAN}Installing CDE (Common Desktop Environment)...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm cde lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        16)
            echo -e "${COLOR_CYAN}Installing UKUI...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm ukui lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        17)
            echo -e "${COLOR_CYAN}Installing Trinity Desktop...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm trinity-desktop lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        18)
            echo -e "${COLOR_CYAN}Installing Sugar Desktop...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm sugar sugar-fructose lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        19)
            echo -e "${COLOR_CYAN}Installing Phosh (Mobile)...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm phosh lightdm lightdm-gtk-greeter\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable lightdm\""
            ;;
        20)
            echo -e "${COLOR_PURPLE}Installing Hyprland (Modern Wayland Compositor)...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm hyprland waybar rofi wl-clipboard sddm\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable sddm\""
            echo -e "${COLOR_PURPLE}Hyprland installed! Note: You may need to configure ~/.config/hypr/hyprland.conf${COLOR_RESET}"
            ;;
        21)
            echo -e "${COLOR_CYAN}Returning to main menu...${COLOR_RESET}"
            ;;
        *)
            echo -e "${COLOR_RED}Invalid option. Returning to main menu.${COLOR_RESET}"
            ;;
    esac
    
    # Cleanup
    echo -e "${COLOR_CYAN}Cleaning up...${COLOR_RESET}"
    execute_command "umount -R /mnt"
}

# Function to display post-install menu
post_install_menu() {
    local fs_type="$1"
    local drive="$2"
    
    while true; do
        echo -e "${COLOR_BLUE}"
        echo "╔══════════════════════════════════════╗"
        echo "║         Post-Install Menu           ║"
        echo "╠══════════════════════════════════════╣"
        echo "║ 1. Chroot into New System           ║"
        echo "║ 2. Install Desktop Environment      ║"
        echo "║ 3. Reboot System                    ║"
        echo "║ 4. Exit                             ║"
        echo "╚══════════════════════════════════════╝"
        echo -e "${COLOR_RESET}"
        
        echo -e "${COLOR_CYAN}Select an option (1-4): ${COLOR_RESET}"
        read -r choice
        
        case $choice in
            1)
                chroot_into_system "$fs_type" "$drive"
                ;;
            2)
                install_desktop "$fs_type" "$drive"
                ;;
            3)
                echo -e "${COLOR_GREEN}Rebooting system...${COLOR_RESET}"
                execute_command "umount -R /mnt 2>/dev/null || true"
                sudo reboot
                ;;
            4)
                echo -e "${COLOR_GREEN}Exiting. Goodbye!${COLOR_RESET}"
                exit 0
                ;;
            *)
                echo -e "${COLOR_RED}Invalid option. Please try again.${COLOR_RESET}"
                ;;
        esac
        
        echo
        echo -e "${COLOR_YELLOW}Press Enter to continue...${COLOR_RESET}"
        read -r
    done
}

# Main script
main() {
    display_header
    display_available_drives

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

    echo -e "${COLOR_GREEN}\nInstallation complete!${COLOR_RESET}"
    
    # Show post-install menu
    post_install_menu "$fs_type" "$drive"
}

# Run main function
main "$@"
