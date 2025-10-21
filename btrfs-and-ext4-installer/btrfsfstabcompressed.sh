#!/bin/bash

# Colors
red_color="\033[38;2;255;0;0m"
cyan_color="\033[38;2;0;255;255m"
reset_color="\033[0m"

# Display header in red
echo -e "${red_color}"
cat << 'EOF'
░█████╗░██╗░░░░░░█████╗░██║░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░╚═╝░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
EOF
echo -e "${cyan_color}claudemods BtrfsGenFstab Arch/Cachyos v1.02 Zstd Level 22 Compression${reset_color}"

# Set ALL remaining text to cyan
echo -e "${cyan_color}"

# Function to run commands with sudo but preserve user environment
run_privileged() {
    sudo --preserve-env "$@"
}

# Backup existing fstab
echo "Backing up fstab..."
run_privileged cp /etc/fstab /etc/fstab.bak || { echo -e "${red_color}Error: Could not backup fstab${reset_color}"; exit 1; }

# Get root UUID from current / mount
echo "Getting root UUID..."
ROOT_UUID=$(run_privileged findmnt -no UUID /) || { echo -e "${red_color}Error: Could not get root UUID${reset_color}"; exit 1; }

# Check and add ONLY the mounts you specified
echo "Checking and adding subvolume entries..."
{
    echo ""
    echo "# Btrfs subvolumes (auto-added)"
    run_privileged grep -q "UUID=$ROOT_UUID.*/ .*subvol=/@" /etc/fstab || echo "UUID=$ROOT_UUID /              btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@ 0 0"
    run_privileged grep -q "UUID=$ROOT_UUID.*/root" /etc/fstab       || echo "UUID=$ROOT_UUID /root          btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@root 0 0"
    run_privileged grep -q "UUID=$ROOT_UUID.*/home" /etc/fstab       || echo "UUID=$ROOT_UUID /home          btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@home 0 0"
    run_privileged grep -q "UUID=$ROOT_UUID.*/srv" /etc/fstab        || echo "UUID=$ROOT_UUID /srv           btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@srv 0 0"
    run_privileged grep -q "UUID=$ROOT_UUID.*/var/cache" /etc/fstab || echo "UUID=$ROOT_UUID /var/cache     btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@cache 0 0"
    run_privileged grep -q "UUID=$ROOT_UUID.*/var/tmp" /etc/fstab   || echo "UUID=$ROOT_UUID /var/tmp       btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@tmp 0 0"
    run_privileged grep -q "UUID=$ROOT_UUID.*/var/log" /etc/fstab   || echo "UUID=$ROOT_UUID /var/log       btrfs   rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@log 0 0"
    run_privileged grep -q "UUID=$ROOT_UUID.*/var/lib/portables" /etc/fstab || echo "UUID=$ROOT_UUID /var/lib/portables btrfs rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@/var/lib/portables 0 0"
    run_privileged grep -q "UUID=$ROOT_UUID.*/var/lib/machines" /etc/fstab || echo "UUID=$ROOT_UUID /var/lib/machines btrfs rw,noatime,compress=zstd:22,discard=async,space_cache=v2,subvol=/@/var/lib/machines 0 0"
} | run_privileged tee -a /etc/fstab >/dev/null

echo -e "\nInstallation Complete"

# Reboot prompt
read -p "Do you want to reboot now? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${cyan_color}Rebooting system...${reset_color}"
    run_privileged reboot
else
    echo -e "${cyan_color}You may need to reboot for changes to take effect.${reset_color}"
fi

# Reset terminal color before exiting
echo -ne "${reset_color}"
