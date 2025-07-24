#!/bin/bash

# Configuration variables
ORIG_IMG_NAME="system.img"
COMPRESSED_IMG_NAME="rootfs.img"
MOUNT_POINT="/mnt/btrfs_temp"
SOURCE_DIR="/"
COMPRESSION_LEVEL="22"
SQUASHFS_COMPRESSION="zstd"
SQUASHFS_COMPRESSION_ARGS=("-Xcompression-level" "22")
USERNAME=$(whoami)
BUILD_DIR="/home/$USERNAME/.config/cmi/build-image-arch-img"
BTRFS_LABEL="LIVE_SYSTEM"
BTRFS_COMPRESSION="zstd"

# Dependencies list
DEPENDENCIES=(
    "rsync"
    "squashfs-tools"
    "xorriso"
    "grub"
    "dosfstools"
    "unzip"
    "nano"
    "arch-install-scripts"
    "bash-completion"
    "erofs-utils"
    "findutils"
    "unzip"
    "jq"
    "libarchive"
    "libisoburn"
    "lsb-release"
    "lvm2"
    "mkinitcpio-archiso"
    "mkinitcpio-nfs-utils"
    "mtools"
    "nbd"
    "pacman-contrib"
    "parted"
    "procps-ng"
    "pv"
    "python"
    "sshfs"
    "syslinux"
    "xdg-utils"
    "zsh-completions"
    "kernel-modules-hook"
    "virt-manager"
    "qt6-tools"
    "qt5-tools"
)

# ANSI color codes
COLOR_RED="\033[31m"
COLOR_GREEN="\033[32m"
COLOR_BLUE="\033[34m"
COLOR_CYAN="\033[38;2;0;255;255m"
COLOR_YELLOW="\033[33m"
COLOR_RESET="\033[0m"
COLOR_HIGHLIGHT="\033[38;2;0;255;255m"
COLOR_NORMAL="\033[34m"

# Configuration state
declare -A CONFIG=(
    ["isoTag"]=""
    ["isoName"]=""
    ["outputDir"]=""
    ["vmlinuzPath"]=""
    ["mkinitcpioGenerated"]="false"
    ["grubEdited"]="false"
    ["dependenciesInstalled"]="false"
)

# Function to get current timestamp
current_time_str() {
    date +"%d/%m/%Y %H:%M:%S"
}

# Function to print checkbox
printCheckbox() {
    if [[ "$1" == "true" ]]; then
        echo -e "${COLOR_GREEN}[✓]${COLOR_RESET}"
    else
        echo -e "${COLOR_RED}[ ]${COLOR_RESET}"
    fi
}

# Function to execute command with error handling
execute_command() {
    local cmd="$1"
    local continueOnError="${2:-false}"
    
    echo -e "${COLOR_CYAN}"
    eval "$cmd"
    local status=$?
    echo -e "${COLOR_RESET}"
    
    if [ $status -ne 0 ] && [ "$continueOnError" == "false" ]; then
        echo -e "${COLOR_RED}Error executing: $cmd${COLOR_RESET}" >&2
        exit 1
    elif [ $status -ne 0 ]; then
        echo -e "${COLOR_YELLOW}Command failed but continuing: $cmd${COLOR_RESET}" >&2
    fi
}

# Function to get user input
getUserInput() {
    echo -ne "${COLOR_GREEN}$1${COLOR_RESET}"
    read -r input
    echo "$input"
}

# Function to clear screen
clearScreen() {
    echo -e "\033[2J\033[1;1H"
}

# Function to print banner
printBanner() {
    clearScreen

    echo -e "${COLOR_RED}
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚══════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
${COLOR_RESET}"
    echo -e "${COLOR_CYAN} Advanced Bash Arch Img Iso Script Beta v2.01 24-07-2025${COLOR_RESET}"

    # Show current UK time
    echo -e "${COLOR_BLUE}Current UK Time: ${COLOR_CYAN}$(current_time_str)${COLOR_RESET}"

    # Show disk usage information
    echo -e "${COLOR_GREEN}Filesystem      Size  Used Avail Use% Mounted on${COLOR_RESET}"
    execute_command "df -h / | tail -1"
    echo
}

