#!/bin/bash

# Configuration variables
ORIG_IMG_NAME="rootfs1.img"
FINAL_IMG_NAME="rootfs.img"
MOUNT_POINT="/mnt/ext4_temp"
SOURCE_DIR="/"
COMPRESSION_LEVEL="22"
SQUASHFS_COMPRESSION="zstd"
declare -a SQUASHFS_COMPRESSION_ARGS=("-Xcompression-level" "22")

# Get username
USERNAME=$(whoami)
BUILD_DIR="/home/$USERNAME/.config/cmi/build-image-arch-img"

# Dependencies list
declare -a DEPENDENCIES=(
    "rsync" "squashfs-tools" "xorriso" "grub" "dosfstools" "unzip" "nano"
    "arch-install-scripts" "bash-completion" "erofs-utils" "findutils" "unzip"
    "jq" "libarchive" "libisoburn" "lsb-release" "lvm2" "mkinitcpio-archiso"
    "mkinitcpio-nfs-utils" "mtools" "nbd" "pacman-contrib" "parted" "procps-ng"
    "pv" "python" "sshfs" "syslinux" "xdg-utils" "zsh-completions"
    "kernel-modules-hook" "virt-manager" "qt6-tools" "qt5-tools"
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
isoTag=""
isoName=""
outputDir=""
vmlinuzPath=""
cloneDir=""
mkinitcpioGenerated=false
grubEdited=false
dependenciesInstalled=false

# Time-related variables
current_time_str=""
time_thread_running=true

# Function to save configuration
saveConfig() {
    local configPath="/home/$USERNAME/.config/cmi/configuration.txt"
    mkdir -p "/home/$USERNAME/.config/cmi"
    
    cat > "$configPath" << EOF
isoTag=$isoTag
isoName=$isoName
outputDir=$outputDir
vmlinuzPath=$vmlinuzPath
cloneDir=$cloneDir
mkinitcpioGenerated=$([ "$mkinitcpioGenerated" = true ] && echo "1" || echo "0")
grubEdited=$([ "$grubEdited" = true ] && echo "1" || echo "0")
dependenciesInstalled=$([ "$dependenciesInstalled" = true ] && echo "1" || echo "0")
EOF
}

# Function to execute commands with error handling
execute_command() {
    local cmd="$1"
    local continueOnError="${2:-false}"
    
    echo -e "$COLOR_CYAN"
    eval "$cmd"
    local status=$?
    echo -e "$COLOR_RESET"
    
    if [ $status -ne 0 ] && [ "$continueOnError" = "false" ]; then
        echo -e "${COLOR_RED}Error executing: $cmd$COLOR_RESET"
        exit 1
    elif [ $status -ne 0 ]; then
        echo -e "${COLOR_YELLOW}Command failed but continuing: $cmd$COLOR_RESET"
    fi
}

# Function to print checkbox
printCheckbox() {
    local checked="$1"
    if [ "$checked" = "true" ]; then
        echo -e "${COLOR_GREEN}[✓]$COLOR_RESET"
    else
        echo -e "${COLOR_RED}[ ]$COLOR_RESET"
    fi
}

# Function to clear screen
clearScreen() {
    echo -e "\033[2J\033[1;1H"
}

# Function to get single character input
getch() {
    local oldt newt ch
    oldt=$(stty -g)
    newt=$oldt
    newt="${newt%-icanon}"
    stty "$newt"
    ch=$(dd bs=1 count=1 2>/dev/null)
    stty "$oldt"
    echo "$ch"
}

# Function to check if key is hit
kbhit() {
    local oldt newt oldf ch
    oldt=$(stty -g)
    newt=$oldt
    newt="${newt%-icanon} -echo"
    stty "$newt"
    oldf=$(fcntl 0 F_GETFL 0)
    fcntl 0 F_SETFL "$((oldf | O_NONBLOCK))" >/dev/null 2>&1
    
    ch=$(dd bs=1 count=1 2>/dev/null)
    local result=$?
    
    stty "$oldt"
    fcntl 0 F_SETFL "$oldf" >/dev/null 2>&1
    
    if [ $result -eq 0 ] && [ -n "$ch" ]; then
        echo "$ch" | cat -v
        return 0
    fi
    return 1
}

# Time update thread (simulated with background process)
update_time_thread() {
    while $time_thread_running; do
        current_time_str=$(date '+%d/%m/%Y %H:%M:%S')
        sleep 1
    done
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
$COLOR_RESET"
    echo -e "${COLOR_CYAN} Advanced Bash Arch Img Iso Script Beta v2.01 26-07-2025$COLOR_RESET"
    echo -e "${COLOR_BLUE}Current UK Time: ${COLOR_CYAN}$current_time_str$COLOR_RESET"
    echo -e "${COLOR_GREEN}Filesystem      Size  Used Avail Use% Mounted on$COLOR_RESET"
    execute_command "df -h / | tail -1"
    echo
}

# Function to print configuration status
printConfigStatus() {
    echo -e "${COLOR_CYAN}Current Configuration:$COLOR_RESET"
    
    echo -n " "
    printCheckbox "$dependenciesInstalled"
    echo " Dependencies Installed"
    
    echo -n " "
    printCheckbox "$([ -n "$isoTag" ] && echo true || echo false)"
    echo " ISO Tag: $([ -z "$isoTag" ] && echo -e "${COLOR_YELLOW}Not set$COLOR_RESET" || echo -e "${COLOR_CYAN}$isoTag$COLOR_RESET")"
    
    echo -n " "
    printCheckbox "$([ -n "$isoName" ] && echo true || echo false)"
    echo " ISO Name: $([ -z "$isoName" ] && echo -e "${COLOR_YELLOW}Not set$COLOR_RESET" || echo -e "${COLOR_CYAN}$isoName$COLOR_RESET")"
    
    echo -n " "
    printCheckbox "$([ -n "$outputDir" ] && echo true || echo false)"
    echo " Output Directory: $([ -z "$outputDir" ] && echo -e "${COLOR_YELLOW}Not set$COLOR_RESET" || echo -e "${COLOR_CYAN}$outputDir$COLOR_RESET")"
    
    echo -n " "
    printCheckbox "$([ -n "$vmlinuzPath" ] && echo true || echo false)"
    echo " vmlinuz Selected: $([ -z "$vmlinuzPath" ] && echo -e "${COLOR_YELLOW}Not selected$COLOR_RESET" || echo -e "${COLOR_CYAN}$vmlinuzPath$COLOR_RESET")"
    
    echo -n " "
    printCheckbox "$([ -n "$cloneDir" ] && echo true || echo false)"
    echo " Clone Directory: $([ -z "$cloneDir" ] && echo -e "${COLOR_YELLOW}Not set$COLOR_RESET" || echo -e "${COLOR_CYAN}$cloneDir$COLOR_RESET")"
    
    echo -n " "
    printCheckbox "$mkinitcpioGenerated"
    echo " mkinitcpio Generated"
    
    echo -n " "
    printCheckbox "$grubEdited"
    echo " GRUB Config Edited"
}

# Function to get user input
getUserInput() {
    local prompt="$1"
    echo -en "${COLOR_GREEN}$prompt$COLOR_RESET"
    
    local oldt newt
    oldt=$(stty -g)
    newt=$oldt
    newt="${newt%-icanon} -echo"
    stty "$newt"
    
    local input=""
    local ch=""
    
    while IFS= read -r -n1 ch; do
        case "$ch" in
            $'\n')
                break
                ;;
            $'\177'|$'\b')
                if [ -n "$input" ]; then
                    input="${input%?}"
                    echo -ne "\b \b"
                fi
                ;;
            *)
                if [[ "$ch" =~ [[:print:]] ]]; then
                    input+="$ch"
                    echo -n "$ch"
                fi
                ;;
        esac
    done
    
    stty "$oldt"
    echo
    echo "$input"
}

