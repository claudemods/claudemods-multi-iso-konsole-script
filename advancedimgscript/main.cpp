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
std::string MOUNT_POINT = "";  // Will be set in main()
const std::string SOURCE_DIR = "/";
const std::string COMPRESSION_LEVEL = "22";
const std::string SQUASHFS_COMPRESSION = "zstd";
const std::vector<std::string> SQUASHFS_COMPRESSION_ARGS = {"-Xcompression-level", "22"};
std::string BUILD_DIR = "";  // Will be set in main()
std::string USERNAME = "";   // Will be set in main()

// ANSI color codes
const std::string COLOR_RED = "\033[31m";
const std::string COLOR_GREEN = "\033[32m";
const std::string COLOR_CYAN = "\033[38;2;0;255;255m";
const std::string COLOR_RESET = "\033[0m";

// Function declarations
void execute_command(const std::string& cmd);
bool createImageFile(const std::string& size, const std::string& filename);
bool formatFilesystem(const std::string& fsType, const std::string& filename);
bool mountFilesystem(const std::string& fsType, const std::string& filename, const std::string& mountPoint);
bool copyFilesWithRsync(const std::string& source, const std::string& destination);
bool unmountAndCleanup(const std::string& mountPoint);
bool createSquashFS(const std::string& inputFile, const std::string& outputFile);
bool createChecksum(const std::string& filename);
void printFinalMessage(const std::string& fsType, const std::string& squashfsOutput);
void showDiskUsage();
void deleteOriginalImage(const std::string& imgName);
std::string getOutputDirectory();
bool createISO();
std::string expandPath(const std::string& path);
std::string getUserInput(const std::string& prompt);
int selectVmlinuz();

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

void showDiskUsage() {
    std::cout << COLOR_GREEN << R"(
Current system disk usage:
)" << COLOR_RESET;
    execute_command("df -h /");
    std::cout << std::endl;
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
    // Expand ~ to home directory
    if ((pos = result.find("~")) != std::string::npos) {
        const char* home = getenv("HOME");
        if (home) {
            result.replace(pos, 1, home);
        }
    }
    // Expand $USER
    if ((pos = result.find("$USER")) != std::string::npos) {
        result.replace(pos, 5, USERNAME);
    }
    return result;
}

std::string getUserInput(const std::string& prompt) {
    std::string input;
    std::cout << COLOR_GREEN << prompt << COLOR_RESET;
    std::getline(std::cin, input);
    return input;
}

int selectVmlinuz() {
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
        exit(1);
    }
    if (vmlinuzFiles.empty()) {
        std::cerr << COLOR_RED << "No vmlinuz files found in /boot!" << COLOR_RESET << std::endl;
        exit(1);
    }
    std::cout << COLOR_GREEN << "Available vmlinuz files:" << COLOR_RESET << std::endl;
    for (size_t i = 0; i < vmlinuzFiles.size(); i++) {
        std::cout << COLOR_GREEN << (i+1) << ") " << vmlinuzFiles[i] << COLOR_RESET << std::endl;
    }
    std::string selection = getUserInput("Select vmlinuz file (1-" + std::to_string(vmlinuzFiles.size()) + "): ");
    try {
        int choice = std::stoi(selection);
        if (choice < 1 || choice > static_cast<int>(vmlinuzFiles.size())) {
            std::cerr << COLOR_RED << "Invalid selection!" << COLOR_RESET << std::endl;
            exit(1);
        }
        return choice - 1;
    } catch (...) {
        std::cerr << COLOR_RED << "Invalid input!" << COLOR_RESET << std::endl;
        exit(1);
    }
}

bool createISO() {
    std::cout << COLOR_CYAN << R"(
Starting ISO creation process...
)" << COLOR_RESET << std::endl;
    // Get user inputs
    std::string tag = getUserInput("Enter the tag (e.g., 2025): ");
    std::string outputDir = getUserInput("Enter the output directory (e.g., /home/username/Downloads): ");
    std::string isoName = getUserInput("Enter the ISO name (e.g., claudemods.iso): ");
    // Select vmlinuz
    int vmlinuzIndex = selectVmlinuz();
    // Get build directory
    std::string buildDir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-ext4img";
    // Copy selected vmlinuz
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
    }
    std::string vmlinuzSource = vmlinuzFiles[vmlinuzIndex];
    std::string vmlinuzDest = buildDir + "/boot/vmlinuz-x86_64";
    std::cout << COLOR_CYAN << "Copying " << vmlinuzSource << " to " << vmlinuzDest << COLOR_RESET << std::endl;
    execute_command("sudo cp " + vmlinuzSource + " " + vmlinuzDest);
    // Generate initramfs
    std::cout << COLOR_CYAN << "Generating initramfs..." << COLOR_RESET << std::endl;
    execute_command("cd " + buildDir + " && sudo mkinitcpio -c mkinitcpio.conf -g " + buildDir + "/boot/initramfs-x86_64.img");
    // Create ISO
    std::cout << COLOR_CYAN << "Creating ISO..." << COLOR_RESET << std::endl;
    outputDir = expandPath(outputDir);
    execute_command("mkdir -p " + outputDir);
    std::string xorrisoCmd = "sudo xorriso -as mkisofs "
    "--modification-date=\"$(date +%Y%m%d%H%M%S00)\" "
    "--protective-msdos-label "
    "-volid \"" + tag + "\" "
    "-appid \"claudemods Linux Live/Rescue CD\" "
    "-publisher \"claudemods claudemods101@gmail.com >\" "
    "-preparer \"Prepared by user\" "
    "-r -graft-points -no-pad "
    "--sort-weight 0 / "
    "--sort-weight 1 /boot "
    "--grub2-mbr " + buildDir + "/boot/grub/i386-pc/boot_hybrid.img "
    "-partition_offset 16 "
    "-b boot/grub/i386-pc/eltorito.img "
    "-c boot.catalog "
    "-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info "
    "-eltorito-alt-boot "
    "-append_partition 2 0xef " + buildDir + "/boot/efi.img "
    "-e --interval:appended_partition_2:all:: "
    "-no-emul-boot "
    "-iso-level 3 "
    "-o \"" + outputDir + "/" + isoName + "\" " +
    buildDir;
    execute_command(xorrisoCmd);
    std::cout << COLOR_CYAN << "ISO created successfully at " << outputDir << "/" << isoName << COLOR_RESET << std::endl;
    return true;
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
    // Create mount point
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
        std::cout << COLOR_CYAN << "Compression: zstd at level " << COMPRESSION_LEVEL << " (forced during write)" << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "SquashFS compression: " << SQUASHFS_COMPRESSION << " with level " << SQUASHFS_COMPRESSION_ARGS[1] << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Mount SquashFS with:" << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "  sudo mount -t squashfs " << squashfsOutput << " /mnt/point -o loop" << COLOR_RESET << std::endl;
    } else {
        std::cout << COLOR_CYAN << "Ext4 image created successfully: " << squashfsOutput << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "SquashFS compression: " << SQUASHFS_COMPRESSION << " with level " << SQUASHFS_COMPRESSION_ARGS[1] << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Mount SquashFS with:" << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "  sudo mount -t squashfs " << squashfsOutput << " /mnt/point -o loop" << COLOR_RESET << std::endl;
    }
    std::cout << COLOR_CYAN << "Checksum file: " << squashfsOutput << ".md5" << COLOR_RESET << std::endl;
}