# Function to print configuration status
printConfigStatus() {
    echo -e "${COLOR_CYAN}Current Configuration:${COLOR_RESET}"

    echo -n " "
    printCheckbox "${CONFIG[dependenciesInstalled]}"
    echo " Dependencies Installed"

    echo -n " "
    printCheckbox "${CONFIG[isoTag]}"
    echo -n " ISO Tag: "
    if [ -z "${CONFIG[isoTag]}" ]; then
        echo -e "${COLOR_YELLOW}Not set${COLOR_RESET}"
    else
        echo -e "${COLOR_CYAN}${CONFIG[isoTag]}${COLOR_RESET}"
    fi

    echo -n " "
    printCheckbox "${CONFIG[isoName]}"
    echo -n " ISO Name: "
    if [ -z "${CONFIG[isoName]}" ]; then
        echo -e "${COLOR_YELLOW}Not set${COLOR_RESET}"
    else
        echo -e "${COLOR_CYAN}${CONFIG[isoName]}${COLOR_RESET}"
    fi

    echo -n " "
    printCheckbox "${CONFIG[outputDir]}"
    echo -n " Output Directory: "
    if [ -z "${CONFIG[outputDir]}" ]; then
        echo -e "${COLOR_YELLOW}Not set${COLOR_RESET}"
    else
        echo -e "${COLOR_CYAN}${CONFIG[outputDir]}${COLOR_RESET}"
    fi

    echo -n " "
    printCheckbox "${CONFIG[vmlinuzPath]}"
    echo -n " vmlinuz Selected: "
    if [ -z "${CONFIG[vmlinuzPath]}" ]; then
        echo -e "${COLOR_YELLOW}Not selected${COLOR_RESET}"
    else
        echo -e "${COLOR_CYAN}${CONFIG[vmlinuzPath]}${COLOR_RESET}"
    fi

    echo -n " "
    printCheckbox "${CONFIG[mkinitcpioGenerated]}"
    echo " mkinitcpio Generated"

    echo -n " "
    printCheckbox "${CONFIG[grubEdited]}"
    echo " GRUB Config Edited"
}

# Function to get config file path
getConfigFilePath() {
    echo "/home/$USERNAME/.config/cmi/configuration.txt"
}

# Function to save configuration
saveConfig() {
    local configPath=$(getConfigFilePath)
    {
        echo "isoTag=${CONFIG[isoTag]}"
        echo "isoName=${CONFIG[isoName]}"
        echo "outputDir=${CONFIG[outputDir]}"
        echo "vmlinuzPath=${CONFIG[vmlinuzPath]}"
        echo "mkinitcpioGenerated=${CONFIG[mkinitcpioGenerated]}"
        echo "grubEdited=${CONFIG[grubEdited]}"
        echo "dependenciesInstalled=${CONFIG[dependenciesInstalled]}"
    } > "$configPath"
}

# Function to load configuration
loadConfig() {
    local configPath=$(getConfigFilePath)
    if [ -f "$configPath" ]; then
        while IFS='=' read -r key value; do
            if [[ -n $key && $key != "#"* ]]; then
                CONFIG["$key"]="$value"
            fi
        done < "$configPath"
    fi
}

# Function to check if ready for ISO creation
isReadyForISO() {
    [[ -n "${CONFIG[isoTag]}" && -n "${CONFIG[isoName]}" && -n "${CONFIG[outputDir]}" && \
    -n "${CONFIG[vmlinuzPath]}" && "${CONFIG[mkinitcpioGenerated]}" == "true" && \
    "${CONFIG[grubEdited]}" == "true" && "${CONFIG[dependenciesInstalled]}" == "true" ]]
}

# Function to install dependencies
installDependencies() {
    echo -e "${COLOR_CYAN}\nInstalling required dependencies...\n${COLOR_RESET}"

    # Build package list string
    local packages="${DEPENDENCIES[*]}"

    local command="sudo pacman -Sy --needed --noconfirm $packages"
    execute_command "$command"

    CONFIG[dependenciesInstalled]="true"
    saveConfig
    echo -e "${COLOR_GREEN}\nDependencies installed successfully!\n${COLOR_RESET}"
}