# Function to install dependencies
installDependencies() {
    echo -e "${COLOR_CYAN}\nInstalling required dependencies...\n$COLOR_RESET"
    
    local packages=""
    for pkg in "${DEPENDENCIES[@]}"; do
        packages="$packages $pkg"
    done
    
    execute_command "sudo pacman -Sy --needed --noconfirm $packages"
    
    dependenciesInstalled=true
    saveConfig
    echo -e "${COLOR_GREEN}\nDependencies installed successfully!\n$COLOR_RESET"
}

# Function to select vmlinuz
selectVmlinuz() {
    local vmlinuzFiles=()
    
    if [ -d "/boot" ]; then
        while IFS= read -r -d $'\0' file; do
            vmlinuzFiles+=("$file")
        done < <(find /boot -name "vmlinuz*" -type f -print0)
    else
        echo -e "${COLOR_RED}Could not open /boot directory$COLOR_RESET"
        return
    fi
    
    if [ ${#vmlinuzFiles[@]} -eq 0 ]; then
        echo -e "${COLOR_RED}No vmlinuz files found in /boot!$COLOR_RESET"
        return
    fi
    
    echo -e "${COLOR_GREEN}Available vmlinuz files:$COLOR_RESET"
    for i in "${!vmlinuzFiles[@]}"; do
        echo -e "${COLOR_GREEN}$((i+1))) ${vmlinuzFiles[i]}$COLOR_RESET"
    done
    
    local selection
    selection=$(getUserInput "Select vmlinuz file (1-${#vmlinuzFiles[@]}): ")
    
    if [[ "$selection" =~ ^[0-9]+$ ]] && [ "$selection" -ge 1 ] && [ "$selection" -le ${#vmlinuzFiles[@]} ]; then
        vmlinuzPath="${vmlinuzFiles[$((selection-1))]}"
        local destPath="$BUILD_DIR/boot/vmlinuz-x86_64"
        
        execute_command "sudo cp \"$vmlinuzPath\" \"$destPath\""
        
        echo -e "${COLOR_CYAN}Selected: $vmlinuzPath$COLOR_RESET"
        echo -e "${COLOR_CYAN}Copied to: $destPath$COLOR_RESET"
        saveConfig
    else
        echo -e "${COLOR_RED}Invalid selection!$COLOR_RESET"
    fi
}

# Function to generate mkinitcpio
generateMkinitcpio() {
    if [ -z "$vmlinuzPath" ]; then
        echo -e "${COLOR_RED}Please select vmlinuz first!$COLOR_RESET"
        return
    fi
    
    if [ -z "$BUILD_DIR" ]; then
        echo -e "${COLOR_RED}Build directory not set!$COLOR_RESET"
        return
    fi
    
    echo -e "${COLOR_CYAN}Generating initramfs...$COLOR_RESET"
    execute_command "cd \"$BUILD_DIR\" && sudo mkinitcpio -c mkinitcpio.conf -g \"$BUILD_DIR/boot/initramfs-x86_64.img\""
    
    mkinitcpioGenerated=true
    saveConfig
    echo -e "${COLOR_GREEN}mkinitcpio generated successfully!$COLOR_RESET"
}

# Function to edit GRUB config
editGrubCfg() {
    if [ -z "$BUILD_DIR" ]; then
        echo -e "${COLOR_RED}Build directory not set!$COLOR_RESET"
        return
    fi
    
    local grubCfgPath="$BUILD_DIR/boot/grub/grub.cfg"
    echo -e "${COLOR_CYAN}Editing GRUB config: $grubCfgPath$COLOR_RESET"
    
    execute_command "sudo env TERM=xterm-256color nano -Y cyanish \"$grubCfgPath\""
    
    grubEdited=true
    saveConfig
    echo -e "${COLOR_GREEN}GRUB config edited!$COLOR_RESET"
}

# Function to set ISO tag
setIsoTag() {
    isoTag=$(getUserInput "Enter ISO tag (e.g., 2025): ")
    saveConfig
}

# Function to set ISO name
setIsoName() {
    isoName=$(getUserInput "Enter ISO name (e.g., claudemods.iso): ")
    saveConfig
}

# Function to set output directory
setOutputDir() {
    local defaultDir="/home/$USERNAME/Downloads"
    echo -e "${COLOR_GREEN}Current output directory: $([ -z "$outputDir" ] && echo -e "${COLOR_YELLOW}Not set$COLOR_RESET" || echo -e "${COLOR_CYAN}$outputDir$COLOR_RESET")"
    echo -e "${COLOR_GREEN}Default directory: ${COLOR_CYAN}$defaultDir$COLOR_RESET"
    
    outputDir=$(getUserInput "Enter output directory (e.g., $defaultDir or \$USER/Downloads): ")
    outputDir="${outputDir//\$USER/$USERNAME}"
    
    if [ -z "$outputDir" ]; then
        outputDir="$defaultDir"
    fi
    
    execute_command "mkdir -p \"$outputDir\"" "true"
    saveConfig
}

# Function to set clone directory
setCloneDir() {
    local defaultDir="/home/$USERNAME"
    echo -e "${COLOR_GREEN}Current clone directory: $([ -z "$cloneDir" ] && echo -e "${COLOR_YELLOW}Not set$COLOR_RESET" || echo -e "${COLOR_CYAN}$cloneDir$COLOR_RESET")"
    echo -e "${COLOR_GREEN}Default directory: ${COLOR_CYAN}$defaultDir$COLOR_RESET"
    
    local parentDir
    parentDir=$(getUserInput "Enter parent directory for clone_system_temp folder (e.g., $defaultDir or \$USER): ")
    parentDir="${parentDir//\$USER/$USERNAME}"
    
    if [ -z "$parentDir" ]; then
        parentDir="$defaultDir"
    fi
    
    cloneDir="$parentDir/clone_system_temp"
    execute_command "sudo mkdir -p \"$cloneDir\"" "true"
    saveConfig
}

# Function to get config file path
getConfigFilePath() {
    echo "/home/$USERNAME/.config/cmi/configuration.txt"
}

# Function to load configuration
loadConfig() {
    local configPath
    configPath=$(getConfigFilePath)
    
    if [ -f "$configPath" ]; then
        while IFS='=' read -r key value; do
            case "$key" in
                "isoTag") isoTag="$value" ;;
                "isoName") isoName="$value" ;;
                "outputDir") outputDir="$value" ;;
                "vmlinuzPath") vmlinuzPath="$value" ;;
                "cloneDir") cloneDir="$value" ;;
                "mkinitcpioGenerated") [ "$value" = "1" ] && mkinitcpioGenerated=true || mkinitcpioGenerated=false ;;
                "grubEdited") [ "$value" = "1" ] && grubEdited=true || grubEdited=false ;;
                "dependenciesInstalled") [ "$value" = "1" ] && dependenciesInstalled=true || dependenciesInstalled=false ;;
            esac
        done < "$configPath"
    fi
}

