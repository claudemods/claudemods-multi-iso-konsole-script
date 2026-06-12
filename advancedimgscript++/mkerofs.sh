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
FINAL_IMG_NAME="rootfs.erofs"

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

# Function to check dependencies
check_dependencies() {
    print_info "Checking dependencies..."
    
    local missing_deps=()
    
    if ! command -v mkfs.erofs &> /dev/null; then
        missing_deps+=("erofs-utils")
    fi
    
    if ! command -v sudo &> /dev/null; then
        missing_deps+=("sudo")
    fi
    
    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_info "Install with: sudo apt install ${missing_deps[*]}"
        print_info "Or for Arch: sudo pacman -S erofs-utils"
        exit 1
    fi
    
    # Check if EROFS is supported by the kernel
    if ! grep -q erofs /proc/filesystems; then
        print_warning "EROFS not detected in kernel. It might need module loading or kernel support."
        print_info "Try: sudo modprobe erofs"
    else
        print_success "EROFS kernel support detected"
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

# Function to create EROFS with exclusions
create_erofs() {
    local input_dir="$1"
    local output_file="$2"

    print_info "Creating EROFS image with MicroLZMA compression..."

    # Create output directory
    mkdir -p "$OUTPUT_DIR"

    # Create exclude list file for mkfs.erofs
    local exclude_file="/tmp/erofs_exclude_$$.txt"
    cat > "$exclude_file" << 'EOF'
dev
proc
sys
tmp
run
mnt
media
lost+found
clone_system_temp
EOF
    
    # Add persistent rules to exclude
    find "$input_dir/etc/udev/rules.d/" -name "70-persistent-*.rules" 2>/dev/null | \
        sed "s|^$input_dir/||" >> "$exclude_file"
    
    # Add fstab and mtab
    echo "etc/fstab" >> "$exclude_file"
    echo "etc/mtab" >> "$exclude_file"

    print_info "Excluding:"
    cat "$exclude_file" | while read line; do
        echo "  - /$line"
    done

    # Create EROFS image with MicroLZMA compression
    # -zlz4hc,16: LZ4HC compression (level 16) for metadata
    # -C65536: Maximum compression cluster size (64K)
    # -Ex: MicroLZMA compression for data
    # --exclude-path: paths to exclude
    sudo mkfs.erofs \
        -zlz4hc,16 \
        -C65536 \
        -Ex \
        --exclude-path="$exclude_file" \
        "$output_file" \
        "$input_dir"

    local exit_code=$?
    rm -f "$exclude_file"

    if [[ $exit_code -eq 0 ]]; then
        print_success "EROFS image created successfully: $output_file"

        # Show file size and compression info
        print_info "Image details:"
        sudo du -h "$output_file" | cut -f1
        print_info "Original size (approx):"
        sudo du -sh "$input_dir" 2>/dev/null | cut -f1
        
        # Show EROFS info
        print_info "EROFS filesystem info:"
        sudo fsck.erofs --extract "$output_file" --help &>/dev/null || true
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

    # Remove temporary exclude file if exists
    rm -f "/tmp/erofs_exclude_$$.txt"

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

# Function to show mount instructions
show_mount_instructions() {
    print_info "To mount the EROFS image:"
    echo -e "${GREEN}  sudo mkdir -p /mnt/erofs"
    echo -e "  sudo mount -t erofs -o ro,loop $FINAL_IMG_PATH /mnt/erofs"
    echo -e "  # To unmount:"
    echo -e "  sudo umount /mnt/erofs${RESET}"
    
    print_info "To verify the image:"
    echo -e "${GREEN}  sudo fsck.erofs $FINAL_IMG_PATH${RESET}"
}

# Main execution
main() {
    print_info "Starting system clone and EROFS creation using bind mount..."
    print_info "Compression: MicroLZMA (LZ4HC for metadata)"

    # Check if not running as root
    check_root

    # Check dependencies
    check_dependencies
    echo

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

    # Show mount instructions
    show_mount_instructions
    echo

    print_success "Process completed successfully!"
    print_info "Final image: $FINAL_IMG_PATH"
    print_info "Checksum: ${FINAL_IMG_PATH}.sha512"
}

# Handle script interruption
trap 'print_error "Script interrupted. Cleaning up..."; cleanup; exit 1' INT TERM

# Run main function
main