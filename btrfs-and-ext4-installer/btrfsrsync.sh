#!/bin/bash

COLOR_CYAN="\033[38;2;0;255;255m"
COLOR_RED="\033[31m"
COLOR_GREEN="\033[32m"
COLOR_YELLOW="\033[33m"
COLOR_BLUE="\033[34m"
COLOR_MAGENTA="\033[35m"
COLOR_RESET="\033[0m"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${COLOR_RED}Please run as root${COLOR_RESET}"
    exit 1
fi

exec_cmd() {
    local cmd="$1"
    local result
    result=$(eval "$cmd" 2>/dev/null)
    echo "$result"
}

execute_command() {
    local cmd="$1"
    echo -e "${COLOR_CYAN}"
    local full_cmd="sudo $cmd"
    eval "$full_cmd"
    local status=$?
    echo -e "${COLOR_RESET}"
    if [ $status -ne 0 ]; then
        echo -e "${COLOR_RED}Error executing: $full_cmd${COLOR_RESET}" >&2
        return 1
    fi
    return 0
}

is_block_device() {
    local path="$1"
    if [ ! -b "$path" ]; then
        return 1
    fi
    return 0
}

directory_exists() {
    local path="$1"
    if [ ! -d "$path" ]; then
        return 1
    fi
    return 0
}

get_uk_date_time() {
    date +"%d-%m-%Y_%I:%M%P"
}

display_header() {
    echo -e "${COLOR_RED}"
    cat << "EOF"
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░╚═╝░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
EOF
    echo -e "${COLOR_CYAN}claudemods cmi btrfs rsync installer v1.02.1${COLOR_RESET}"
    echo -e "${COLOR_CYAN}Supports Btrfs (with Zstd compression) filesystem${COLOR_RESET}"
    echo
}

prepare_target_partitions() {
    local drive="$1"
    execute_command "umount -f ${drive}* 2>/dev/null || true"
    execute_command "wipefs -a $drive"
    execute_command "parted -s $drive mklabel gpt"
    execute_command "parted -s $drive mkpart primary fat32 1MiB 551MiB"
    execute_command "parted -s $drive mkpart primary btrfs 551MiB 100%"
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
    execute_command "mkfs.btrfs -f -L ROOT $root_part"
}

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

copy_system() {
    local efi_part="$1"
    local rsync_cmd="rsync -aHAXSr --numeric-ids --info=progress2 \
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
    --include=/dev \
    --include=/proc \
    --include=/tmp \
    --include=/sys \
    --include=/run \
    --include=/usr \
    --include=/etc \
    / /mnt"
    execute_command "$rsync_cmd"
    execute_command "mount $efi_part /mnt/boot/efi"
    execute_command "mkdir -p /mnt/{proc,sys,dev,run,tmp}"
    execute_command "cp btrfsfstabcompressed.sh /mnt/opt"
    execute_command "chmod +x /mnt/opt/btrfsfstabcompressed.sh"
}

install_grub_btrfs() {
    local drive="$1"
    execute_command "touch /mnt/etc/fstab"
    execute_command "mount --bind /dev /mnt/dev"
    execute_command "mount --bind /dev/pts /mnt/dev/pts"
    execute_command "mount --bind /proc /mnt/proc"
    execute_command "mount --bind /sys /mnt/sys"
    execute_command "mount --bind /run /mnt/run"

    execute_command "chroot /mnt /bin/bash -c 'mount -t efivarfs efivarfs /sys/firmware/efi/efivars'"
    execute_command "chroot /mnt /bin/bash -c 'grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB --recheck'"

    # Generate GRUB config
    execute_command "chroot /mnt /bin/bash -c 'grub-mkconfig -o /boot/grub/grub.cfg'"

    # Run the btrfs script and initramfs
    execute_command "chroot /mnt /bin/bash -c '/opt/btrfsfstabcompressed.sh'"
    execute_command "chroot /mnt /bin/bash -c 'mkinitcpio -P'"
}

chroot_into_system() {
    echo -e "${COLOR_CYAN}Chrooting into new system...${COLOR_RESET}"
    execute_command "mount --bind /dev /mnt/dev"
    execute_command "mount --bind /dev/pts /mnt/dev/pts"
    execute_command "mount --bind /proc /mnt/proc"
    execute_command "mount --bind /sys /mnt/sys"
    execute_command "mount --bind /run /mnt/run"
    execute_command "chroot /mnt"
}