# Function to show menu
showMenu() {
    local title="$1"
    shift
    local items=("$@")
    local selected="$2"
    
    clearScreen
    printBanner
    printConfigStatus
    
    echo -e "${COLOR_CYAN}\n  $title$COLOR_RESET"
    echo -e "${COLOR_CYAN}  $(printf '%*s' "${#title}" '' | tr ' ' '-')$COLOR_RESET"
    
    for i in "${!items[@]}"; do
        if [ "$i" -eq "$selected" ]; then
            echo -e "${COLOR_HIGHLIGHT}➤ ${items[i]}$COLOR_RESET"
        else
            echo -e "${COLOR_NORMAL}  ${items[i]}$COLOR_RESET"
        fi
    done
}

# Function to check if setup is ready for ISO
isReadyForISO() {
    [ -n "$isoTag" ] && [ -n "$isoName" ] && [ -n "$outputDir" ] && \
    [ -n "$vmlinuzPath" ] && [ "$mkinitcpioGenerated" = true ] && \
    [ "$grubEdited" = true ] && [ "$dependenciesInstalled" = true ]
}

# Function to show setup menu
showSetupMenu() {
    local items=(
        "Install Dependencies"
        "Set Clone Directory"
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
        showMenu "ISO Creation Setup Menu:" "${items[@]}" "$selected"
        
        read -rsn1 key
        case "$key" in
            $'\x1b')
                read -rsn2 -t 0.1 key
                case "$key" in
                    "[A") [ $selected -gt 0 ] && ((selected--)) ;;
                    "[B") [ $selected -lt $((${#items[@]}-1)) ] && ((selected++)) ;;
                esac
                ;;
            "")
                case $selected in
                    0) installDependencies ;;
                    1) setCloneDir ;;
                    2) setIsoTag ;;
                    3) setIsoName ;;
                    4) setOutputDir ;;
                    5) selectVmlinuz ;;
                    6) generateMkinitcpio ;;
                    7) editGrubCfg ;;
                    8) return ;;
                esac
                
                if [ $selected -ne 8 ]; then
                    echo -e "${COLOR_GREEN}\nPress any key to continue...$COLOR_RESET"
                    getch >/dev/null
                fi
                ;;
        esac
    done
}

