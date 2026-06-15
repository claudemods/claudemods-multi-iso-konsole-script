#ifndef CLONER_H
#define CLONER_H

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sys/mount.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <pwd.h>

// Forward declarations from main
extern std::string USERNAME;
extern std::string BUILD_DIR;
extern const std::string COLOR_RED;
extern const std::string COLOR_GREEN;
extern const std::string COLOR_BLUE;
extern const std::string COLOR_CYAN;
extern const std::string COLOR_YELLOW;
extern const std::string COLOR_RESET;

// Forward declare functions from main (NO default arguments here)
void execute_command(const std::string& cmd, bool continueOnError);
std::string getUserInput(const std::string& prompt);
int getch();
std::string expandPath(const std::string& path);
bool isDeviceMounted(const std::string& device);
bool mountDevice(const std::string& device, const std::string& mountPoint);

// Forward declare showMenu from main.cpp (free function, not class member)
int showMenu(const std::string &title, const std::vector<std::string> &items, int selected);

// SquashFS related constants
const std::string ORIG_IMG_NAME = "rootfs1.img";
const std::string FINAL_IMG_NAME = "rootfs.img";
const std::string SQUASHFS_COMPRESSION = "zstd";
const std::string COMPRESSION_LEVEL = "22";
const std::vector<std::string> SQUASHFS_COMPRESSION_ARGS = {"-Xcompression-level", "22"};

class Cloner {
public:
    // Get the output directory for SquashFS images
    static std::string getOutputDirectory() {
        std::string dir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img/LiveOS";
        return dir;
    }

    // Create SquashFS with exact rsync exclusions for current system
    static bool createSquashFS(const std::string& inputDir, const std::string& outputFile) {
        std::string command = "sudo mksquashfs " + inputDir + " " + outputFile +
        " -noappend -comp " + SQUASHFS_COMPRESSION +
        " -Xcompression-level " + COMPRESSION_LEVEL + " -b 256K " +
        "-e etc/udev/rules.d/70-persistent-cd.rules " +
        "-e etc/udev/rules.d/70-persistent-net.rules " +
        "-e etc/mtab " +
        "-e etc/fstab " +
        "-e dev/* " +
        "-e proc/* " +
        "-e sys/* " +
        "-e tmp/* " +
        "-e run/* " +
        "-e mnt/* " +
        "-e media/* " +
        "-e lost+found " +
        "-e clone_system_temp";

        execute_command(command, true);
        return true;
    }

    // Create SquashFS for another drive with xz compression
    static bool createSquashFSForDrive(const std::string& inputDir, const std::string& outputFile) {
        std::string command = "sudo mksquashfs " + inputDir + " " + outputFile +
        " -noappend -comp xz -b 256K -Xbcj x86 " +
        "-e etc/udev/rules.d/70-persistent-cd.rules " +
        "-e etc/udev/rules.d/70-persistent-net.rules " +
        "-e etc/mtab " +
        "-e etc/fstab " +
        "-e dev/* " +
        "-e proc/* " +
        "-e sys/* " +
        "-e tmp/* " +
        "-e run/* " +
        "-e mnt/* " +
        "-e media/* " +
        "-e lost+found " +
        "-e temp_clone_mount";

        execute_command(command, true);
        return true;
    }

    // Create checksum file for the image
    static bool createChecksum(const std::string& filename) {
        std::string command = "sudo sha512sum " + filename + " > " + filename + ".sha512";
        execute_command(command, true);
        return true;
    }

    // Print final success message with file info
    static void printFinalMessage(const std::string& outputFile) {
        std::cout << std::endl;
        std::cout << COLOR_CYAN << "SquashFS image created successfully: " << outputFile << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Checksum file: " << outputFile + ".sha512" << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Size: ";
        execute_command("sudo du -h " + outputFile + " | cut -f1", true);
        std::cout << COLOR_RESET;
    }

