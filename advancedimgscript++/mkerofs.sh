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

# Function to show progress bar
show_progress_bar() {
    local percentage=$1
    local width=50
    local filled=$((percentage * width / 100))
    local empty=$((width - filled))

    # Build the bar
    printf "\r${CYAN}["
    printf "%${filled}s" | tr ' ' '='
    printf "%${empty}s" | tr ' ' ' '
    printf "] %3d%%${RESET}" "$percentage"
}

# Function to run progress bar simulation while EROFS is creating
progress_bar_simulation() {
    local pid=$1
    local percentage=0

    # Hide cursor
    tput civis

    # Start at 10% immediately
    percentage=10
    show_progress_bar $percentage

    # Simulate progress from 10% to 80% over approximately 14 minutes (2 minutes per 10%)
    while kill -0 $pid 2>/dev/null; do
        if [ $percentage -lt 80 ]; then
            sleep 120  # 2 minutes
            percentage=$((percentage + 10))
            show_progress_bar $percentage
        else
            show_progress_bar $percentage
            sleep 1
        fi
    done

    # Command finished, show 100%
    show_progress_bar 100
    echo

    # Show cursor
    tput cnorm
}

# Function to create EROFS with exclusions
create_erofs() {
    local input_dir="$1"
    local output_file="$2"

    print_info "Creating EROFS image with LZMA compression..."

    # Create output directory
    mkdir -p "$OUTPUT_DIR"

    sudo cp -a /boot /home/$USER/clone_system_temp/ > /dev/null 2>&1

    # Start EROFS creation in background with output redirected to /dev/null
    sudo mkfs.erofs \
        -d9 \
        -zlzma,level=109,dictsize=1048576 \
        -C1048576 \
        --exclude-path=home/$USER/clone_system_temp \
        --exclude-path=home/$USER/Downloads/clone \
        --exclude-path=home/$USER/Downloads/clone/rootfs.erofs \
        --exclude-path=home/$USER/clone_system_temp/etc/udev/rules.d/70-persistent-cd.rules \
        --exclude-path=home/$USER/clone_system_temp/etc/udev/rules.d/70-persistent-net.rules \
        --exclude-path=home/$USER/clone_system_temp/etc/mtab \
        --exclude-path=home/$USER/clone_system_temp/etc/fstab \
        --exclude-path=home/$USER/clone_system_temp/dev/* \
        --exclude-path=home/$USER/clone_system_temp/proc/* \
        --exclude-path=home/$USER/clone_system_temp/sys/* \
        --exclude-path=home/$USER/clone_system_temp/tmp/* \
        --exclude-path=home/$USER/clone_system_temp/run/* \
        --exclude-path=home/$USER/clone_system_temp/mnt/* \
        --exclude-path=home/$USER/clone_system_temp/lost+found \
        --exclude-path=home/$USER/clone_system_temp/clone \
        "$output_file" \
        "$input_dir" > /dev/null 2>&1 &

    # Get the PID of the background process
    local erofs_pid=$!

    # Start progress bar simulation
    progress_bar_simulation $erofs_pid

    # Wait for the background process to complete and get its exit status
    wait $erofs_pid
    local exit_status=$?

    if [[ $exit_status -eq 0 ]]; then
        print_success "EROFS image created successfully: $output_file"

        # Show file size in green
        print_info "Image size:"
        echo -e "${GREEN}$(sudo du -h "$output_file" | cut -f1)${RESET}"
    else
        print_error "Failed to create EROFS image"
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
    echo -e "${GREEN}$(df -h / | tail -1)${RESET}"
}

# Main execution
main() {
    print_info "Starting system clone and EROFS creation using bind mount..."

    # Check if not running as root
    check_root

    # Show initial disk usage
    show_disk_usage
    echo

    # Setup bind mount
    setup_bind_mount
    echo

    # Create EROFS image directly from bind mount with exclusions
    FINAL_IMG_PATH="$OUTPUT_DIR/$FINAL_IMG_NAME"
    create_erofs "$CLONE_DIR" "$FINAL_IMG_PATH"
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