# Function to copy files with rsync
copyFilesWithRsync() {
    local source="$1"
    local destination="$2"
    
    echo -e "${COLOR_CYAN}Copying files...$COLOR_RESET"
    
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
        \"$source/\" \"$destination/\""
    
    execute_command "$command" "true"
    return 0
}

# Function to create SquashFS
createSquashFS() {
    local inputDir="$1"
    local outputFile="$2"
    
    echo -e "${COLOR_CYAN}Creating SquashFS image, this may take some time...$COLOR_RESET"
    
    local command="sudo mksquashfs \"$inputDir\" \"$outputFile\" \
        -noappend -comp xz -b 256K -Xbcj x86"
    
    execute_command "$command" "true"
    return 0
}

# Function to create checksum
createChecksum() {
    local filename="$1"
    execute_command "sha512sum \"$filename\" > \"$filename.sha512\"" "true"
    return 0
}

# Function to print final message
printFinalMessage() {
    local outputFile="$1"
    echo
    echo -e "${COLOR_CYAN}SquashFS image created successfully: $outputFile$COLOR_RESET"
    echo -e "${COLOR_CYAN}Checksum file: $outputFile.sha512$COLOR_RESET"
    echo -ne "${COLOR_CYAN}Size: "
    execute_command "sudo du -h \"$outputFile\" | cut -f1" "true"
    echo -e "$COLOR_RESET"
}

