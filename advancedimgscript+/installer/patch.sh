#!/bin/bash

# Color definitions
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[38;2;0;255;255m'
NC='\033[0m' # No Color

# Function to print colored output - ENTIRE message in green
print_status() {
    echo -e "${GREEN}[INFO] $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}[WARNING] $1${NC}"
}

print_error() {
    echo -e "${RED}[ERROR] $1${NC}"
}

# Function to run commands with cyan output
run_command() {
    # Enable cyan color for the command output
    exec 3>&1
    local exit_code
    (
        echo -e "${CYAN}"
        eval "$@"
        exit_code=$?
        echo -e "${NC}"
        exit $exit_code
    ) | while IFS= read -r line; do
        echo -e "${CYAN}${line}${NC}" >&3
    done
    return ${PIPESTATUS[0]}
}

# Function to run commands with cyan output and capture exit code
run_command_cyan() {
    echo -e "${CYAN}"
    eval "$@"
    local exit_code=$?
    echo -e "${NC}"
    return $exit_code
}

# Function to detect the distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
    else
        print_error "Cannot detect the distribution. Exiting."
        exit 1
    fi
}

# Main script starts here
print_status "Setting up directories"
cd /home/$USER >/dev/null 2>&1
mkdir -p /home/$USER/.config/cmi >/dev/null 2>&1

# Detect the distribution
print_status "Detecting distribution"
detect_distro

# Conditional logic based on the detected distribution
if [[ "$DISTRO" == "arch" || "$DISTRO" == "cachyos" ]]; then
    # Commands for Arch/CachyOS
    
    print_status "Updating pacman database"
    print_status "Installing dependencies"
    print_status "git clone and install"
    print_status "further installation of calamares"
    run_command_cyan "sudo pacman -Sy"
    print_status "Installing dependencies"
    run_command_cyan "sudo pacman -S --needed --noconfirm git rsync squashfs-tools xorriso grub dosfstools unzip nano arch-install-scripts bash-completion erofs-utils findutils jq libarchive libisoburn lsb-release lvm2 mkinitcpio-archiso mkinitcpio-nfs-utils mtools nbd pacman-contrib parted procps-ng pv python sshfs syslinux xdg-utils zsh-completions kernel-modules-hook virt-manager qt6-tools btrfs-progs e2fsprogs f2fs-tools xfsprogs xfsdump cmake"
    print_status "Git cloning repository"
    run_command_cyan "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git"
    print_status "Building installer"
    run_command_cyan "cd /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript+/updatermain && qmake6"
    run_command_cyan "cd /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript+/updatermain && make"
    print_status "Installing"
    run_command_cyan "cd /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript+/updatermain && ./advancedcscriptupdater.bin"
    print_status "Cleaning up"
    run_command_cyan "rm -rf /home/$USER/claudemods-multi-iso-konsole-script"
else
    print_error "Unsupported distribution: $DISTRO"
    exit 1
fi
