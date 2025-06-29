#!/bin/bash

# Constants
MAX_PATH=4096
MAX_CMD=16384
BLUE="\033[34m"
GREEN="\033[32m"
RED="\033[31m"
RESET="\033[0m"
COLOR_CYAN="\033[38;2;0;255;255m"
COLOR_GOLD="\033[38;2;36;255;255m"
PASSWORD_MAX=100
COLOR_RESET="\033[0m"

# Paths
VERSION_FILE="/home/$USER/.config/cmi/version.txt"
GUIDE_PATH="/home/$USER/.config/cmi/readme.txt"
CHANGELOG_PATH="/home/$USER/.config/cmi/changelog.txt"
CLONE_DIR_FILE="/home/$USER/.config/cmi/clonedir.txt"
ISO_NAME_FILE="/home/$USER/.config/cmi/isoname.txt"

# Function to expand paths with $USER
expand_path() {
    local path="$1"
    echo "${path/\$USER/$USER}"
}

# Get CMI version
get_cmi_version() {
    local expanded_path=$(expand_path "$VERSION_FILE")
    if [ -f "$expanded_path" ]; then
        head -n 1 "$expanded_path"
    else
        echo "unknown"
    fi
}

# Get kernel version
get_kernel_version() {
    uname -r
}

# Get vmlinuz version
get_vmlinuz_version() {
    local vmlinuz=$(ls /boot/vmlinuz* 2>/dev/null | head -n 1)
    if [ -n "$vmlinuz" ]; then
        basename "$vmlinuz"
    else
        echo "unknown"
    fi
}

# Get distro version
get_distro_version() {
    if [ -f "/etc/os-release" ]; then
        source /etc/os-release
        echo "$VERSION_ID"
    else
        echo "unknown"
    fi
}

# Detect distro
detect_distro() {
    if [ -f "/etc/os-release" ]; then
        source /etc/os-release
        case "$ID" in
            arch) echo "ARCH" ;;
            ubuntu) echo "UBUNTU" ;;
            debian) echo "DEBIAN" ;;
            cachyos) echo "CACHYOS" ;;
            neon) echo "NEON" ;;
            *) echo "UNKNOWN" ;;
        esac
    else
        echo "UNKNOWN"
    fi
}

# Get distro name
get_distro_name() {
    local distro=$1
    case "$distro" in
        ARCH) echo "Arch Linux" ;;
        UBUNTU) echo "Ubuntu" ;;
        DEBIAN) echo "Debian" ;;
        CACHYOS) echo "CachyOS" ;;
        NEON) echo "KDE Neon" ;;
        *) echo "Unknown" ;;
    esac
}

# Execute command with error handling
execute_command() {
    local cmd="$1"
    if ! eval "$cmd"; then
        echo "Command failed: $cmd"
        exit 1
    fi
}

# Read clone directory
read_clone_dir() {
    local expanded_path=$(expand_path "$CLONE_DIR_FILE")
    if [ -f "$expanded_path" ]; then
        local dir_path=$(head -n 1 "$expanded_path")
        if [ -n "$dir_path" ]; then
            if [[ "$dir_path" != */ ]]; then
                dir_path="$dir_path/"
            fi
            echo "$dir_path"
        fi
    fi
}

# Check if directory exists
dir_exists() {
    [ -d "$1" ]
}

# Save clone directory
save_clone_dir() {
    local dir_path="$1"
    local config_dir=$(expand_path "/home/$USER/.config/cmi")

    if ! dir_exists "$config_dir"; then
        execute_command "mkdir -p \"$config_dir\""
    fi

    local full_clone_path="$dir_path"
    if [[ "$full_clone_path" != */ ]]; then
        full_clone_path="$full_clone_path/"
    fi

    local file_path="$config_dir/clonedir.txt"
    echo "$full_clone_path" > "$file_path"

    local clone_folder="${full_clone_path}clone_system_temp"
    if ! dir_exists "$clone_folder"; then
        execute_command "mkdir -p \"$clone_folder\""
    fi
}

