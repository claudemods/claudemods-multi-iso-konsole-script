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
OUTPUT_DIR="clone"
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

# Function to create and mount bind directory
setup_bind_mount() {
    print_info "Setting up bind mount..."

    # Create temporary directory
    sudo mkdir -p "$CLONE_DIR"
    sudo chown $USER:$USER "$CLONE_DIR"

    # Create bind mount
    sudo mount --bind "$SOURCE_DIR" "$CLONE_DIR"
    
    if [[ $? -eq 0 ]]; then
        print_success "Bind mount created successfully: $SOURCE_DIR -> $CLONE_DIR"
    else
        print_error "Failed to create bind mount"
        exit 1
    fi
}

# Function to create SquashFS with exclusions
create_squashfs() {
    local input_dir="$1"
    local output_file="$2"

    print_info "Creating SquashFS image with exclusions..."

    # Create output directory
    mkdir -p "$OUTPUT_DIR"

    # Exact mksquashfs command with exclusions from C++ code
    sudo mksquashfs "$input_dir" "$output_file" \
        -noappend -comp xz -b 256K -Xbcj x86 \
        -e etc/udev/rules.d/70-persistent-cd.rules \
        -e etc/udev/rules.d/70-persistent-net.rules \
        -e etc/mtab \
        -e etc/fstab \
        -e dev/* \
        -e proc/* \
        -e sys/* \
        -e tmp/* \
        -e run/* \
        -e mnt/* \
        -e media/* \
        -e lost+found \
        -e clone_system_temp

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
    print_info "Cleaning up..."

    # Unmount bind mount if it exists
    if mountpoint -q "$CLONE_DIR"; then
        sudo umount "$CLONE_DIR"
        print_success "Bind mount unmounted: $CLONE_DIR"
    fi

    # Remove temporary directory
    if [[ -d "$CLONE_DIR" ]]; then
        sudo rm -rf "$CLONE_DIR"
        print_success "Temporary directory cleaned up: $CLONE_DIR"
    fi
}

# Function to show disk usage
show_disk_usage() {
    print_info "Current disk usage:"
    df -h /
}

# Main execution
main() {
    print_info "Starting system clone and SquashFS creation using bind mount..."

    # Check if not running as root
    check_root

    # Show initial disk usage
    show_disk_usage
    echo

    # Setup bind mount
    setup_bind_mount
    echo

    # Create SquashFS image directly from bind mount with exclusions
    FINAL_IMG_PATH="$OUTPUT_DIR/$FINAL_IMG_NAME"
    create_squashfs "$CLONE_DIR" "$FINAL_IMG_PATH"
    echo

    # Clean up bind mount
    cleanup
    echo

    # Create checksum
    create_checksum "$FINAL_IMG_PATH"
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