# Function to get output directory
getOutputDirectory() {
    echo "/home/$USERNAME/.config/cmi/build-image-arch-img/LiveOS"
}

# Function to expand path
expandPath() {
    local path="$1"
    path="${path//\~/$HOME}"
    path="${path//\$USER/$USERNAME}"
    echo "$path"
}

# Function to create ISO
createISO() {
    if ! isReadyForISO; then
        echo -e "${COLOR_RED}Cannot create ISO - setup is incomplete!$COLOR_RESET"
        return 1
    fi
    
    echo -e "${COLOR_CYAN}\nStarting ISO creation process...\n$COLOR_RESET"
    
    local expandedOutputDir
    expandedOutputDir=$(expandPath "$outputDir")
    execute_command "mkdir -p \"$expandedOutputDir\"" "true"
    
    local xorrisoCmd="sudo xorriso -as mkisofs \
        --modification-date=\"\$(date +%Y%m%d%H%M%S00)\" \
        --protective-msdos-label \
        -volid \"$isoTag\" \
        -appid \"claudemods Linux Live/Rescue CD\" \
        -publisher \"claudemods claudemods101@gmail.com >\" \
        -preparer \"Prepared by user\" \
        -r -graft-points -no-pad \
        --sort-weight 0 / \
        --sort-weight 1 /boot \
        --grub2-mbr \"$BUILD_DIR/boot/grub/i386-pc/boot_hybrid.img\" \
        -partition_offset 16 \
        -b boot/grub/i386-pc/eltorito.img \
        -c boot.catalog \
        -no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info \
        -eltorito-alt-boot \
        -append_partition 2 0xef \"$BUILD_DIR/boot/efi.img\" \
        -e --interval:appended_partition_2:all:: \
        -no-emul-boot \
        -iso-level 3 \
        -o \"$expandedOutputDir/$isoName\" \
        \"$BUILD_DIR\""
    
    execute_command "$xorrisoCmd" "true"
    echo -e "${COLOR_CYAN}ISO created successfully at $expandedOutputDir/$isoName$COLOR_RESET"
    return 0
}