# Save ISO name
save_iso_name() {
    local name="$1"
    local config_dir=$(expand_path "/home/$USER/.config/cmi")

    if ! dir_exists "$config_dir"; then
        execute_command "mkdir -p \"$config_dir\""
    fi

    local file_path="$config_dir/isoname.txt"
    echo "$name" > "$file_path"
}

# Get ISO name
get_iso_name() {
    local expanded_path=$(expand_path "$ISO_NAME_FILE")
    if [ -f "$expanded_path" ]; then
        head -n 1 "$expanded_path"
    fi
}

# Check if init is generated
is_init_generated() {
    local distro=$1
    local init_path

    if [ "$distro" = "UBUNTU" ] || [ "$distro" = "DEBIAN" ]; then
        local distro_name="noble"
        [ "$distro" = "DEBIAN" ] && distro_name="debian"
        init_path=$(expand_path "/home/$USER/.config/cmi/build-image-${distro_name}/live/initrd.img-$(get_kernel_version)")
    else
        init_path=$(expand_path "/home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img")
    fi

    [ -f "$init_path" ]
}

# Install dependencies
install_dependencies() {
    local distro=$1
    case "$distro" in
        ARCH|CACHYOS)
            execute_command "sudo pacman -S --needed --noconfirm arch-install-scripts bash-completion dosfstools erofs-utils findutils grub jq libarchive libisoburn lsb-release lvm2 mkinitcpio-archiso mkinitcpio-nfs-utils mtools nbd pacman-contrib parted procps-ng pv python rsync squashfs-tools sshfs syslinux xdg-utils bash-completion zsh-completions kernel-modules-hook virt-manager"
            ;;
        UBUNTU|NEON)
            execute_command "sudo apt install -y cryptsetup dmeventd isolinux libaio-dev libcares2 libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot live-boot-doc live-boot-initramfs-tools live-config-systemd live-tools lvm2 pxelinux syslinux syslinux-common thin-provisioning-tools squashfs-tools xorriso"
            ;;
        DEBIAN)
            execute_command "sudo apt install -y cryptsetup dmeventd isolinux libaio1 libc-ares2 libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot live-boot-doc live-boot-initramfs-tools live-config-systemd live-tools lvm2 pxelinux syslinux syslinux-common thin-provisioning-tools squashfs-tools xorriso"
            ;;
        *)
            exit 1
            ;;
    esac
}

# Generate initrd
generate_initrd() {
    local distro=$1
    case "$distro" in
        ARCH|CACHYOS)
            execute_command "cd $(expand_path "/home/$USER/.config/cmi/build-image-arch") >/dev/null 2>&1 && sudo mkinitcpio -c live.conf -g $(expand_path "/home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img")"
            execute_command "sudo cp /boot/vmlinuz-linux* $(expand_path "/home/$USER/.config/cmi/build-image-arch/live/") 2>/dev/null"
            ;;
        UBUNTU|NEON)
            execute_command "sudo mkinitramfs -o \"$(expand_path "/home/$USER/.config/cmi/build-image-noble/live/initrd.img-$(uname -r)")\" \"$(uname -r)\""
            execute_command "sudo cp /boot/vmlinuz* $(expand_path "/home/$USER/.config/cmi/build-image-noble/live/") 2>/dev/null"
            ;;
        DEBIAN)
            execute_command "sudo mkinitramfs -o \"$(expand_path "/home/$USER/.config/cmi/build-image-debian/live/initrd.img-$(uname -r)")\" \"$(uname -r)\""
            execute_command "sudo cp /boot/vmlinuz* $(expand_path "/home/$USER/.config/cmi/build-image-debian/live/") 2>/dev/null"
            ;;
        *)
            exit 1
            ;;
    esac
}