# Function to select vmlinuz
selectVmlinuz() {
    local vmlinuzFiles=()
    while IFS= read -r -d $'\0' file; do
        vmlinuzFiles+=("$file")
    done < <(find /boot -name 'vmlinuz*' -print0 | sort -z)

    if [ ${#vmlinuzFiles[@]} -eq 0 ]; then
        echo -e "${COLOR_RED}No vmlinuz files found in /boot!${COLOR_RESET}" >&2
        return
    fi

    echo -e "${COLOR_GREEN}Available vmlinuz files:${COLOR_RESET}"
    for i in "${!vmlinuzFiles[@]}"; do
        echo -e "${COLOR_GREEN}$((i+1))) ${vmlinuzFiles[i]}${COLOR_RESET}"
    done

    local selection=$(getUserInput "Select vmlinuz file (1-${#vmlinuzFiles[@]}): ")
    if [[ "$selection" =~ ^[0-9]+$ && $selection -ge 1 && $selection -le ${#vmlinuzFiles[@]} ]]; then
        CONFIG[vmlinuzPath]="${vmlinuzFiles[$((selection-1))]}"
        echo -e "${COLOR_CYAN}Selected: ${CONFIG[vmlinuzPath]}${COLOR_RESET}"
        saveConfig
    else
        echo -e "${COLOR_RED}Invalid selection!${COLOR_RESET}" >&2
    fi
}

# Function to generate mkinitcpio
generateMkinitcpio() {
    if [ -z "${CONFIG[vmlinuzPath]}" ]; then
        echo -e "${COLOR_RED}Please select vmlinuz first!${COLOR_RESET}" >&2
        return
    fi

    if [ -z "$BUILD_DIR" ]; then
        echo -e "${COLOR_RED}Build directory not set!${COLOR_RESET}" >&2
        return
    fi

    echo -e "${COLOR_CYAN}Generating initramfs...${COLOR_RESET}"
    execute_command "cd $BUILD_DIR && sudo mkinitcpio -c mkinitcpio.conf -g $BUILD_DIR/boot/initramfs-x86_64.img"

    CONFIG[mkinitcpioGenerated]="true"
    saveConfig
    echo -e "${COLOR_GREEN}mkinitcpio generated successfully!${COLOR_RESET}"
}

# Function to edit GRUB config
editGrubCfg() {
    if [ -z "$BUILD_DIR" ]; then
        echo -e "${COLOR_RED}Build directory not set!${COLOR_RESET}" >&2
        return
    fi

    local grubCfgPath="$BUILD_DIR/boot/grub/grub.cfg"
    echo -e "${COLOR_CYAN}Editing GRUB config: $grubCfgPath${COLOR_RESET}"
    execute_command "sudo nano $grubCfgPath"

    CONFIG[grubEdited]="true"
    saveConfig
    echo -e "${COLOR_GREEN}GRUB config edited!${COLOR_RESET}"
}

# Function to set ISO tag
setIsoTag() {
    CONFIG[isoTag]=$(getUserInput "Enter ISO tag (e.g., 2025): ")
    saveConfig
}

# Function to set ISO name
setIsoName() {
    CONFIG[isoName]=$(getUserInput "Enter ISO name (e.g., claudemods.iso): ")
    saveConfig
}

# Function to set output directory
setOutputDir() {
    local defaultDir="/home/$USERNAME/Downloads"
    echo -e "${COLOR_GREEN}Current output directory: "
    if [ -z "${CONFIG[outputDir]}" ]; then
        echo -e "${COLOR_YELLOW}Not set${COLOR_RESET}"
    else
        echo -e "${COLOR_CYAN}${CONFIG[outputDir]}${COLOR_RESET}"
    fi
    echo -e "${COLOR_GREEN}Default directory: ${COLOR_CYAN}$defaultDir${COLOR_RESET}"
    
    CONFIG[outputDir]=$(getUserInput "Enter output directory (e.g., $defaultDir or \$USER/Downloads): ")

    # Replace $USER with actual username
    CONFIG[outputDir]=${CONFIG[outputDir]//\$USER/$USERNAME}

    # If empty, use default
    if [ -z "${CONFIG[outputDir]}" ]; then
        CONFIG[outputDir]="$defaultDir"
    fi

    # Create directory if it doesn't exist
    execute_command "mkdir -p ${CONFIG[outputDir]}" "true"

    saveConfig
}

# Function to show menu
showMenu() {
    local title="$1"
    local -n items="$2"
    local selected="$3"
    
    clearScreen
    printBanner
    printConfigStatus

    echo -e "${COLOR_CYAN}\n  $title${COLOR_RESET}"
    echo -e "${COLOR_CYAN}  $(printf '%*s' "${#title}" '' | tr ' ' '-')${COLOR_RESET}"

    for i in "${!items[@]}"; do
        if [ $i -eq $selected ]; then
            echo -e "${COLOR_HIGHLIGHT}➤ ${items[i]}${COLOR_RESET}"
        else
            echo -e "${COLOR_NORMAL}  ${items[i]}${COLOR_RESET}"
        fi
    done

    read -rsn1 -t 0.1 # Clear any existing input
    read -rsn1 input
    case "$input" in
        $'\x1b') # ESC sequence
            read -rsn2 -t 0.1 input
            case "$input" in
                '[A') echo "UP" ;;
                '[B') echo "DOWN" ;;
            esac
            ;;
        $'\n') echo "ENTER" ;;
        *) echo "$input" ;;
    esac
}

# Function to show filesystem menu
showFilesystemMenu() {
    local -n items="$1"
    local selected="$2"
    
    clearScreen
    printBanner

    echo -e "${COLOR_CYAN}\n  Choose filesystem type:${COLOR_RESET}"
    echo -e "${COLOR_CYAN}  ----------------------${COLOR_RESET}"

    for i in "${!items[@]}"; do
        if [ $i -eq $selected ]; then
            echo -e "${COLOR_HIGHLIGHT}➤ ${items[i]}${COLOR_RESET}"
        else
            echo -e "${COLOR_NORMAL}  ${items[i]}${COLOR_RESET}"
        fi
    done

    read -rsn1 -t 0.1 # Clear any existing input
    read -rsn1 input
    case "$input" in
        $'\x1b') # ESC sequence
            read -rsn2 -t 0.1 input
            case "$input" in
                '[A') echo "UP" ;;
                '[B') echo "DOWN" ;;
            esac
            ;;
        $'\n') echo "ENTER" ;;
        *) echo "$input" ;;
    esac
}

