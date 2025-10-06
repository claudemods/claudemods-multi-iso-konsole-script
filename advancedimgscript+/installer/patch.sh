#!/bin/bash

# Function to detect the distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
    else
        echo "Cannot detect the distribution. Exiting."
        exit 1
    fi
}

# Main script starts here
cd /home/$USER >/dev/null 2>&1
mkdir -p /home/$USER/.config/cmi >/dev/null 2>&1

# Detect the distribution
detect_distro

# Conditional logic based on the detected distribution
if [[ "$DISTRO" == "arch" || "$DISTRO" == "cachyos" ]]; then
    # Commands for Arch/CachyOS
    sudo pacman -S --noconfirm git rsync squashfs-tools xorriso grub dosfstools unzip nano arch-install-scripts bash-completion erofs-utils findutils jq libarchive libisoburn lsb-release lvm2 mkinitcpio-archiso mkinitcpio-nfs-utils mtools nbd pacman-contrib parted procps-ng pv python sshfs syslinux xdg-utils zsh-completions kernel-modules-hook virt-manager qt6-tools btrfs-progs e2fsprogs f2fs-tools xfsprogs xfsdump cmake
    git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git  >/dev/null 2>&1
    cd /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript+/updatermain && qmake6 && make >/dev/null 2>&1
    ./advancedcscriptupdater.bin && rm -rf /home/$USER/claudemods-multi-iso-konsole-script
else
    echo "Unsupported distribution: $DISTRO"
    exit 1
fi