# Edit isolinux config
edit_isolinux_cfg() {
    local distro=$1
    case "$distro" in
        ARCH|CACHYOS)
            execute_command "nano $(expand_path "/home/$USER/.config/cmi/build-image-arch/isolinux/isolinux.cfg")"
            ;;
        UBUNTU|NEON)
            execute_command "nano $(expand_path "/home/$USER/.config/cmi/build-image-noble/isolinux/isolinux.cfg")"
            ;;
        DEBIAN)
            execute_command "nano $(expand_path "/home/$USER/.config/cmi/build-image-debian/isolinux/isolinux.cfg")"
            ;;
        *)
            exit 1
            ;;
    esac
}

# Edit GRUB config
edit_grub_cfg() {
    local distro=$1
    case "$distro" in
        ARCH|CACHYOS)
            execute_command "nano $(expand_path "/home/$USER/.config/cmi/build-image-arch/boot/grub/grub.cfg")"
            ;;
        UBUNTU|NEON)
            execute_command "nano $(expand_path "/home/$USER/.config/cmi/build-image-noble/boot/grub/grub.cfg")"
            ;;
        DEBIAN)
            execute_command "nano $(expand_path "/home/$USER/.config/cmi/build-image-debian/boot/grub/grub.cfg")"
            ;;
        *)
            exit 1
            ;;
    esac
}

# Install Calamares
install_calamares() {
    local distro=$1
    case "$distro" in
        ARCH)
            execute_command "cd $(expand_path "/home/$USER/.config/cmi/calamares-per-distro/arch") >/dev/null 2>&1 && sudo pacman -U calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar ckbcomp-1.227-2-any.pkg.tar.zst"
            ;;
        CACHYOS)
            execute_command "sudo pacman -U --noconfirm calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar ckbcomp-1.227-2-any.pkg.tar.zst"
            ;;
        UBUNTU|NEON)
            execute_command "sudo apt install -y calamares-settings-ubuntu-common calamares"
            ;;
        DEBIAN)
            execute_command "sudo apt install -y calamares-settings-debian calamares"
            ;;
        *)
            exit 1
            ;;
    esac
}

# Edit Calamares branding
edit_calamares_branding() {
    execute_command "sudo nano /usr/share/calamares/branding/claudemods/branding.desc"
}

# Clone system
clone_system() {
    local clone_dir="$1"
    local full_clone_path="$clone_dir"

    if [[ "$full_clone_path" != */ ]]; then
        full_clone_path="$full_clone_path/"
    fi
    full_clone_path="${full_clone_path}clone_system_temp"

    if ! dir_exists "$full_clone_path"; then
        execute_command "mkdir -p \"$full_clone_path\""
    fi

    local command="sudo rsync -aHAXSr --numeric-ids --info=progress2 \
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
    --include=etc \
    / \"$full_clone_path\""

    execute_command "$command"
}

# Create squashfs
create_squashfs() {
    local distro=$1
    local clone_dir=$(read_clone_dir)
    if [ -z "$clone_dir" ]; then
        exit 1
    fi

    local full_clone_path="$clone_dir"
    if [[ "$full_clone_path" != */ ]]; then
        full_clone_path="$full_clone_path/"
    fi
    full_clone_path="${full_clone_path}clone_system_temp"

    local output_path
    if [ "$distro" = "UBUNTU" ] || [ "$distro" = "NEON" ]; then
        output_path=$(expand_path "/home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs")
    elif [ "$distro" = "DEBIAN" ]; then
        output_path=$(expand_path "/home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs")
    else
        output_path=$(expand_path "/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs")
    fi

    local command="sudo mksquashfs \"$full_clone_path\" \"$output_path\" \
    -comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery \
    -always-use-fragments -wildcards -xattrs"

    execute_command "$command"
}

# Create squashfs from clone
create_squashfs_clone() {
    local distro=$1
    local clone_dir=$(read_clone_dir)
    if [ -z "$clone_dir" ]; then
        exit 1
    fi

    if ! dir_exists "${clone_dir}/clone_system_temp"; then
        clone_system "$clone_dir"
    fi
    create_squashfs "$distro"
}