# Function to show setup menu
showSetupMenu() {
    local items=(
        "Install Dependencies"
        "Set ISO Tag"
        "Set ISO Name"
        "Set Output Directory"
        "Select vmlinuz"
        "Generate mkinitcpio"
        "Edit GRUB Config"
        "Back to Main Menu"
    )

    local selected=0
    local key

    while true; do
        key=$(showMenu "ISO Creation Setup Menu:" items $selected)

        case "$key" in
            "UP")
                if [ $selected -gt 0 ]; then
                    ((selected--))
                fi
                ;;
            "DOWN")
                if [ $selected -lt $((${#items[@]}-1)) ]; then
                    ((selected++))
                fi
                ;;
            "ENTER")
                case $selected in
                    0) installDependencies ;;
                    1) setIsoTag ;;
                    2) setIsoName ;;
                    3) setOutputDir ;;
                    4) selectVmlinuz ;;
                    5) generateMkinitcpio ;;
                    6) editGrubCfg ;;
                    7) return ;;
                esac

                # Pause to show result before returning to menu
                if [ $selected -ne 7 ]; then
                    echo -e "${COLOR_GREEN}\nPress any key to continue...${COLOR_RESET}"
                    read -n1 -s
                fi
                ;;
        esac
    done
}

# Function to create image file
createImageFile() {
    local size="$1"
    local filename="$2"
    local command="sudo truncate -s $size $filename"
    execute_command "$command" "true"
    return $?
}

# Function to format filesystem
formatFilesystem() {
    local fsType="$1"
    local filename="$2"

    if [ "$fsType" == "btrfs" ]; then
        echo -e "${COLOR_CYAN}Creating Btrfs filesystem with $BTRFS_COMPRESSION compression${COLOR_RESET}"

        # Create temporary rootdir
        execute_command "sudo mkdir -p $SOURCE_DIR/btrfs_rootdir" "true"

        local command="sudo mkfs.btrfs -L \"$BTRFS_LABEL\" --compress=$BTRFS_COMPRESSION --rootdir=$SOURCE_DIR/btrfs_rootdir -f $filename"
        execute_command "$command" "true"

        # Cleanup temporary rootdir
        execute_command "sudo rmdir $SOURCE_DIR/btrfs_rootdir" "true"
    else
        echo -e "${COLOR_CYAN}Formatting as ext4${COLOR_RESET}"
        local command="sudo mkfs.ext4 -F -L \"SYSTEM_BACKUP\" $filename"
        execute_command "$command" "true"
    fi
    return $?
}

