#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <dirent.h>

// Constants
const std::string ORIG_IMG_NAME = "rootfs1.img";
const std::string COMPRESSED_IMG_NAME = "rootfs.img";
std::string MOUNT_POINT = "";
const std::string SOURCE_DIR = "/";
const std::string COMPRESSION_LEVEL = "22";
const std::string SQUASHFS_COMPRESSION = "zstd";
const std::vector<std::string> SQUASHFS_COMPRESSION_ARGS = {"-Xcompression-level", "22"};
std::string BUILD_DIR = "/home/spitfire/.config/cmi/build-image-arch-ext4img";
std::string USERNAME = "";

// Configuration state
struct ConfigState {
    std::string isoTag;
    std::string isoName;
    std::string outputDir;
    std::string vmlinuzPath;
    bool mkinitcpioGenerated = false;
    bool grubEdited = false;

    bool isReadyForISO() const {
        return !isoTag.empty() && !isoName.empty() && !outputDir.empty() &&
        !vmlinuzPath.empty() && mkinitcpioGenerated && grubEdited;
    }
} config;

// ANSI color codes
const std::string COLOR_RED = "\033[31m";
const std::string COLOR_GREEN = "\033[32m";
const std::string COLOR_BLUE = "\033[34m";
const std::string COLOR_CYAN = "\033[38;2;0;255;255m";
const std::string COLOR_YELLOW = "\033[33m";
const std::string COLOR_RESET = "\033[0m";

void execute_command(const std::string& cmd) {
    std::cout << COLOR_CYAN;
    fflush(stdout);
    int status = system(cmd.c_str());
    std::cout << COLOR_RESET;
    if (status != 0) {
        std::cerr << COLOR_RED << "Error executing: " << cmd << COLOR_RESET << std::endl;
        exit(1);
    }
}

void printCheckbox(bool checked) {
    if (checked) {
        std::cout << COLOR_GREEN << "[✓]" << COLOR_RESET;
    } else {
        std::cout << COLOR_RED << "[ ]" << COLOR_RESET;
    }
}

void printBanner() {
    std::cout << COLOR_RED << R"(
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚══════╝╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
)" << COLOR_RESET << std::endl;
std::cout << COLOR_CYAN << "claudemods ext4 img iso creator v1.0" << COLOR_RESET << std::endl << std::endl;
}

void showDiskUsage() {
    std::cout << COLOR_GREEN << "\nCurrent system disk usage:\n" << COLOR_RESET;
    execute_command("df -h /");
    std::cout << std::endl;
}

void checkSetupStatus() {
    std::cout << COLOR_BLUE << "\nCurrent ISO Creation Setup Status:\n" << COLOR_RESET;
    std::cout << " "; printCheckbox(!config.isoTag.empty());
    std::cout << " ISO Tag: " << (config.isoTag.empty() ? COLOR_YELLOW + "Not set" : COLOR_CYAN + config.isoTag) << COLOR_RESET << "\n";

    std::cout << " "; printCheckbox(!config.isoName.empty());
    std::cout << " ISO Name: " << (config.isoName.empty() ? COLOR_YELLOW + "Not set" : COLOR_CYAN + config.isoName) << COLOR_RESET << "\n";

    std::cout << " "; printCheckbox(!config.outputDir.empty());
    std::cout << " Output Directory: " << (config.outputDir.empty() ? COLOR_YELLOW + "Not set" : COLOR_CYAN + config.outputDir) << COLOR_RESET << "\n";

    std::cout << " "; printCheckbox(!config.vmlinuzPath.empty());
    std::cout << " vmlinuz Selected: " << (config.vmlinuzPath.empty() ? COLOR_YELLOW + "Not selected" : COLOR_CYAN + config.vmlinuzPath) << COLOR_RESET << "\n";

    std::cout << " "; printCheckbox(config.mkinitcpioGenerated);
    std::cout << " mkinitcpio Generated\n";

    std::cout << " "; printCheckbox(config.grubEdited);
    std::cout << " GRUB Config Edited\n";

    if (config.isReadyForISO()) {
        std::cout << COLOR_GREEN << "\nAll requirements met for ISO creation!\n" << COLOR_RESET;
    } else {
        std::cout << COLOR_YELLOW << "\nSome requirements are missing for ISO creation\n" << COLOR_RESET;
    }
}

std::string getUserInput(const std::string& prompt) {
    std::string input;
    std::cout << COLOR_GREEN << prompt << COLOR_RESET;
    std::getline(std::cin, input);
    return input;
}