    // Mount system using bind mount instead of OverlayFS
    static bool mountSystemToCloneDir(const std::string& cloneDir) {
        std::cout << COLOR_CYAN << "Mounting system to: " << cloneDir << COLOR_RESET << std::endl;

        execute_command("sudo mkdir -p " + cloneDir, true);

        // Use bind mount instead of OverlayFS
        std::string mountCmd = "sudo mount --bind / " + cloneDir;

        if (system(mountCmd.c_str()) != 0) {
            std::cerr << COLOR_RED << "Failed to bind mount!" << COLOR_RESET << std::endl;
            return false;
        }

        std::cout << COLOR_GREEN << "System mounted successfully to: " << cloneDir << COLOR_RESET << std::endl;
        return true;
    }

    // Clone current system using bind mount with unmount after completion
    static void cloneCurrentSystem(const std::string& cloneDir) {
        if (!mountSystemToCloneDir(cloneDir)) {
            std::cerr << COLOR_RED << "Failed to mount system!" << COLOR_RESET << std::endl;
            return;
        }

        // Create SquashFS directly from the mounted bind
        std::string outputDir = getOutputDirectory();
        std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;

        createSquashFS(cloneDir, finalImgPath);

        // Unmount the bind mount after SquashFS creation
        std::cout << COLOR_CYAN << "Unmounting bind mount..." << COLOR_RESET << std::endl;
        execute_command("sudo umount " + cloneDir, true);

        createChecksum(finalImgPath);
        printFinalMessage(finalImgPath);

        std::cout << COLOR_GREEN << "Current system cloned successfully using bind mount!" << COLOR_RESET << std::endl;
    }

    // Clone another drive
    static void cloneAnotherDrive(const std::string& cloneDir) {
        std::cout << COLOR_GREEN << "\nAvailable drives:" << COLOR_RESET << std::endl;
        execute_command("lsblk -f -o NAME,FSTYPE,SIZE,MOUNTPOINT | grep -v 'loop'", true);

        std::string drive = getUserInput("Enter drive to clone (e.g., /dev/sda2): ");
        if (drive.empty()) {
            std::cerr << COLOR_RED << "No drive specified!" << COLOR_RESET << std::endl;
            return;
        }

        std::string checkCmd = "ls " + drive + " > /dev/null 2>&1";
        if (system(checkCmd.c_str()) != 0) {
            std::cerr << COLOR_RED << "Drive " + drive + " does not exist!" << COLOR_RESET << std::endl;
            return;
        }

        std::string tempMountPoint = "/mnt/temp_clone_mount";

        if (!isDeviceMounted(drive)) {
            if (!mountDevice(drive, tempMountPoint)) {
                std::cerr << COLOR_RED << "Failed to mount " + drive + "!" << COLOR_RESET << std::endl;
                return;
            }
        } else {
            std::string mountCmd = "mount | grep " + drive + " | awk '{print $3}'";
            FILE* fp = popen(mountCmd.c_str(), "r");
            if (fp) {
                char mountPath[256];
                if (fgets(mountPath, sizeof(mountPath), fp)) {
                    tempMountPoint = mountPath;
                    tempMountPoint.erase(tempMountPoint.find_last_not_of("\n") + 1);
                }
                pclose(fp);
            }
        }

        std::cout << COLOR_GREEN << "Creating SquashFS from " << drive << "..." << COLOR_RESET << std::endl;

        std::string outputDir = getOutputDirectory();
        std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;

        // Create SquashFS directly from the mounted drive with exclusions
        createSquashFSForDrive(tempMountPoint, finalImgPath);

        if (!isDeviceMounted(drive) || system(("mount | grep " + drive + " | grep " + tempMountPoint).c_str()) == 0) {
            execute_command("sudo umount " + tempMountPoint, true);
            execute_command("sudo rmdir " + tempMountPoint, true);
        }

        createChecksum(finalImgPath);
        printFinalMessage(finalImgPath);

        std::cout << COLOR_GREEN << "Drive " << drive << " cloned successfully!" << COLOR_RESET << std::endl;
    }

