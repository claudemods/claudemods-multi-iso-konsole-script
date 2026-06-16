#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <iomanip>
#include <sys/ioctl.h>
#include <chrono>
#include <thread>
#include <sstream>
#include <fstream>
#include <atomic>
#include <mutex>
#include <sys/utsname.h>

// Colors for output
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string CYAN = "\033[38;2;0;255;255m";
const std::string YELLOW = "\033[33m";
const std::string RESET = "\033[0m";

// Global variables for display thread
std::atomic<int> current_percentage(1);
std::atomic<bool> display_running(true);
std::mutex display_mutex;
std::chrono::steady_clock::time_point start_time;

// Global variable for compression type
int compression_option = 1;

// Spinner state
int spinner_pos = 0;
int spinner_char_index = 0;
int spinner_frame_counter = 0;

// Function to print colored output
void print_info(const std::string& msg) {
    std::cout << GREEN << msg << RESET << std::endl;
}

void print_success(const std::string& msg) {
    std::cout << GREEN << msg << RESET << std::endl;
}

void print_error(const std::string& msg) {
    std::cout << RED << msg << RESET << std::endl;
}

void print_warning(const std::string& msg) {
    std::cout << YELLOW << msg << RESET << std::endl;
}

// Function to check if running as root
void check_root() {
    if (geteuid() == 0) {
        print_error("This script should not be run as root. Please run as regular user and use sudo when needed.");
        exit(1);
    }
}

// Function to create and mount bind directory
void setup_bind_mount() {
    print_info("Setting up bind mount...");

    system("sudo mkdir -p \"$HOME/clone_system_temp\"");
    system("sudo chown $USER:$USER \"$HOME/clone_system_temp\"");
    system("sudo mount --bind / \"$HOME/clone_system_temp\"");

    print_success("Bind mount created successfully: / -> $HOME/clone_system_temp");
}

// Function to get file size in human readable format
std::string get_file_size(const std::string& path) {
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

// Function to show progress bar with spinner
void show_progress_bar(int percentage, const std::string& timer, const std::string& size) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int width = w.ws_col;

    // Calculate space needed for timer and size
    std::string timer_display = " [" + timer + "]";
    std::string size_display = " [" + size + "]";
    int extra_width = timer_display.length() + size_display.length();

    // Make progress bar smaller to accommodate timer and size
    int bar_width = width - 9 - extra_width;
    if (bar_width < 10) bar_width = 10;

    // Ensure percentage is at least 1 for display
    int display_percentage = percentage;
    if (display_percentage < 1) display_percentage = 1;

    int filled = (display_percentage * bar_width) / 100;
    if (filled < 1) filled = 1;
    int empty = bar_width - filled;

    // mksquashfs spinner characters
    const char spinner_chars[] = {'|', '/', '-', '\\'};

    std::lock_guard<std::mutex> lock(display_mutex);
    std::cout << "\r" << GREEN << "[";

    // Print filled portion with spinner
    for (int i = 0; i < filled; i++) {
        if (i == spinner_pos) {
            std::cout << spinner_chars[spinner_char_index % 4];
        } else {
            std::cout << "=";
        }
    }

    // Print empty portion
    for (int i = 0; i < empty; i++) {
        std::cout << " ";
    }

    std::cout << "] " << std::setw(3) << display_percentage << "%" << RESET;
    std::cout << GREEN << timer_display << size_display << RESET;
    std::cout.flush();
}