# Delete clone
delete_clone() {
    local distro=$1
    local clone_dir=$(read_clone_dir)
    if [ -z "$clone_dir" ]; then
        exit 1
    fi

    local full_clone_path="$clone_dir"
    if [[ "$full_clone_path" != */ ]]; then
        full_clone_path="$full_clone_path/"
    fi
    full_clone_path="${full_clone_path}clone_system_temp"

    execute_command "sudo rm -rf \"$full_clone_path\""

    local squashfs_path
    if [ "$distro" = "UBUNTU" ] || [ "$distro" = "NEON" ]; then
        squashfs_path=$(expand_path "/home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs")
    elif [ "$distro" = "DEBIAN" ]; then
        squashfs_path=$(expand_path "/home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs")
    else
        squashfs_path=$(expand_path "/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs")
    fi

    if [ -f "$squashfs_path" ]; then
        execute_command "sudo rm -f \"$squashfs_path\""
    fi
}

# Create ISO
create_iso() {
    local distro=$1
    local output_dir="$2"
    local iso_name=$(get_iso_name)
    if [ -z "$iso_name" ]; then
        exit 1
    fi

    if [ -z "$output_dir" ]; then
        exit 1
    fi

    local timestamp=$(date +"%Y-%m-%d_%H%M")
    local iso_file_name="${output_dir}/${iso_name}_amd64_${timestamp}.iso"

    local build_image_dir
    if [ "$distro" = "UBUNTU" ] || [ "$distro" = "NEON" ]; then
        build_image_dir=$(expand_path "/home/$USER/.config/cmi/build-image-noble")
    elif [ "$distro" = "DEBIAN" ]; then
        build_image_dir=$(expand_path "/home/$USER/.config/cmi/build-image-debian")
    else
        build_image_dir=$(expand_path "/home/$USER/.config/cmi/build-image-arch")
    fi

    if ! dir_exists "$output_dir"; then
        execute_command "mkdir -p \"$output_dir\""
    fi

    local xorriso_command="sudo xorriso -as mkisofs -o \"$iso_file_name\" -V 2025 -iso-level 3"

    if [ "$distro" = "UBUNTU" ] || [ "$distro" = "NEON" ]; then
        xorriso_command+=" -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin"
        xorriso_command+=" -c isolinux/boot.cat"
        xorriso_command+=" -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
        xorriso_command+=" -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot -isohybrid-gpt-basdat"
    elif [ "$distro" = "DEBIAN" ]; then
        xorriso_command+=" -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin"
        xorriso_command+=" -c isolinux/boot.cat"
        xorriso_command+=" -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
        xorriso_command+=" -eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat"
    else
        xorriso_command+=" -isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin"
        xorriso_command+=" -c isolinux/boot.cat"
        xorriso_command+=" -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
        xorriso_command+=" -eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat"
    fi

    xorriso_command+=" \"$build_image_dir\""
    execute_command "$xorriso_command"
}

# Run QEMU
run_qemu() {
    execute_command "ruby /opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb"
}

# Install updater
install_updater() {
    execute_command "$(expand_path "./home/$USER/.config/cmi/patch.sh")"
}

# Show guide
show_guide() {
    execute_command "cat $(expand_path "$GUIDE_PATH")"
}

# Show changelog
show_changelog() {
    execute_command "cat $(expand_path "$CHANGELOG_PATH")"
}