install_desktop_environment() {
    while true; do
        clear
        echo -e "${COLOR_BLUE}"
        echo "╔══════════════════════════════════════════════════════════════╗"
        echo "║                   Desktop Environments                       ║"
        echo "╠══════════════════════════════════════════════════════════════╣"
        echo "║  1. GNOME                                                   ║"
        echo "║  2. KDE Plasma                                              ║"
        echo "║  3. XFCE                                                    ║"
        echo "║  4. LXQt                                                    ║"
        echo "║  5. Cinnamon                                                ║"
        echo "║  6. MATE                                                    ║"
        echo "║  7. Budgie                                                  ║"
        echo "║  8. i3 (tiling WM)                                          ║"
        echo "║  9. Sway (Wayland tiling)                                   ║"
        echo "║ 10. Hyprland (Wayland)                                      ║"
        echo "║ 11. Return to Main Menu                                     ║"
        echo "╚══════════════════════════════════════════════════════════════╝"
        echo -e "${COLOR_RESET}"

        read -p "$(echo -e "${COLOR_YELLOW}Select desktop environment (1-11): ${COLOR_RESET}")" de_choice

        case $de_choice in
            1)
                echo -e "${COLOR_CYAN}Installing GNOME...${COLOR_RESET}"
                execute_command "chroot /mnt /bin/bash -c 'pacman -S --noconfirm gnome gnome-extra gdm'"
                execute_command "chroot /mnt /bin/bash -c 'systemctl enable gdm'"
                ;;
            2)
                echo -e "${COLOR_CYAN}Installing KDE Plasma...${COLOR_RESET}"
                execute_command "chroot /mnt /bin/bash -c 'pacman -S --noconfirm plasma-desktop sddm dolphin konsole'"
                execute_command "chroot /mnt /bin/bash -c 'systemctl enable sddm'"
                ;;
            3)
                echo -e "${COLOR_CYAN}Installing XFCE...${COLOR_RESET}"
                execute_command "chroot /mnt /bin/bash -c 'pacman -S --noconfirm xfce4 xfce4-goodies lightdm lightdm-gtk-greeter'"
                execute_command "chroot /mnt /bin/bash -c 'systemctl enable lightdm'"
                ;;
            4)
                echo -e "${COLOR_CYAN}Installing LXQt...${COLOR_RESET}"
                execute_command "chroot /mnt /bin/bash -c 'pacman -S --noconfirm lxqt sddm'"
                execute_command "chroot /mnt /bin/bash -c 'systemctl enable sddm'"
                ;;
            5)
                echo -e "${COLOR_CYAN}Installing Cinnamon...${COLOR_RESET}"
                execute_command "chroot /mnt /bin/bash -c 'pacman -S --noconfirm cinnamon lightdm lightdm-gtk-greeter'"
                execute_command "chroot /mnt /bin/bash -c 'systemctl enable lightdm'"
                ;;
            6)
                echo -e "${COLOR_CYAN}Installing MATE...${COLOR_RESET}"
                execute_command "chroot /mnt /bin/bash -c 'pacman -S --noconfirm mate mate-extra lightdm lightdm-gtk-greeter'"
                execute_command "chroot /mnt /bin/bash -c 'systemctl enable lightdm'"
                ;;
            7)
                echo -e "${COLOR_CYAN}Installing Budgie...${COLOR_RESET}"
                execute_command "chroot /mnt /bin/bash -c 'pacman -S --noconfirm budgie-desktop lightdm lightdm-gtk-greeter'"
                execute_command "chroot /mnt /bin/bash -c 'systemctl enable lightdm'"
                ;;
            8)
                echo -e "${COLOR_CYAN}Installing i3 (tiling WM)...${COLOR_RESET}"
                execute_command "chroot /mnt /bin/bash -c 'pacman -S --noconfirm i3-wm i3status i3blocks dmenu lightdm lightdm-gtk-greeter'"
                execute_command "chroot /mnt /bin/bash -c 'systemctl enable lightdm'"
                ;;
            9)
                echo -e "${COLOR_CYAN}Installing Sway (Wayland tiling)...${COLOR_RESET}"
                execute_command "chroot /mnt /bin/bash -c 'pacman -S --noconfirm sway waybar wofi foot'"
                ;;
            10)
                echo -e "${COLOR_CYAN}Installing Hyprland (Wayland)...${COLOR_RESET}"
                execute_command "chroot /mnt /bin/bash -c 'pacman -S --noconfirm hyprland waybar rofi foot'"
                ;;
            11)
                return 0
                ;;
            *)
                echo -e "${COLOR_RED}Invalid selection. Please try again.${COLOR_RESET}"
                sleep 2
                continue
                ;;
        esac

        if [ $? -eq 0 ]; then
            echo -e "${COLOR_GREEN}Desktop environment installed successfully!${COLOR_RESET}"
        else
            echo -e "${COLOR_RED}Failed to install desktop environment.${COLOR_RESET}"
        fi

        read -p "$(echo -e "${COLOR_YELLOW}Press Enter to continue...${COLOR_RESET}")"
    done
}

