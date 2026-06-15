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

    // Show clone options menu
    static void showCloneOptionsMenu(bool allCheckboxesChecked, const std::string& cloneDirConfig) {
        std::vector<std::string> items = {
            "Clone Current System (as it is now)",
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
                            return;
                    }

                    if (selected != 1) {
                        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                        getch();
                    }
                    break;
            }
        }
    }
};

#endif // CLONER_H