void selectVmlinuz() {
    DIR *dir;
    struct dirent *ent;
    std::vector<std::string> vmlinuzFiles;

    if ((dir = opendir("/boot")) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            std::string filename = ent->d_name;
            if (filename.find("vmlinuz") == 0) {
                vmlinuzFiles.push_back("/boot/" + filename);
            }
        }
        closedir(dir);
    } else {
        std::cerr << COLOR_RED << "Could not open /boot directory" << COLOR_RESET << std::endl;
        return;
    }

    if (vmlinuzFiles.empty()) {
        std::cerr << COLOR_RED << "No vmlinuz files found in /boot!" << COLOR_RESET << std::endl;
        return;
    }

    std::cout << COLOR_GREEN << "Available vmlinuz files:" << COLOR_RESET << std::endl;
    for (size_t i = 0; i < vmlinuzFiles.size(); i++) {
        std::cout << COLOR_GREEN << (i+1) << ") " << vmlinuzFiles[i] << COLOR_RESET << std::endl;
    }

    std::string selection = getUserInput("Select vmlinuz file (1-" + std::to_string(vmlinuzFiles.size()) + "): ");
    try {
        int choice = std::stoi(selection);
        if (choice > 0 && choice <= static_cast<int>(vmlinuzFiles.size())) {
            config.vmlinuzPath = vmlinuzFiles[choice-1];
            std::cout << COLOR_CYAN << "Selected: " << config.vmlinuzPath << COLOR_RESET << std::endl;
        } else {
            std::cerr << COLOR_RED << "Invalid selection!" << COLOR_RESET << std::endl;
        }
    } catch (...) {
        std::cerr << COLOR_RED << "Invalid input!" << COLOR_RESET << std::endl;
    }
}

void generateMkinitcpio() {
    if (config.vmlinuzPath.empty()) {
        std::cerr << COLOR_RED << "Please select vmlinuz first!" << COLOR_RESET << std::endl;
        return;
    }

    if (BUILD_DIR.empty()) {
        std::cerr << COLOR_RED << "Build directory not set!" << COLOR_RESET << std::endl;
        return;
    }

    std::string vmlinuzDest = BUILD_DIR + "/boot/vmlinuz-x86_64";
    std::cout << COLOR_CYAN << "Copying " << config.vmlinuzPath << " to " << vmlinuzDest << COLOR_RESET << std::endl;
    execute_command("sudo cp " + config.vmlinuzPath + " " + vmlinuzDest);

    std::cout << COLOR_CYAN << "Generating initramfs..." << COLOR_RESET << std::endl;
    execute_command("cd " + BUILD_DIR + " && sudo mkinitcpio -c mkinitcpio.conf -g " + BUILD_DIR + "/boot/initramfs-x86_64.img");

    config.mkinitcpioGenerated = true;
    std::cout << COLOR_GREEN << "mkinitcpio generated successfully!" << COLOR_RESET << std::endl;
}

void editGrubCfg() {
    if (BUILD_DIR.empty()) {
        std::cerr << COLOR_RED << "Build directory not set!" << COLOR_RESET << std::endl;
        return;
    }

    std::string grubCfgPath = BUILD_DIR + "/boot/grub/grub.cfg";
    std::cout << COLOR_CYAN << "Editing GRUB config: " << grubCfgPath << COLOR_RESET << std::endl;
    execute_command("nano " + grubCfgPath);

    config.grubEdited = true;
    std::cout << COLOR_GREEN << "GRUB config edited!" << COLOR_RESET << std::endl;
}

void setIsoTag() {
    config.isoTag = getUserInput("Enter ISO tag (e.g., 2025): ");
}

void setIsoName() {
    config.isoName = getUserInput("Enter ISO name (e.g., claudemods.iso): ");
}

void setOutputDir() {
    config.outputDir = getUserInput("Enter output directory (e.g., ~/Downloads): ");
}

void showSetupMenu() {
    std::cout << COLOR_BLUE << "\nISO Creation Setup Menu:\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "1) Set ISO Tag\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "2) Set ISO Name\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "3) Set Output Directory\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "4) Select vmlinuz\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "5) Generate mkinitcpio\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "6) Edit GRUB Config\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "7) Check Setup Status\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "8) Back to Main Menu\n" << COLOR_RESET;
}

void processSetupChoice(int choice) {
    switch (choice) {
        case 1: setIsoTag(); break;
        case 2: setIsoName(); break;
        case 3: setOutputDir(); break;
        case 4: selectVmlinuz(); break;
        case 5: generateMkinitcpio(); break;
        case 6: editGrubCfg(); break;
        case 7: checkSetupStatus(); break;
        case 8: return;
        default: std::cout << COLOR_RED << "Invalid choice!" << COLOR_RESET << std::endl;
    }
}

bool createImageFile(const std::string& size, const std::string& filename) {
    std::string command = "truncate -s " + size + " " + filename;
    execute_command(command);
    return true;
}