# Function to show guide
showGuide() {
    local readmePath="/home/$USERNAME/.config/cmi/readme.txt"
    execute_command "mkdir -p \"/home/$USERNAME/.config/cmi\"" "true"
    execute_command "nano \"$readmePath\"" "true"
}

# Function to install ISO to USB
installISOToUSB() {
    if [ -z "$outputDir" ]; then
        echo -e "${COLOR_RED}Output directory not set!$COLOR_RESET"
        return
    fi
    
    local expandedOutputDir
    expandedOutputDir=$(expandPath "$outputDir")
    local isoFiles=()
    
    if [ -d "$expandedOutputDir" ]; then
        while IFS= read -r -d $'\0' file; do
            isoFiles+=("$(basename "$file")")
        done < <(find "$expandedOutputDir" -maxdepth 1 -name "*.iso" -type f -print0)
    else
        echo -e "${COLOR_RED}Could not open output directory: $expandedOutputDir$COLOR_RESET"
        return
    fi
    
    if [ ${#isoFiles[@]} -eq 0 ]; then
        echo -e "${COLOR_RED}No ISO files found in output directory!$COLOR_RESET"
        return
    fi
    
    echo -e "${COLOR_GREEN}Available ISO files:$COLOR_RESET"
    for i in "${!isoFiles[@]}"; do
        echo -e "${COLOR_GREEN}$((i+1))) ${isoFiles[i]}$COLOR_RESET"
    done
    
    local selection
    selection=$(getUserInput "Select ISO file (1-${#isoFiles[@]}): ")
    
    if [[ "$selection" =~ ^[0-9]+$ ]] && [ "$selection" -ge 1 ] && [ "$selection" -le ${#isoFiles[@]} ]; then
        local selectedISO="$expandedOutputDir/${isoFiles[$((selection-1))]}"
        
        echo -e "${COLOR_CYAN}\nAvailable drives:$COLOR_RESET"
        execute_command "lsblk -d -o NAME,SIZE,MODEL | grep -v 'loop'" "true"
        
        local targetDrive
        targetDrive=$(getUserInput "\nEnter target drive (e.g., /dev/sda): ")
        
        if [ -z "$targetDrive" ]; then
            echo -e "${COLOR_RED}No drive specified!$COLOR_RESET"
            return
        fi
        
        echo -e "${COLOR_RED}\nWARNING: This will overwrite all data on $targetDrive!$COLOR_RESET"
        local confirm
        confirm=$(getUserInput "Are you sure you want to continue? (y/N): ")
        
        if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
            echo -e "${COLOR_CYAN}Operation cancelled.$COLOR_RESET"
            return
        fi
        
        echo -e "${COLOR_CYAN}\nWriting $selectedISO to $targetDrive...$COLOR_RESET"
        execute_command "sudo dd if=\"$selectedISO\" of=\"$targetDrive\" bs=4M status=progress oflag=sync" "true"
        
        echo -e "${COLOR_GREEN}\nISO successfully written to USB drive!$COLOR_RESET"
        echo -e "${COLOR_GREEN}Press any key to continue...$COLOR_RESET"
        getch >/dev/null
    else
        echo -e "${COLOR_RED}Invalid selection!$COLOR_RESET"
    fi
}

# Function to run CMI installer
runCMIInstaller() {
    execute_command "cmirsyncinstaller" "true"
    echo -e "${COLOR_GREEN}\nPress any key to continue...$COLOR_RESET"
    getch >/dev/null
}

# Function to update script
updateScript() {
    echo -e "${COLOR_CYAN}\nUpdating script from GitHub...$COLOR_RESET"
    execute_command "bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/claudemods/claudemods-multi-iso-konsole-script/main/advancedimgscript/installer/patch.sh)\""
    echo -e "${COLOR_GREEN}\nScript updated successfully!$COLOR_RESET"
    echo -e "${COLOR_GREEN}Press any key to continue...$COLOR_RESET"
    getch >/dev/null
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
        "Update Script"
        "Exit"
    )
    
    local selected=0
    local key
    
    while true; do
        showMenu "Main Menu:" "${items[@]}" "$selected"
        
        read -rsn1 key
        case "$key" in
            $'\x1b')
                read -rsn2 -t 0.1 key
                case "$key" in
                    "[A") [ $selected -gt 0 ] && ((selected--)) ;;
                    "[B") [ $selected -lt $((${#items[@]}-1)) ] && ((selected++)) ;;
                esac
                ;;
            "")
                case $selected in
                    0)
                        showGuide
                        ;;
                    1)
                        showSetupMenu
                        ;;
                    2)
                        if [ -z "$cloneDir" ]; then
                            echo -e "${COLOR_RED}Clone directory not set! Please set it in Setup Scripts menu.$COLOR_RESET"
                            echo -e "${COLOR_GREEN}\nPress any key to continue...$COLOR_RESET"
                            getch >/dev/null
                            break
                        fi
                        
                        local outputDir
                        outputDir=$(getOutputDirectory)
                        local finalImgPath="$outputDir/$FINAL_IMG_NAME"
                        local expandedCloneDir
                        expandedCloneDir=$(expandPath "$cloneDir")
                        
                        execute_command "sudo mkdir -p \"$expandedCloneDir\"" "true"
                        
                        if copyFilesWithRsync "$SOURCE_DIR" "$expandedCloneDir"; then
                            createSquashFS "$expandedCloneDir" "$finalImgPath"
                            createChecksum "$finalImgPath"
                            printFinalMessage "$finalImgPath"
                        fi
                        
                        echo -e "${COLOR_GREEN}\nPress any key to continue...$COLOR_RESET"
                        getch >/dev/null
                        ;;
                    3)
                        createISO
                        echo -e "${COLOR_GREEN}\nPress any key to continue...$COLOR_RESET"
                        getch >/dev/null
                        ;;
                    4)
                        execute_command "df -h"
                        echo -e "${COLOR_GREEN}\nPress any key to continue...$COLOR_RESET"
                        getch >/dev/null
                        ;;
                    5)
                        installISOToUSB
                        ;;
                    6)
                        runCMIInstaller
                        ;;
                    7)
                        updateScript
                        ;;
                    8)
                        return
                        ;;
                esac
                ;;
        esac
    done
}

# Main function
main() {
    USERNAME=$(whoami)
    BUILD_DIR="/home/$USERNAME/.config/cmi/build-image-arch-img"
    
    local configDir="/home/$USERNAME/.config/cmi"
    execute_command "mkdir -p \"$configDir\"" "true"
    
    loadConfig
    
    # Start time update in background
    update_time_thread &
    local time_pid=$!
    
    # Set terminal to non-canonical mode
    local oldt
    oldt=$(stty -g)
    stty -icanon
    
    showMainMenu
    
    # Cleanup
    time_thread_running=false
    kill $time_pid 2>/dev/null
    wait $time_pid 2>/dev/null
    stty "$oldt"
}

# Run main function
main "$@"