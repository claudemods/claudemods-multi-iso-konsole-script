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
    echo -e "${COLOR_CYAN}Supports Ext4 filesystems${COLOR_RESET}"
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
    execute_command "mkfs.ext4 -F -L ROOT $root_part"
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

# Function to chroot into the new system
chroot_into_system() {
    local fs_type="$1"
    local drive="$2"
    
    echo -e "${COLOR_CYAN}Mounting the new system for chroot...${COLOR_RESET}"
    
    execute_command "mount ${drive}2 /mnt"
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
    
    execute_command "mount ${drive}2 /mnt"
    execute_command "mount ${drive}1 /mnt/boot/efi"
    execute_command "mount --bind /dev /mnt/dev"
    execute_command "mount --bind /dev/pts /mnt/dev/pts"
    execute_command "mount --bind /proc /mnt/proc"
    execute_command "mount --bind /sys /mnt/sys"
    execute_command "mount --bind /run /mnt/run"
    
    # Display desktop options - Top 10 Arch package list
    echo -e "${COLOR_MAGENTA}"
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
    
    echo -e "${COLOR_CYAN}Select desktop environment (1-11): ${COLOR_RESET}"
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
            echo -e "${COLOR_PURPLE}Installing Hyprland (Modern Wayland Compositor)...${COLOR_RESET}"
            execute_command "chroot /mnt /bin/bash -c \"pacman -S --noconfirm hyprland waybar rofi wl-clipboard sddm\""
            execute_command "chroot /mnt /bin/bash -c \"systemctl enable sddm\""
            echo -e "${COLOR_PURPLE}Hyprland installed! Note: You may need to configure ~/.config/hypr/hyprland.conf${COLOR_RESET}"
            ;;
        11)
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

    echo -e "${COLOR_CYAN}Choose filesystem type (ext4/btrfs): ${COLOR_RESET}"
    read -r fs_type

    if [ "$fs_type" = "btrfs" ]; then
        echo -e "${COLOR_CYAN}Executing btrfsrsync.sh with drive: $drive${COLOR_RESET}"
        execute_command "./btrfsrsync.sh $drive"
        echo -e "${COLOR_GREEN}Btrfs installation complete!${COLOR_RESET}"
        exit 0
    fi

    if [ "$fs_type" != "ext4" ]; then
        echo -e "${COLOR_RED}Error: Invalid filesystem type. Choose 'ext4' or 'btrfs'${COLOR_RESET}" >&2
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
    setup_ext4_filesystem "$root_part"

    echo -e "${COLOR_CYAN}Copying system files (this may take a while)...${COLOR_RESET}"
    copy_system "${drive}1"

    echo -e "${COLOR_CYAN}Installing bootloader...${COLOR_RESET}"
    install_grub_ext4 "$drive"

    echo -e "${COLOR_CYAN}Cleaning up...${COLOR_RESET}"
    execute_command "umount -R /mnt"

    echo -e "${COLOR_GREEN}\nInstallation complete!${COLOR_RESET}"
    
    # Show post-install menu
    post_install_menu "$fs_type" "$drive"
}

# Run main function
main "$@"