    // Clone folder or file
    static void cloneFolderOrFile(const std::string& cloneDir) {
        std::cout << COLOR_CYAN << "\nClone Folder or File" << COLOR_RESET << std::endl;
        std::cout << COLOR_YELLOW << "Enter the path to a folder or file you want to clone." << COLOR_RESET << std::endl;
        std::cout << COLOR_YELLOW << "Examples:" << COLOR_RESET << std::endl;
        std::cout << COLOR_YELLOW << "  - Folder: /home/" << USERNAME << "/Documents" << COLOR_RESET << std::endl;
        std::cout << COLOR_YELLOW << "  - File: /home/" << USERNAME << "/file.txt" << COLOR_RESET << std::endl;

        std::string sourcePath = getUserInput("Enter folder or file path to clone: ");
        if (sourcePath.empty()) {
            std::cerr << COLOR_RED << "No path specified!" << COLOR_RESET << std::endl;
            return;
        }

        std::string checkCmd = "sudo test -e " + sourcePath + " > /dev/null 2>&1";
        if (system(checkCmd.c_str()) != 0) {
            std::cerr << COLOR_RED << "Source path does not exist: " << sourcePath << COLOR_RESET << std::endl;
            return;
        }

        std::string userfilesDir = cloneDir + "/home/userfiles";
        execute_command("sudo mkdir -p " + userfilesDir, true);

        std::cout << COLOR_CYAN << "Cloning " << sourcePath << " to " << userfilesDir << "..." << COLOR_RESET << std::endl;

        std::string rsyncCmd = "sudo rsync -aHAXSr --numeric-ids --info=progress2 " +
        sourcePath + " " + userfilesDir + "/";

        execute_command(rsyncCmd, true);

        std::cout << COLOR_GREEN << "Successfully cloned " << sourcePath << " to " << userfilesDir << "!" << COLOR_RESET << std::endl;

        std::string listCmd = "sudo ls -la " + userfilesDir + " | head -20";
        std::cout << COLOR_CYAN << "Contents of userfiles directory:" << COLOR_RESET << std::endl;
        execute_command(listCmd, true);
    }

    // Show clone options menu
    static void showCloneOptionsMenu(bool allCheckboxesChecked, const std::string& cloneDirConfig) {
        std::vector<std::string> items = {
            "Clone Current System (as it is now)",
            "Clone Another Drive (e.g., /dev/sda2)",
            "Clone Folder or File",
            "Back to Main Menu"
        };

        int selected = 0;
        int key;

        while (true) {
            // Call the free function showMenu, not Cloner::showMenu
            key = showMenu("Clone Options - Select Source:", items, selected);

            if (key == -1) {
                continue;
            }

            switch (key) {
                case 'A':
                    if (selected > 0) selected--;
                    break;
                case 'B':
                    if (selected < static_cast<int>(items.size()) - 1) selected++;
                    break;
                case '\n':
                    if (cloneDirConfig.empty()) {
                        std::cerr << COLOR_RED << "Clone directory not set! Please set it in Setup Scripts menu." << COLOR_RESET << std::endl;
                        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                        getch();
                        break;
                    }

                    if (!allCheckboxesChecked) {
                        std::cerr << COLOR_RED << "Cannot create image - all setup steps must be completed first!" << COLOR_RESET << std::endl;
                        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                        getch();
                        break;
                    }

                    std::string cloneDir = expandPath(cloneDirConfig);
                    execute_command("sudo mkdir -p " + cloneDir, true);

                    switch (selected) {
                        case 0:
                            cloneCurrentSystem(cloneDir);
                            break;
                        case 1:
                            cloneAnotherDrive(cloneDir);
                            break;
                        case 2:
                            cloneFolderOrFile(cloneDir);
                            std::cout << COLOR_YELLOW << "Folder/File cloned to " << cloneDir << "/home/userfiles" << COLOR_RESET << std::endl;
                            std::cout << COLOR_YELLOW << "You can now create an ISO that includes these files." << COLOR_RESET << std::endl;
                            break;
                        case 3:
                            return;
                    }

                    if (selected != 3) {
                        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                        getch();
                    }
                    break;
            }
        }
    }
};

#endif // CLONER_H