# Function to mount filesystem
mountFilesystem() {
    local fsType="$1"
    local filename="$2"
    local mountPoint="$3"

    execute_command "sudo mkdir -p $mountPoint" "true"

    if [ "$fsType" == "btrfs" ]; then
        local command="sudo mount $filename $mountPoint"
        execute_command "$command" "true"
    else
        local options="loop,discard,noatime"
        local command="sudo mount -o $options $filename $mountPoint"
        execute_command "$command" "true"
    fi
    return $?
}

# Function to copy files with rsync
copyFilesWithRsync() {
    local source="$1"
    local destination="$2"
    local fsType="$3"

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
    --exclude=*rootfs1.img \
    --exclude=btrfs_temp \
    --exclude=system.img \
    --exclude=rootfs.img \
    $source $destination"
    execute_command "$command" "true"

    if [ "$fsType" == "btrfs" ]; then
        # Optimize compression after copy
        echo -e "${COLOR_CYAN}Optimizing compression...${COLOR_RESET}"
        execute_command "sudo btrfs filesystem defrag -r -v -c $BTRFS_COMPRESSION $destination" "true"

        # Shrink to minimum size
        echo -e "${COLOR_CYAN}Finalizing image...${COLOR_RESET}"
        execute_command "sudo btrfs filesystem resize max $destination" "true"
    fi

    return $?
}

# Function to unmount and cleanup
unmountAndCleanup() {
    local mountPoint="$1"
    sync
    execute_command "sudo umount $mountPoint" "true"
    execute_command "sudo rmdir $mountPoint" "true"
    return $?
}

# Function to create SquashFS
createSquashFS() {
    local inputFile="$1"
    local outputFile="$2"

    local command="sudo mksquashfs $inputFile $outputFile \
    -comp $SQUASHFS_COMPRESSION \
    $SQUASHFS_COMPRESSION_ARGS \
    -noappend"
    execute_command "$command" "true"
    return $?
}

# Function to create checksum
createChecksum() {
    local filename="$1"
    local command="md5sum $filename > $filename.md5"
    execute_command "$command" "true"
    return $?
}

# Function to print final message
printFinalMessage() {
    local fsType="$1"
    local squashfsOutput="$2"

    echo
    if [ "$fsType" == "btrfs" ]; then
        echo -e "${COLOR_CYAN}Compressed BTRFS image created successfully: $squashfsOutput${COLOR_RESET}"
    else
        echo -e "${COLOR_CYAN}Ext4 image created successfully: $squashfsOutput${COLOR_RESET}"
    fi
    echo -e "${COLOR_CYAN}Checksum file: $squashfsOutput.md5${COLOR_RESET}"
    echo -ne "${COLOR_CYAN}Size: "
    execute_command "sudo du -h $squashfsOutput | cut -f1" "true"
    echo -ne "${COLOR_RESET}"
}

# Function to delete original image
deleteOriginalImage() {
    local imgName="$1"
    echo -e "${COLOR_CYAN}Deleting original image file: $imgName${COLOR_RESET}"
    execute_command "sudo rm -f $imgName" "true"
}

# Function to get output directory
getOutputDirectory() {
    echo "/home/$USERNAME/.config/cmi/build-image-arch-img/LiveOS"
}

# Function to expand path with variables
expandPath() {
    local path="$1"
    path="${path/#~/$HOME}"
    path="${path//\$USER/$USERNAME}"
    echo "$path"
}