// Display update thread function
void update_display() {
    while (display_running) {
        // Calculate elapsed time
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
        int minutes = elapsed.count() / 60;
        int seconds = elapsed.count() % 60;

        std::stringstream timer_ss;
        timer_ss << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << seconds;
        std::string timer = timer_ss.str();

        // Get current file size
        std::string size = get_file_size("clone/rootfs.img");

        // Update spinner
        int percentage = current_percentage.load();
        // Ensure percentage is at least 1 for display
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

        // Move spinner right every 3 frames
        spinner_frame_counter++;
        if (spinner_frame_counter >= 3) {
            spinner_frame_counter = 0;
            // Only move right, stop at the end
            if (spinner_pos < filled - 1) {
                spinner_pos++;
            }
            spinner_char_index++;
        }

        // Update display
        show_progress_bar(percentage, timer, size);

        // Update every 100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Function to choose compression type
void choose_compression() {
    std::cout << std::endl;
    std::cout << GREEN << "Choose compression type:" << RESET << std::endl;
    std::cout << GREEN << "  [1] Fast - Medium compression (LZ4HC, level 12)" << RESET << std::endl;
    std::cout << GREEN << "  [2] Slow - Maximum compression (LZMA, level 109, dict 2MB)" << RESET << std::endl;
    std::cout << std::endl;
    std::cout << GREEN << "Enter your choice (1 or 2): " << RESET;

    while (true) {
        std::string input;
        std::getline(std::cin, input);

        if (input == "1") {
            compression_option = 1;
            print_success("Selected: Fast compression (LZ4HC, level 12)");
            break;
        } else if (input == "2") {
            compression_option = 2;
            print_success("Selected: Slow maximum compression (LZMA, level 109, dict 2MB)");
            break;
        } else {
            std::cout << RED << "Invalid choice. Please enter 1 or 2: " << RESET;
        }
    }
    std::cout << std::endl;
}

// Function to create EROFS with exclusions
void create_erofs() {
    print_info("Creating EROFS image...");

    system("mkdir -p clone");

    system("sudo cp -a /boot /home/$USER/clone_system_temp/ > /dev/null 2>&1");

    // Remove old log file if exists
    system("rm -f clone/log.txt");

    // Hide cursor
    std::cout << "\033[?25l";

    // Set initial percentage to 1 so bar shows from 1%
    current_percentage = 1;
    spinner_pos = 0;
    spinner_char_index = 0;
    spinner_frame_counter = 0;

    // Record start time
    start_time = std::chrono::steady_clock::now();

    // Start display update thread
    display_running = true;
    std::thread display_thread(update_display);

    // Build compression option string
    std::string compression_args;
    if (compression_option == 1) {
        compression_args = "-zlz4hc,level=12,";
    } else {
        compression_args = "-zlzma,level=109,dictsize=8388608";
    }

    // Build the mkfs.erofs command
    std::string cmd = "sudo mkfs.erofs "
    "-d9 "
    + compression_args + " "
    "-C1048576 "
    "--exclude-path=home/$USER/clone_system_temp "
    "--exclude-path=home/$USER/Downloads/clone "
    "--exclude-path=home/$USER/Downloads/clone/rootfs.img "
    "--exclude-path=home/$USER/clone_system_temp/etc/udev/rules.d/70-persistent-cd.rules "
    "--exclude-path=home/$USER/clone_system_temp/etc/udev/rules.d/70-persistent-net.rules "
    "--exclude-path=home/$USER/clone_system_temp/etc/mtab "
    "--exclude-path=home/$USER/clone_system_temp/etc/fstab "
    "--exclude-path=home/$USER/clone_system_temp/dev/* "
    "--exclude-path=home/$USER/clone_system_temp/proc/* "
    "--exclude-path=home/$USER/clone_system_temp/sys/* "
    "--exclude-path=home/$USER/clone_system_temp/tmp/* "
    "--exclude-path=home/$USER/clone_system_temp/run/* "
    "--exclude-path=home/$USER/clone_system_temp/mnt/* "
    "--exclude-path=home/$USER/clone_system_temp/lost+found "
    "--exclude-path=home/$USER/clone_system_temp/clone "
    "clone/rootfs.img "
    "\"$HOME/clone_system_temp\" > clone/log.txt 2>&1 &";

    // Run mkfs.erofs in background
    system(cmd.c_str());

    // Monitor progress
    int seconds_elapsed = 0;
    bool reached_60 = false;
    bool uuid_detected = false;
    int uuid_wait_counter = 0;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        seconds_elapsed++;

        // Update progress: 1% every 10 seconds up to 60
        if (!reached_60) {
            int progress = seconds_elapsed / 10;
            if (progress < 1) progress = 1; // Keep at least 1%
            if (progress >= 60) {
                current_percentage = 60;
                reached_60 = true;
            } else {
                current_percentage = progress;
            }
        }

        // Only check for UUID after reaching 60%
        if (reached_60 && !uuid_detected) {
            std::ifstream log_file("clone/log.txt");
            if (log_file.is_open()) {
                std::string line;
                while (std::getline(log_file, line)) {
                    if (line.find("uuid") != std::string::npos || line.find("UUID") != std::string::npos) {
                        uuid_detected = true;
                        break;
                    }
                }
                log_file.close();
            }
        }

        // If uuid detected, wait 10 seconds then go to 100%
        if (uuid_detected) {
            uuid_wait_counter++;
            if (uuid_wait_counter >= 10) {
                current_percentage = 100;
                break;
            }
        }
    }

    // Let display update one final time
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Stop display thread
    display_running = false;
    display_thread.join();

    std::cout << std::endl;

    // Show cursor
    std::cout << "\033[?25h";

    print_success("EROFS image created successfully: clone/rootfs.img");

    // Show file size in green
    print_info("Image size:");
    system("sudo du -h clone/rootfs.img | cut -f1");
}

// Function to clean up
void cleanup() {
    print_info("Cleaning up...");

    system("sudo umount \"$HOME/clone_system_temp\"");
    print_success("Bind mount unmounted: $HOME/clone_system_temp");

    system("sudo rm -rf \"$HOME/clone_system_temp\"");
    print_success("Temporary directory cleaned up: $HOME/clone_system_temp");
}

// Function to show disk usage
void show_disk_usage() {
    print_info("Current disk usage:");

    // Capture df output and print in green
    FILE* pipe = popen("df -h / | tail -1", "r");
    if (pipe) {
        char buffer[256];
        std::string df_output;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            df_output += buffer;
        }
        pclose(pipe);
        std::cout << GREEN << df_output << RESET;
    }

    print_info("System Information:");

    // Get distribution info from /etc/os-release
    std::string distro_info = "Unknown";
    std::ifstream os_release("/etc/os-release");
    if (os_release.is_open()) {
        std::string line;
        while (std::getline(os_release, line)) {
            if (line.find("PRETTY_NAME=") == 0) {
                size_t start = line.find('"');
                size_t end = line.find('"', start + 1);
                if (start != std::string::npos && end != std::string::npos) {
                    distro_info = line.substr(start + 1, end - start - 1);
                }
                break;
            }
        }
        os_release.close();
    }
    std::cout << GREEN << "  Distribution: " << distro_info << RESET << std::endl;

    // Get kernel info using uname syscall
    struct utsname kernel_info;
    if (uname(&kernel_info) == 0) {
        std::cout << GREEN << "  Kernel: " << kernel_info.release << RESET << std::endl;
    }
}

// Main execution
int main() {
    signal(SIGINT, [](int) {
        display_running = false;
        print_error("Script interrupted. Cleaning up...");
        system("sudo umount \"$HOME/clone_system_temp\" 2>/dev/null");
        system("sudo rm -rf \"$HOME/clone_system_temp\" 2>/dev/null");
        exit(1);
    });
    signal(SIGTERM, [](int) {
        display_running = false;
        print_error("Script interrupted. Cleaning up...");
        system("sudo umount \"$HOME/clone_system_temp\" 2>/dev/null");
        system("sudo rm -rf \"$HOME/clone_system_temp\" 2>/dev/null");
        exit(1);
    });

    print_info("Starting system clone and EROFS creation using bind mount...");
    check_root();
    show_disk_usage();
    std::cout << std::endl;
    choose_compression();
    setup_bind_mount();
    std::cout << std::endl;
    create_erofs();
    std::cout << std::endl;
    cleanup();
    std::cout << std::endl;
    show_disk_usage();
    std::cout << std::endl;
    print_success("Process completed successfully!");
    print_info("Final image: clone/rootfs.img");

    return 0;
}