int main() {
    printBanner();
    showDiskUsage();
    
    // Get username once at the start
    USERNAME = getUserInput("Enter your username: ");
    MOUNT_POINT = "/home/" + USERNAME + "/btrfs_temp";
    BUILD_DIR = "/home/" + USERNAME + "/.config/cmi/build-image-arch-ext4img";
    
    // Get output directory
    std::string outputDir = getOutputDirectory();
    std::string outputOrigImgPath = outputDir + "/" + ORIG_IMG_NAME;
    std::string outputCompressedImgPath = outputDir + "/" + COMPRESSED_IMG_NAME;
    
    // Ask user for image size
    std::string imgSize = getUserInput("Enter the image size in GB (e.g., 6 for 6GB): ");
    imgSize += "G";
    
    // Ask user for filesystem type
    std::cout << COLOR_GREEN << "Choose the filesystem type:" << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "1) btrfs" << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "2) ext4" << COLOR_RESET << std::endl;
    std::string choice = getUserInput("Enter your choice (1 or 2): ");
    std::string fsType;
    if (choice == "1") {
        fsType = "btrfs";
    } else if (choice == "2") {
        fsType = "ext4";
    } else {
        std::cout << COLOR_CYAN << "Invalid choice. Defaulting to btrfs." << COLOR_RESET << std::endl;
        fsType = "btrfs";
    }
    
    // Create the image file
    std::cout << COLOR_CYAN << "Creating " << imgSize << " " << fsType << " image file: " << outputOrigImgPath << COLOR_RESET << std::endl;
    if (!createImageFile(imgSize, outputOrigImgPath)) {
        std::cerr << COLOR_RED << "Failed to create image file" << COLOR_RESET << std::endl;
        return 1;
    }
    
    // Format the image based on filesystem type
    if (!formatFilesystem(fsType, outputOrigImgPath)) {
        execute_command("rm -f " + outputOrigImgPath);
        return 1;
    }
    
    // Create mount point and mount
    std::cout << COLOR_CYAN << "Mounting the image file" << COLOR_RESET << std::endl;
    if (!mountFilesystem(fsType, outputOrigImgPath, MOUNT_POINT)) {
        execute_command("rm -f " + outputOrigImgPath);
        return 1;
    }
    
    // Rsync command to copy files
    std::cout << COLOR_CYAN << "Copying files with rsync..." << COLOR_RESET << std::endl;
    if (!copyFilesWithRsync(SOURCE_DIR, MOUNT_POINT)) {
        unmountAndCleanup(MOUNT_POINT);
        execute_command("rm -f " + outputOrigImgPath);
        return 1;
    }
    
    // Clean up
    std::cout << COLOR_CYAN << "Unmounting and finalizing..." << COLOR_RESET << std::endl;
    if (!unmountAndCleanup(MOUNT_POINT)) {
        // Continue even if unmount fails, but warn user
    }
    
    // Compress the final image with SquashFS
    std::cout << COLOR_CYAN << "Compressing final image with SquashFS..." << COLOR_RESET << std::endl;
    if (!createSquashFS(outputOrigImgPath, outputCompressedImgPath)) {
        return 1;
    }
    
    // Delete original image after successful compression
    deleteOriginalImage(outputOrigImgPath);
    
    // Create checksum
    std::cout << COLOR_CYAN << "Creating checksum..." << COLOR_RESET << std::endl;
    if (!createChecksum(outputCompressedImgPath)) {
        // Non-fatal error, continue
    }
    
    // Final message
    printFinalMessage(fsType, outputCompressedImgPath);
    
    // Ask if user wants to create ISO
    std::string createIsoChoice = getUserInput("Do you want to create an ISO from this image? (y/n): ");
    if (createIsoChoice == "y" || createIsoChoice == "Y") {
        if (!createISO()) {
            std::cerr << COLOR_RED << "ISO creation failed" << COLOR_RESET << std::endl;
            return 1;
        }
    }
    
    return 0;
}