# Function to create ISO
createISO() {
    if ! isReadyForISO; then
        echo -e "${COLOR_RED}Cannot create ISO - setup is incomplete!${COLOR_RESET}" >&2
        return 1
    fi

    echo -e "${COLOR_CYAN}\nStarting ISO creation process...\n${COLOR_RESET}"

    local expandedOutputDir=$(expandPath "${CONFIG[outputDir]}")

    # Ensure output directory exists
    execute_command "mkdir -p $expandedOutputDir" "true"

    local xorrisoCmd="sudo xorriso -as mkisofs \
    --modification-date=\"$(date +%Y%m%d%H%M%S00)\" \
    --protective-msdos-label \
    -volid \"${CONFIG[isoTag]}\" \
    -appid \"claudemods Linux Live/Rescue CD\" \
    -publisher \"claudemods claudemods101@gmail.com >\" \
    -preparer \"Prepared by user\" \
    -r -graft-points -no-pad \
    --sort-weight 0 / \
    --sort-weight 1 /boot \
    --grub2-mbr $BUILD_DIR/boot/grub/i386-pc/boot_hybrid.img \
    -partition_offset 16 \
    -b boot/grub/i386-pc/eltorito.img \
    -c boot.catalog \
    -no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info \
    -eltorito-alt-boot \
    -append_partition 2 0xef $BUILD_DIR/boot/efi.img \
    -e --interval:appended_partition_2:all:: \
    -no-emul-boot \
    -iso-level 3 \
    -o \"$expandedOutputDir/${CONFIG[isoName]}\" \
    $BUILD_DIR"

    execute_command "$xorrisoCmd" "true"
    echo -e "${COLOR_CYAN}ISO created successfully at $expandedOutputDir/${CONFIG[isoName]}${COLOR_RESET}"
    return 0
}

# Function to show guide
showGuide() {
    local readmePath="/home/$USERNAME/.config/cmi/readme.txt"
    execute_command "mkdir -p /home/$USERNAME/.config/cmi" "true"

    # Set guide output color to cyan
    echo -e "${COLOR_CYAN}"
    execute_command "nano $readmePath" "true"
    echo -e "${COLOR_RESET}"
}