reboot_system() {
    echo -e "${COLOR_YELLOW}Rebooting system in 5 seconds...${COLOR_RESET}"
    sleep 5
    execute_command "reboot"
}

post_install_menu() {
    while true; do
        clear
        echo -e "${COLOR_MAGENTA}"
        echo "╔══════════════════════════════════════════════════════════════╗"
        echo "║                     Post-Install Menu                        ║"
        echo "╠══════════════════════════════════════════════════════════════╣"
        echo "║  1. Chroot into New System                                   ║"
        echo "║  2. Install Desktop Environment                              ║"
        echo "║  3. Reboot System                                            ║"
        echo "║  4. Exit                                                     ║"
        echo "╚══════════════════════════════════════════════════════════════╝"
        echo -e "${COLOR_RESET}"

        read -p "$(echo -e "${COLOR_YELLOW}Select option (1-4): ${COLOR_RESET}")" choice

        case $choice in
            1)
                chroot_into_system
                ;;
            2)
                install_desktop_environment
                ;;
            3)
                reboot_system
                ;;
            4)
                echo -e "${COLOR_CYAN}Exiting post-install menu.${COLOR_RESET}"
                break
                ;;
            *)
                echo -e "${COLOR_RED}Invalid selection. Please try again.${COLOR_RESET}"
                sleep 2
                ;;
        esac
    done
}

# Main script
display_header

# Check if drive argument is provided
if [ $# -eq 0 ]; then
    echo -e "${COLOR_RED}Error: No drive specified${COLOR_RESET}"
    echo -e "${COLOR_CYAN}Usage: $0 /dev/sdX${COLOR_RESET}"
    echo -e "${COLOR_CYAN}Example: $0 /dev/vda${COLOR_RESET}"
    exit 1
fi

drive="$1"

if ! is_block_device "$drive"; then
    echo -e "${COLOR_RED}Error: $drive is not a valid block device${COLOR_RESET}" >&2
    exit 1
fi

echo -e "${COLOR_YELLOW}\nWARNING: This will erase ALL data on $drive and install a new system.\n"
read -p "$(echo -e "${COLOR_YELLOW}Are you sure you want to continue? (yes/no): ${COLOR_RESET}")" confirmation

if [ "$confirmation" != "yes" ]; then
    echo -e "${COLOR_CYAN}Operation cancelled.${COLOR_RESET}"
    exit 0
fi

echo -e "${COLOR_CYAN}\nPreparing target partitions...${COLOR_RESET}"
prepare_target_partitions "$drive"

root_part="${drive}2"

echo -e "${COLOR_CYAN}Setting up Btrfs filesystem...${COLOR_RESET}"
setup_btrfs_subvolumes "$root_part"

echo -e "${COLOR_CYAN}Copying system files (this may take a while)...${COLOR_RESET}"
copy_system "${drive}1"

echo -e "${COLOR_CYAN}Installing bootloader...${COLOR_RESET}"
install_grub_btrfs "$drive"

echo -e "${COLOR_CYAN}Cleaning up...${COLOR_RESET}"
execute_command "umount -R /mnt"

echo -e "${COLOR_GREEN}\nInstallation complete!${COLOR_RESET}"

# Show post-install menu immediately after installation
post_install_menu

echo -e "${COLOR_GREEN}Script execution finished. You can now reboot into your new system.${COLOR_RESET}"
