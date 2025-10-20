#!/bin/bash

# Colors for output
RED='\033[31m'
GREEN='\033[32m'
CYAN='\033[36m'
YELLOW='\033[33m'
RESET='\033[0m'

# Configuration
SOURCE_DIR="/"
CLONE_DIR="$HOME/clone_system_temp"
OUTPUT_DIR="*"
FINAL_IMG_NAME="rootfs.img"

# Function to print colored output
print_info() {
    echo -e "${CYAN}$1${RESET}"
}

print_success() {
    echo -e "${GREEN}$1${RESET}"
}

print_error() {
    echo -e "${RED}$1${RESET}"
}

print_warning() {
    echo -e "${YELLOW}$1${RESET}"
}

# Function to check if running as root
check_root() {
    if [[ $EUID -eq 0 ]]; then
        print_error "This script should not be run as root. Please run as regular user and use sudo when needed."
        exit 1
    fi
}

# Function to create directories
create_directories() {
    print_info "Creating directories..."
    sudo mkdir -p "$CLONE_DIR"
    sudo mkdir -p "$OUTPUT_DIR"
    sudo chown $USER:$USER "$CLONE_DIR"
}

# Function to clone system using exact rsync command from C++ code
clone_system() {
    print_info "Cloning system using rsync..."

    # Exact rsync command from your C++ code
    sudo rsync -aHAXSr --numeric-ids --info=progress2 \
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
        --exclude=mksquashfs.sh \
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
        "$SOURCE_DIR/" "$CLONE_DIR/"

    if [[ $? -eq 0 ]]; then
        print_success "System cloned successfully to $CLONE_DIR"
    else
        print_error "Failed to clone system"
        exit 1
    fi
}

# Function to create SquashFS using exact mksquashfs command from C++ code
create_squashfs() {
    local input_dir="$1"
    local output_file="$2"

    print_info "Creating SquashFS image..."

    # Exact mksquashfs command from your C++ code
    sudo mksquashfs "$input_dir" "$output_file" \
        -noappend -comp xz -b 256K -Xbcj x86

    if [[ $? -eq 0 ]]; then
        print_success "SquashFS image created successfully: $output_file"

        # Show file size
        print_info "Image size:"
        sudo du -h "$output_file" | cut -f1
    else
        print_error "Failed to create SquashFS image"
        exit 1
    fi
}

# Function to create checksum
create_checksum() {
    local filename="$1"

    print_info "Creating SHA512 checksum..."
    sha512sum "$filename" > "${filename}.sha512"
    print_success "Checksum created: ${filename}.sha512"
}

# Function to clean up
cleanup() {
    print_info "Cleaning up temporary directory..."
    sudo rm -rf "$CLONE_DIR"
    print_success "Temporary directory cleaned up: $CLONE_DIR"
}

# Function to show disk usage
show_disk_usage() {
    print_info "Current disk usage:"
    df -h /
}

# Main execution
main() {
    print_info "Starting system clone and SquashFS creation..."

    # Check if not running as root
    check_root

    # Show initial disk usage
    show_disk_usage
    echo

    # Create directories
    create_directories

    # Clone system
    clone_system
    echo

    # Create SquashFS image
    FINAL_IMG_PATH="$OUTPUT_DIR/$FINAL_IMG_NAME"
    create_squashfs "$CLONE_DIR" "$FINAL_IMG_PATH"
    echo

    # Create checksum
    create_checksum "$FINAL_IMG_PATH"
    echo

    # Clean up
    cleanup
    echo

    # Show final disk usage
    show_disk_usage
    echo

    print_success "Process completed successfully!"
    print_info "Final image: $FINAL_IMG_PATH"
    print_info "Checksum: ${FINAL_IMG_PATH}.sha512"
}

# Handle script interruption
trap 'print_error "Script interrupted. Cleaning up..."; cleanup; exit 1' INT TERM

# Run main function
main