# Function to install ISO to USB
installISOToUSB() {
    if [ -z "${CONFIG[outputDir]}" ]; then
        echo -e "${COLOR_RED}Output directory not set!${COLOR_RESET}" >&2
        return
    fi

    # List available ISO files in output directory
    local expandedOutputDir=$(expandPath "${CONFIG[outputDir]}")
    local isoFiles=()
    while IFS= read -r -d $'\0' file; do
        isoFiles+=("$(basename "$file")")
    done < <(find "$expandedOutputDir" -maxdepth 1 -name "*.iso" -print0 | sort -z)

    if [ ${#isoFiles[@]} -eq 0 ]; then
        echo -e "${COLOR_RED}No ISO files found in output directory!${COLOR_RESET}" >&2
        return
    fi

    # Show available ISO files
    echo -e "${COLOR_GREEN}Available ISO files:${COLOR_RESET}"
    for i in "${!isoFiles[@]}"; do
        echo -e "${COLOR_GREEN}$((i+1))) ${isoFiles[i]}${COLOR_RESET}"
    done

    # Get user selection
    local selection=$(getUserInput "Select ISO file (1-${#isoFiles[@]}): ")
    local choice
    if [[ "$selection" =~ ^[0-9]+$ ]]; then
        choice=$((selection-1))
        if [ $choice -lt 0 ] || [ $choice -ge ${#isoFiles[@]} ]; then
            echo -e "${COLOR_RED}Invalid selection!${COLOR_RESET}" >&2
            return
        fi
    else
        echo -e "${COLOR_RED}Invalid input!${COLOR_RESET}" >&2
        return
    fi

    local selectedISO="$expandedOutputDir/${isoFiles[$choice]}"

    # Get target drive
    echo -e "${COLOR_CYAN}\nAvailable drives:${COLOR_RESET}"
    execute_command "lsblk -d -o NAME,SIZE,MODEL | grep -v 'loop'" "true"

    local targetDrive=$(getUserInput "\nEnter target drive (e.g., /dev/sda): ")
    if [ -z "$targetDrive" ]; then
        echo -e "${COLOR_RED}No drive specified!${COLOR_RESET}" >&2
        return
    fi

    # Confirm before writing
    echo -e "${COLOR_RED}\nWARNING: This will overwrite all data on $targetDrive!${COLOR_RESET}"
    local confirm=$(getUserInput "Are you sure you want to continue? (y/N): ")
    if [[ "$confirm" != "y" && "$confirm" != "Y" ]]; then
        echo -e "${COLOR_CYAN}Operation cancelled.${COLOR_RESET}"
        return
    fi

    # Write ISO to USB
    echo -e "${COLOR_CYAN}\nWriting $selectedISO to $targetDrive...${COLOR_RESET}"
    local ddCommand="sudo dd if=$selectedISO of=$targetDrive bs=4M status=progress oflag=sync"
    execute_command "$ddCommand" "true"

    echo -e "${COLOR_GREEN}\nISO successfully written to USB drive!${COLOR_RESET}"
    echo -e "${COLOR_GREEN}Press any key to continue...${COLOR_RESET}"
    read -n1 -s
}

# Function to run CMI installer
runCMIInstaller() {
    execute_command "cmiinstaller" "true"
    echo -e "${COLOR_GREEN}\nPress any key to continue...${COLOR_RESET}"
    read -n1 -s
}

# Function to show main menu
showMainMenu() {
    local items=(
        "Guide"
        "Setup Scripts"
        "Create Image"
        "Create ISO"
        "Show Disk Usage"
        "Install ISO to USB"
        "CMI BTRFS/EXT4 Installer"
        "Exit"
    )

    local selected=0
    local key

    while true; do
        key=$(showMenu "Main Menu:" items $selected)

        case "$key" in
            "UP")
                if [ $selected -gt 0 ]; then
                    ((selected--))
                fi
                ;;
            "DOWN")
                if [ $selected -lt $((${#items[@]}-1)) ]; then
                    ((selected++))
                fi
                ;;
            "ENTER")
                case $selected in
                    0) showGuide ;;
                    1) showSetupMenu ;;
                    2)
                        local outputDir=$(getOutputDirectory)
                        local outputOrigImgPath="$outputDir/$ORIG_IMG_NAME"
                        local outputCompressedImgPath="$outputDir/$COMPRESSED_IMG_NAME"

                        # Cleanup old files
                        execute_command "sudo umount $MOUNT_POINT 2>/dev/null || true" "true"
                        execute_command "sudo rm -rf $outputOrigImgPath" "true"
                        execute_command "sudo mkdir -p $MOUNT_POINT" "true"
                        execute_command "sudo mkdir -p $outputDir" "true"

                        local imgSize=$(getUserInput "Enter the image size in GB (e.g., 6 for 6GB): ")G

                        local fsOptions=("btrfs" "ext4")
                        local fsSelected=0
                        local fsKey
                        local fsChosen=false

                        while ! $fsChosen; do
                            fsKey=$(showFilesystemMenu fsOptions $fsSelected)

                            case "$fsKey" in
                                "UP")
                                    if [ $fsSelected -gt 0 ]; then
                                        ((fsSelected--))
                                    fi
                                    ;;
                                "DOWN")
                                    if [ $fsSelected -lt $((${#fsOptions[@]}-1)) ]; then
                                        ((fsSelected++))
                                    fi
                                    ;;
                                "ENTER")
                                    fsChosen=true
                                    ;;
                            esac
                        done

                        local fsType="${fsOptions[$fsSelected]}"

                        if createImageFile "$imgSize" "$outputOrigImgPath" && \
                           formatFilesystem "$fsType" "$outputOrigImgPath" && \
                           mountFilesystem "$fsType" "$outputOrigImgPath" "$MOUNT_POINT" && \
                           copyFilesWithRsync "$SOURCE_DIR" "$MOUNT_POINT" "$fsType"; then
                            unmountAndCleanup "$MOUNT_POINT"
                            createSquashFS "$outputOrigImgPath" "$outputCompressedImgPath"
                            deleteOriginalImage "$outputOrigImgPath"
                            createChecksum "$outputCompressedImgPath"
                            printFinalMessage "$fsType" "$outputCompressedImgPath"
                        fi

                        echo -e "${COLOR_GREEN}\nPress any key to continue...${COLOR_RESET}"
                        read -n1 -s
                        ;;
                    3)
                        createISO
                        echo -e "${COLOR_GREEN}\nPress any key to continue...${COLOR_RESET}"
                        read -n1 -s
                        ;;
                    4)
                        execute_command "df -h"
                        echo -e "${COLOR_GREEN}\nPress any key to continue...${COLOR_RESET}"
                        read -n1 -s
                        ;;
                    5) installISOToUSB ;;
                    6) runCMIInstaller ;;
                    7) exit 0 ;;
                esac
                ;;
        esac
    done
}

# Main function
main() {
    # Create config directory if it doesn't exist
    local configDir="/home/$USERNAME/.config/cmi"
    execute_command "mkdir -p $configDir" "true"

    # Load existing configuration
    loadConfig

    # Set terminal settings for arrow key detection
    local oldt
    oldt=$(stty -g)
    stty -echo -icanon -icrnl time 0 min 0

    showMainMenu

    # Restore terminal settings
    stty "$oldt"
}

# Start the script
main
