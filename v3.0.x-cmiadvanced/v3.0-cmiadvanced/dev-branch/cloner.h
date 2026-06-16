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
#include <sys/wait.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <sys/utsname.h>

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

// EROFS related global variables
namespace ErofsState {
    inline std::atomic<int> current_percentage(1);
    inline std::atomic<bool> display_running(true);
    inline std::mutex display_mutex;
    inline std::chrono::steady_clock::time_point start_time;
    inline int compression_option = 1;
    inline int spinner_pos = 0;
    inline int spinner_char_index = 0;
    inline int spinner_frame_counter = 0;
    inline std::string lz4hc_level = "12";
    inline std::string lzma_level = "109";
}

class Cloner {
public:
    // Get the output directory for SquashFS images
    static std::string getOutputDirectory() {
        std::string dir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img/LiveOS";
        return dir;
    }

    // Create SquashFS with zstd compression (custom compression level)
    static bool createSquashFS(const std::string& inputDir, const std::string& outputFile, const std::string& compLevel = "22") {
        std::string command = "sudo mksquashfs " + inputDir + " " + outputFile +
        " -noappend -comp " + SQUASHFS_COMPRESSION +
        " -Xcompression-level " + compLevel + " -b 256K " +
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

    // Create SquashFS with xz compression
    static bool createSquashFS_xz(const std::string& inputDir, const std::string& outputFile) {
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

    // Clone current system using zstd compression
    static void cloneCurrentSystem(const std::string& cloneDir) {
        if (!mountSystemToCloneDir(cloneDir)) {
            std::cerr << COLOR_RED << "Failed to mount system!" << COLOR_RESET << std::endl;
            return;
        }

        // Ask for compression level
        std::cout << COLOR_CYAN << "Enter zstd compression level ( 1-22 default: 22): " << COLOR_RESET;
        std::string compLevel = getUserInput("");
        if (compLevel.empty()) {
            compLevel = "22";
        }

        // Create SquashFS directly from the mounted bind
        std::string outputDir = getOutputDirectory();
        std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;

        createSquashFS(cloneDir, finalImgPath, compLevel);

        // Unmount the bind mount after SquashFS creation
        std::cout << COLOR_CYAN << "Unmounting bind mount..." << COLOR_RESET << std::endl;
        execute_command("sudo umount " + cloneDir, true);

        createChecksum(finalImgPath);
        printFinalMessage(finalImgPath);

        std::cout << COLOR_GREEN << "Current system cloned successfully using zstd compression (level " << compLevel << ")!" << COLOR_RESET << std::endl;
    }

    // Clone current system using xz compression
    static void cloneCurrentSystem_xz(const std::string& cloneDir) {
        if (!mountSystemToCloneDir(cloneDir)) {
            std::cerr << COLOR_RED << "Failed to mount system!" << COLOR_RESET << std::endl;
            return;
        }

        // Create SquashFS directly from the mounted bind using xz compression
        std::string outputDir = getOutputDirectory();
        std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;

        createSquashFS_xz(cloneDir, finalImgPath);

        // Unmount the bind mount after SquashFS creation
        std::cout << COLOR_CYAN << "Unmounting bind mount..." << COLOR_RESET << std::endl;
        execute_command("sudo umount " + cloneDir, true);

        createChecksum(finalImgPath);
        printFinalMessage(finalImgPath);

        std::cout << COLOR_GREEN << "Current system cloned successfully using xz compression!" << COLOR_RESET << std::endl;
    }

    // EROFS helper functions
    static std::string getErofsFileSize(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            std::streamsize size = file.tellg();
            file.close();

            if (size >= 1073741824) {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << (size / 1073741824.0) << " GB";
                return ss.str();
            } else if (size >= 1048576) {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << (size / 1048576.0) << " MB";
                return ss.str();
            } else if (size >= 1024) {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << (size / 1024.0) << " KB";
                return ss.str();
            } else {
                return std::to_string(size) + " B";
            }
        }
        return "0 B";
    }

    static void showErofsProgressBar(int percentage, const std::string& timer, const std::string& size) {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        int width = w.ws_col;

        std::string timer_display = " [" + timer + "]";
        std::string size_display = " [" + size + "]";
        int extra_width = timer_display.length() + size_display.length();

        int bar_width = width - 9 - extra_width;
        if (bar_width < 10) bar_width = 10;

        int display_percentage = percentage;
        if (display_percentage < 1) display_percentage = 1;

        int filled = (display_percentage * bar_width) / 100;
        if (filled < 1) filled = 1;
        int empty = bar_width - filled;

        const char spinner_chars[] = {'|', '/', '-', '\\'};

        std::lock_guard<std::mutex> lock(ErofsState::display_mutex);
        std::cout << "\r" << COLOR_GREEN << "[";

        for (int i = 0; i < filled; i++) {
            if (i == ErofsState::spinner_pos) {
                std::cout << spinner_chars[ErofsState::spinner_char_index % 4];
            } else {
                std::cout << "=";
            }
        }

        for (int i = 0; i < empty; i++) {
            std::cout << " ";
        }

        std::cout << "] " << std::setw(3) << display_percentage << "%" << COLOR_RESET;
        std::cout << COLOR_GREEN << timer_display << size_display << COLOR_RESET;
        std::cout.flush();
    }

    static void updateErofsDisplay(const std::string& outputFile) {
        while (ErofsState::display_running) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - ErofsState::start_time);
            int minutes = elapsed.count() / 60;
            int seconds = elapsed.count() % 60;

            std::stringstream timer_ss;
            timer_ss << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << seconds;
            std::string timer = timer_ss.str();

            std::string size = getErofsFileSize(outputFile);

            int percentage = ErofsState::current_percentage.load();
            int display_percentage = percentage;
            if (display_percentage < 1) display_percentage = 1;

            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            int width = w.ws_col;
            std::string timer_display = " [" + timer + "]";
            std::string size_display = " [" + size + "]";
            int extra_width = timer_display.length() + size_display.length();
            int bar_width = width - 9 - extra_width;
            if (bar_width < 10) bar_width = 10;
            int filled = (display_percentage * bar_width) / 100;
            if (filled < 1) filled = 1;

            ErofsState::spinner_frame_counter++;
            if (ErofsState::spinner_frame_counter >= 3) {
                ErofsState::spinner_frame_counter = 0;
                if (ErofsState::spinner_pos < filled - 1) {
                    ErofsState::spinner_pos++;
                }
                ErofsState::spinner_char_index++;
            }

            showErofsProgressBar(percentage, timer, size);

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    static void createErofsImage(const std::string& cloneDir, const std::string& outputFile) {
        std::cout << COLOR_CYAN << "Creating EROFS image..." << COLOR_RESET << std::endl;

        execute_command("mkdir -p " + getOutputDirectory(), true);

        execute_command("sudo cp -a /boot " + cloneDir + "/ > /dev/null 2>&1", true);

        std::string logFile = getOutputDirectory() + "/log.txt";
        execute_command("rm -f " + logFile, true);

        // Hide cursor
        std::cout << "\033[?25l";

        ErofsState::current_percentage = 1;
        ErofsState::spinner_pos = 0;
        ErofsState::spinner_char_index = 0;
        ErofsState::spinner_frame_counter = 0;

        ErofsState::start_time = std::chrono::steady_clock::now();

        ErofsState::display_running = true;
        std::thread display_thread(updateErofsDisplay, outputFile);

        std::string compression_args;
        if (ErofsState::compression_option == 1) {
            compression_args = "-zlz4hc,level=" + ErofsState::lz4hc_level + ",";
        } else {
            compression_args = "-zlzma,level=" + ErofsState::lzma_level + ",dictsize=8388608";
        }

        // Build exclusions with paths that don't start with /
        std::string exclusions = "";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + " ";
        exclusions += "--exclude-path=" + getOutputDirectory().substr(1) + " ";
        exclusions += "--exclude-path=" + outputFile.substr(1) + " ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/etc/udev/rules.d/70-persistent-cd.rules ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/etc/udev/rules.d/70-persistent-net.rules ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/etc/mtab ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/etc/fstab ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/dev/* ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/proc/* ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/sys/* ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/tmp/* ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/run/* ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/mnt/* ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/lost+found ";
        exclusions += "--exclude-path=" + cloneDir.substr(1) + "/clone_system_temp";

        std::string cmd = "sudo mkfs.erofs "
        "-d9 "
        + compression_args + " "
        "-C1048576 "
        + exclusions + " "
        + outputFile + " "
        + cloneDir + " > " + logFile + " 2>&1 &";

        system(cmd.c_str());

        int seconds_elapsed = 0;
        bool reached_60 = false;
        bool uuid_detected = false;
        int uuid_wait_counter = 0;

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            seconds_elapsed++;

            if (!reached_60) {
                int progress = seconds_elapsed / 10;
                if (progress < 1) progress = 1;
                if (progress >= 60) {
                    reached_60 = true;
                } else {
                    ErofsState::current_percentage = progress;
                }
            }

            if (reached_60) {
                // ALWAYS keep at 60% while waiting for UUID
                ErofsState::current_percentage = 60;

                if (!uuid_detected) {
                    std::ifstream log_file(logFile);
                    if (log_file.is_open()) {
                        std::string line;
                        while (std::getline(log_file, line)) {
                            if (line.find("Filesystem UUID") != std::string::npos || line.find("Filesystem UUID") != std::string::npos) {
                                uuid_detected = true;
                                break;
                            }
                        }
                        log_file.close();
                    }
                }

                if (uuid_detected) {
                    uuid_wait_counter++;
                    if (uuid_wait_counter >= 10) {
                        ErofsState::current_percentage = 100;
                        break;
                    }
                    // Keep at 60% during the 10 second wait
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

        ErofsState::display_running = false;
        display_thread.join();

        std::cout << std::endl;

        // Show cursor
        std::cout << "\033[?25h";

        std::cout << COLOR_GREEN << "EROFS image created successfully: " << outputFile << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "Image size: ";
        execute_command("sudo du -h " + outputFile + " | cut -f1", true);
        std::cout << COLOR_RESET;
    }

    // Clone current system using EROFS Fast compression
    static void cloneCurrentSystemErofsFast(const std::string& cloneDir) {
        ErofsState::compression_option = 1;

        // Ask for LZ4HC compression level
        std::cout << COLOR_CYAN << "Enter lz4hc compression level (1-12 default: 12): " << COLOR_RESET;
        std::string level = getUserInput("");
        if (level.empty()) {
            ErofsState::lz4hc_level = "12";
        } else {
            ErofsState::lz4hc_level = level;
        }

        if (!mountSystemToCloneDir(cloneDir)) {
            std::cerr << COLOR_RED << "Failed to mount system!" << COLOR_RESET << std::endl;
            return;
        }

        std::string outputDir = getOutputDirectory();
        std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;

        createErofsImage(cloneDir, finalImgPath);

        std::cout << COLOR_CYAN << "Unmounting bind mount..." << COLOR_RESET << std::endl;
        execute_command("sudo umount " + cloneDir, true);

        createChecksum(finalImgPath);

        std::cout << COLOR_GREEN << "Current system cloned successfully using erofs lz4hc compression (level " << ErofsState::lz4hc_level << ")!" << COLOR_RESET << std::endl;
    }

    // Clone current system using EROFS Max compression
    static void cloneCurrentSystemErofsMax(const std::string& cloneDir) {
        ErofsState::compression_option = 2;

        // Ask for LZMA compression level
        std::cout << COLOR_CYAN << "Enter lzma compression level (1-109 default: 109): " << COLOR_RESET;
        std::string level = getUserInput("");
        if (level.empty()) {
            ErofsState::lzma_level = "109";
        } else {
            ErofsState::lzma_level = level;
        }

        if (!mountSystemToCloneDir(cloneDir)) {
            std::cerr << COLOR_RED << "Failed to mount system!" << COLOR_RESET << std::endl;
            return;
        }

        std::string outputDir = getOutputDirectory();
        std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;

        createErofsImage(cloneDir, finalImgPath);

        std::cout << COLOR_CYAN << "Unmounting bind mount..." << COLOR_RESET << std::endl;
        execute_command("sudo umount " + cloneDir, true);

        createChecksum(finalImgPath);

        std::cout << COLOR_GREEN << "Current system cloned successfully using erofs lzma compression (level " << ErofsState::lzma_level << ")!" << COLOR_RESET << std::endl;
    }

    // Show clone options menu
    static void showCloneOptionsMenu(bool allCheckboxesChecked, const std::string& cloneDirConfig) {
        std::vector<std::string> items = {
            "--- Squashfs Slow Compression Options ---",
            "Clone Current System (xz compression)",
            "Clone Current System (zstd compression)",
            "--- Erofs Fast Options ---",
            "Clone Current System (Lz4hc compression)",
            "Clone Current System (Lzma Max compression)",
            "Back to Main Menu"
        };

        int selected = 2; // Start on first actual option, skip the header
        int key;

        while (true) {
            // Call the free function showMenu, not Cloner::showMenu
            key = showMenu("Clone Options - Select Source:", items, selected);

            if (key == -1) {
                continue;
            }

            switch (key) {
                case 'A':
                    if (selected > 0) {
                        selected--;
                        // Skip headers
                        if (selected == 0 || selected == 3) selected--;
                    }
                    break;
                case 'B':
                    if (selected < static_cast<int>(items.size()) - 1) {
                        selected++;
                        // Skip headers
                        if (selected == 0 || selected == 3) selected++;
                    }
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

                    // Skip if header is selected
                    if (selected == 0 || selected == 3) {
                        break;
                    }

                    std::string cloneDir = expandPath(cloneDirConfig);
                    execute_command("sudo mkdir -p " + cloneDir, true);

                    switch (selected) {
                        case 1:
                            cloneCurrentSystem_xz(cloneDir);
                            break;
                        case 2:
                            cloneCurrentSystem(cloneDir);
                            break;
                        case 4:
                            cloneCurrentSystemErofsFast(cloneDir);
                            break;
                        case 5:
                            cloneCurrentSystemErofsMax(cloneDir);
                            break;
                        case 6:
                            return;
                    }

                    if (selected != 6) {
                        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                        getch();
                    }
                    break;
            }
        }
    }
};

#endif // CLONER_H