# Show status
show_status() {
    local distro=$1
    local distro_name=$(get_distro_name "$distro")
    local distro_version=$(get_distro_version)
    local cmi_version=$(get_cmi_version)
    local kernel_version=$(get_kernel_version)
    local vmlinuz_version=$(get_vmlinuz_version)
    local clone_dir=$(read_clone_dir)
    local iso_name=$(get_iso_name)
    local init_generated=$(is_init_generated "$distro" && echo "Generated" || echo "Not generated")
    local init_color=$(is_init_generated "$distro" && echo "$GREEN" || echo "$RED")
    local clone_color=$([ -z "$clone_dir" ] && echo "$RED" || echo "$GREEN")
    local iso_color=$([ -z "$iso_name" ] && echo "$RED" || echo "$GREEN")
    local clone_display=$([ -z "$clone_dir" ] && echo "Not set" || echo "$clone_dir")
    local iso_display=$([ -z "$iso_name" ] && echo "Not set" || echo "$iso_name")

    echo -e "Current Distribution: ${COLOR_CYAN}${distro_name} ${distro_version}${RESET}"
    echo -e "CMI Version: ${COLOR_CYAN}${cmi_version}${RESET}"
    echo -e "Kernel Version: ${COLOR_CYAN}${kernel_version}${RESET}"
    echo -e "vmlinuz Version: ${COLOR_CYAN}${vmlinuz_version}${RESET}"
    echo -e "Clone Directory: ${clone_color}${clone_display}${RESET}"
    echo -e "Initramfs: ${init_color}${init_generated}${RESET}"
    echo -e "ISO Name: ${iso_color}${iso_display}${RESET}"
}

# Print usage
print_usage() {
    echo -e "Usage: cmi <command> [options]\n\nAvailable commands:"
    echo -e "${COLOR_CYAN}  install-deps            - Install required dependencies"
    echo -e "  gen-init                - Generate initramfs"
    echo -e "  set-iso-name <name>     - Set ISO output name"
    echo -e "  edit-isolinux           - Edit isolinux configuration"
    echo -e "  edit-grub               - Edit GRUB configuration"
    echo -e "  set-clone-dir <path>    - Set directory for system cloning"
    echo -e "  install-calamares       - Install Calamares installer"
    echo -e "  edit-branding           - Edit Calamares branding"
    echo -e "  install-updater         - Install updater"
    echo -e "  create-squashfs         - Create squashfs from current system"
    echo -e "  create-squashfs-clone   - Clone system and create squashfs"
    echo -e "  delete-clone            - Delete cloned system"
    echo -e "  create-iso <out_dir>    - Create ISO image"
    echo -e "  run-qemu                - Run QEMU with the ISO"
    echo -e "  status                  - Show current status"
    echo -e "  guide                   - Show user guide"
    echo -e "  changelog               - Show changelog"
    echo -e "  mainmenu                - Launch main menu (cmitui)${RESET}"
}

# Main function
main() {
    local distro=$(detect_distro)

    if [ $# -lt 1 ]; then
        show_status "$distro"
        echo
        print_usage
        return 0
    fi

    local command="$1"

    case "$command" in
        "install-deps")
            install_dependencies "$distro"
            ;;
        "gen-init")
            generate_initrd "$distro"
            ;;
        "set-iso-name")
            if [ $# -lt 2 ]; then
                exit 1
            fi
            save_iso_name "$2"
            ;;
        "edit-isolinux")
            edit_isolinux_cfg "$distro"
            ;;
        "edit-grub")
            edit_grub_cfg "$distro"
            ;;
        "set-clone-dir")
            if [ $# -lt 2 ]; then
                exit 1
            fi
            save_clone_dir "$2"
            ;;
        "install-calamares")
            install_calamares "$distro"
            ;;
        "edit-branding")
            edit_calamares_branding
            ;;
        "install-updater")
            install_updater
            ;;
        "create-squashfs")
            create_squashfs "$distro"
            ;;
        "create-squashfs-clone")
            create_squashfs_clone "$distro"
            ;;
        "delete-clone")
            delete_clone "$distro"
            ;;
        "create-iso")
            if [ $# -lt 2 ]; then
                exit 1
            fi
            create_iso "$distro" "$2"
            ;;
        "run-qemu")
            run_qemu
            ;;
        "status")
            show_status "$distro"
            ;;
        "guide")
            show_guide
            ;;
        "changelog")
            show_changelog
            ;;
        "mainmenu")
            execute_command "cmitui"
            ;;
        *)
            show_status "$distro"
            echo
            print_usage
            exit 1
            ;;
    esac
}

# Call main function with all arguments
main "$@"