bool formatFilesystem(const std::string& fsType, const std::string& filename) {
    if (fsType == "btrfs") {
        std::cout << COLOR_CYAN << "Formatting as BTRFS with forced zstd:" << COMPRESSION_LEVEL << " compression" << COLOR_RESET << std::endl;
        std::string command = "sudo mkfs.btrfs -f --compress-force=zstd:" + COMPRESSION_LEVEL + " -L \"SYSTEM_BACKUP\" " + filename;
        execute_command(command);
    } else {
        std::cout << COLOR_CYAN << "Formatting as ext4" << COLOR_RESET << std::endl;
        std::string command = "sudo mkfs.ext4 -F -L \"SYSTEM_BACKUP\" " + filename;
        execute_command(command);
    }
    return true;
}

bool mountFilesystem(const std::string& fsType, const std::string& filename, const std::string& mountPoint) {
    execute_command("mkdir -p " + mountPoint);
    if (fsType == "btrfs") {
        std::string options = "loop,compress-force=zstd:" + COMPRESSION_LEVEL + ",discard,noatime,space_cache=v2,autodefrag";
        std::string command = "sudo mount -o " + options + " " + filename + " " + mountPoint;
        execute_command(command);
    } else {
        std::string options = "loop,discard,noatime";
        std::string command = "sudo mount -o " + options + " " + filename + " " + mountPoint;
        execute_command(command);
    }
    return true;
}

bool copyFilesWithRsync(const std::string& source, const std::string& destination) {
    std::string command = "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
    "--exclude=/etc/udev/rules.d/70-persistent-cd.rules "
    "--exclude=/etc/udev/rules.d/70-persistent-net.rules "
    "--exclude=/etc/mtab "
    "--exclude=/etc/fstab "
    "--exclude=/dev/* "
    "--exclude=/proc/* "
    "--exclude=/sys/* "
    "--exclude=/tmp/* "
    "--exclude=/run/* "
    "--exclude=/mnt/* "
    "--exclude=/media/* "
    "--exclude=/lost+found "
    "--exclude=*rootfs1.img "
    "--exclude=btrfs_temp "
    "--exclude=rootfs.img " +
    source + " " + destination;
    execute_command(command);
    return true;
}

bool unmountAndCleanup(const std::string& mountPoint) {
    sync();
    try {
        execute_command("sudo umount " + mountPoint);
        execute_command("rmdir " + mountPoint);
    } catch (...) {
        return false;
    }
    return true;
}

bool createSquashFS(const std::string& inputFile, const std::string& outputFile) {
    std::string command = "sudo mksquashfs " + inputFile + " " + outputFile +
    " -comp " + SQUASHFS_COMPRESSION +
    " " + SQUASHFS_COMPRESSION_ARGS[0] + " " + SQUASHFS_COMPRESSION_ARGS[1] +
    " -noappend";
    execute_command(command);
    return true;
}

bool createChecksum(const std::string& filename) {
    std::string command = "md5sum " + filename + " > " + filename + ".md5";
    execute_command(command);
    return true;
}

void printFinalMessage(const std::string& fsType, const std::string& squashfsOutput) {
    std::cout << std::endl;
    if (fsType == "btrfs") {
        std::cout << COLOR_CYAN << "Compressed BTRFS image created successfully: " << squashfsOutput << COLOR_RESET << std::endl;
    } else {
        std::cout << COLOR_CYAN << "Ext4 image created successfully: " << squashfsOutput << COLOR_RESET << std::endl;
    }
    std::cout << COLOR_CYAN << "Checksum file: " << squashfsOutput << ".md5" << COLOR_RESET << std::endl;
}

void deleteOriginalImage(const std::string& imgName) {
    std::cout << COLOR_CYAN << "Deleting original image file: " << imgName << COLOR_RESET << std::endl;
    execute_command("rm -f " + imgName);
}

std::string getOutputDirectory() {
    std::string dir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-ext4img/LiveOS";
    execute_command("mkdir -p " + dir);
    return dir;
}

std::string expandPath(const std::string& path) {
    std::string result = path;
    size_t pos;
    if ((pos = result.find("~")) != std::string::npos) {
        const char* home = getenv("HOME");
        if (home) result.replace(pos, 1, home);
    }
    if ((pos = result.find("$USER")) != std::string::npos) {
        result.replace(pos, 5, USERNAME);
    }
    return result;
}

bool createISO() {
    if (!config.isReadyForISO()) {
        std::cerr << COLOR_RED << "Cannot create ISO - setup is incomplete!" << COLOR_RESET << std::endl;
        checkSetupStatus();
        return false;
    }

    std::cout << COLOR_CYAN << "\nStarting ISO creation process...\n" << COLOR_RESET;

    std::string expandedOutputDir = expandPath(config.outputDir);
    execute_command("mkdir -p " + expandedOutputDir);

    std::string xorrisoCmd = "sudo xorriso -as mkisofs "
    "--modification-date=\"$(date +%Y%m%d%H%M%S00)\" "
    "--protective-msdos-label "
    "-volid \"" + config.isoTag + "\" "
    "-appid \"claudemods Linux Live/Rescue CD\" "
    "-publisher \"claudemods claudemods101@gmail.com >\" "
    "-preparer \"Prepared by user\" "
    "-r -graft-points -no-pad "
    "--sort-weight 0 / "
    "--sort-weight 1 /boot "
    "--grub2-mbr " + BUILD_DIR + "/boot/grub/i386-pc/boot_hybrid.img "
    "-partition_offset 16 "
    "-b boot/grub/i386-pc/eltorito.img "
    "-c boot.catalog "
    "-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info "
    "-eltorito-alt-boot "
    "-append_partition 2 0xef " + BUILD_DIR + "/boot/efi.img "
    "-e --interval:appended_partition_2:all:: "
    "-no-emul-boot "
    "-iso-level 3 "
    "-o \"" + expandedOutputDir + "/" + config.isoName + "\" " +
    BUILD_DIR;

    execute_command(xorrisoCmd);
    std::cout << COLOR_CYAN << "ISO created successfully at " << expandedOutputDir << "/" << config.isoName << COLOR_RESET << std::endl;
    return true;
}

void showMainMenu() {
    std::cout << COLOR_BLUE << "\nMain Menu:\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "1) Create Image\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "2) ISO Creation Setup\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "3) Create ISO\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "4) Show Disk Usage\n" << COLOR_RESET;
    std::cout << COLOR_BLUE << "5) Exit\n" << COLOR_RESET;
    std::cout << COLOR_CYAN << "Enter your choice (1-5): " << COLOR_RESET;
}

void processMainMenuChoice(int choice) {
    switch (choice) {
        case 1: {
            USERNAME = getUserInput("Enter your username: ");
            MOUNT_POINT = "/home/" + USERNAME + "/btrfs_temp";
            BUILD_DIR = "/home/" + USERNAME + "/.config/cmi/build-image-arch-ext4img";

            std::string outputDir = getOutputDirectory();
            std::string outputOrigImgPath = outputDir + "/" + ORIG_IMG_NAME;
            std::string outputCompressedImgPath = outputDir + "/" + COMPRESSED_IMG_NAME;

            std::string imgSize = getUserInput("Enter the image size in GB (e.g., 6 for 6GB): ") + "G";

            std::cout << COLOR_BLUE << "Choose filesystem type:\n" << COLOR_RESET;
            std::cout << COLOR_BLUE << "1) btrfs\n" << COLOR_RESET;
            std::cout << COLOR_BLUE << "2) ext4\n" << COLOR_RESET;
            std::string fsChoice = getUserInput("Enter choice (1 or 2): ");
            std::string fsType = (fsChoice == "1") ? "btrfs" : "ext4";

            if (!createImageFile(imgSize, outputOrigImgPath) ||
                !formatFilesystem(fsType, outputOrigImgPath) ||
                !mountFilesystem(fsType, outputOrigImgPath, MOUNT_POINT) ||
                !copyFilesWithRsync(SOURCE_DIR, MOUNT_POINT)) {
                return;
                }

                unmountAndCleanup(MOUNT_POINT);
            createSquashFS(outputOrigImgPath, outputCompressedImgPath);
            deleteOriginalImage(outputOrigImgPath);
            createChecksum(outputCompressedImgPath);
            printFinalMessage(fsType, outputCompressedImgPath);
            break;
        }
        case 2: {
            while (true) {
                showSetupMenu();
                std::string input;
                std::getline(std::cin, input);
                try {
                    int setupChoice = std::stoi(input);
                    if (setupChoice == 8) break;
                    processSetupChoice(setupChoice);
                } catch (...) {
                    std::cout << COLOR_RED << "Invalid input!" << COLOR_RESET << std::endl;
                }
            }
            break;
        }
        case 3:
            createISO();
            break;
        case 4:
            showDiskUsage();
            break;
        case 5:
            exit(0);
        default:
            std::cout << COLOR_RED << "Invalid choice!" << COLOR_RESET << std::endl;
    }
}

int main() {
    printBanner();
    showDiskUsage();

    while (true) {
        showMainMenu();
        std::string input;
        std::getline(std::cin, input);
        try {
            int choice = std::stoi(input);
            processMainMenuChoice(choice);
        } catch (...) {
            std::cout << COLOR_RED << "Invalid input!" << COLOR_RESET << std::endl;
        }
    }

    return 0;
}
