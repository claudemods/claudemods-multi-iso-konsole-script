i need a new last option on main menu called cmi btrfs/ext4 installer when selected execute command cmiinstaller i need a new option to select as option 1 on the main  menu called guide when selected execute command nano /home/$USER/.config/cmi/readme.txt also show .isos in the output dirctorty set if its set and make a new last menu option install iso to usb and when selected also show isos but let user select one then ask for drive e.g /dev/sda and dd iso to the drive i need full code with the changes do not change or remove anything else #include <iostream>
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
#include <pwd.h>
#include <ctime>
#include <termios.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <atomic>
#include <mutex>
#include <thread>

// Forward declarations
void saveConfig();
void execute_command(const std::string& cmd, bool continueOnError = false);
void printCheckbox(bool checked);
std::string getUserInput(const std::string& prompt);
void clearScreen();
int getch();
int kbhit();

// Time-related globals
std::atomic<bool> time_thread_running(true);
std::mutex time_mutex;
std::string current_time_str;
bool should_reset = false;

// Constants
const std::string ORIG_IMG_NAME = "system.img";
const std::string COMPRESSED_IMG_NAME = "rootfs.img";
std::string MOUNT_POINT = "/mnt/btrfs_temp";
const std::string SOURCE_DIR = "/";
const std::string COMPRESSION_LEVEL = "22";
const std::string SQUASHFS_COMPRESSION = "zstd";
const std::vector<std::string> SQUASHFS_COMPRESSION_ARGS = {"-Xcompression-level", "22"};
std::string BUILD_DIR = "/home/$USER/.config/cmi/build-image-arch-img";
std::string USERNAME = "";
const std::string BTRFS_LABEL = "LIVE_SYSTEM";
const std::string BTRFS_COMPRESSION = "zstd";

// Dependencies list
const std::vector<std::string> DEPENDENCIES = {
    "rsync",
    "squashfs-tools",
    "xorriso",
    "grub",
    "dosfstools",
    "unzip",
    "arch-install-scripts",
    "bash-completion",
    "erofs-utils",
    "findutils",
    "unzip",
    "jq",
    "libarchive",
    "libisoburn",
    "lsb-release",
    "lvm2",
    "mkinitcpio-archiso",
    "mkinitcpio-nfs-utils",
    "mtools",
    "nbd",
    "pacman-contrib",
    "parted",
    "procps-ng",
    "pv",
    "python",
    "sshfs",
    "syslinux",
    "xdg-utils",
    "zsh-completions",
    "kernel-modules-hook",
    "virt-manager",
    "qt6-tools",
    "qt5-tools"
};

// Configuration state
struct ConfigState {
    std::string isoTag;
    std::string isoName;
    std::string outputDir;
    std::string vmlinuzPath;
    bool mkinitcpioGenerated = false;
    bool grubEdited = false;
    bool dependenciesInstalled = false;

    bool isReadyForISO() const {
        return !isoTag.empty() && !isoName.empty() && !outputDir.empty() &&
        !vmlinuzPath.empty() && mkinitcpioGenerated && grubEdited && dependenciesInstalled;
    }
} config;

// ANSI color codes
const std::string COLOR_RED = "\033[31m";
const std::string COLOR_GREEN = "\033[32m";
const std::string COLOR_BLUE = "\033[34m";
const std::string COLOR_CYAN = "\033[38;2;0;255;255m";
const std::string COLOR_YELLOW = "\033[33m";
const std::string COLOR_RESET = "\033[0m";
const std::string COLOR_HIGHLIGHT = "\033[38;2;0;255;255m";
const std::string COLOR_NORMAL = "\033[34m";

void update_time_thread() {
    while (time_thread_running) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char datetime[50];
        strftime(datetime, sizeof(datetime), "%d/%m/%Y %H:%M:%S", t);

        {
            std::lock_guard<std::mutex> lock(time_mutex);
            current_time_str = datetime;
        }
        sleep(1);
    }
}

void clearScreen() {
    std::cout << "\033[2J\033[1;1H";
}

int getch() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

void execute_command(const std::string& cmd, bool continueOnError) {
    std::cout << COLOR_CYAN;
    fflush(stdout);
    int status = system(cmd.c_str());
    std::cout << COLOR_RESET;
    if (status != 0 && !continueOnError) {
        std::cerr << COLOR_RED << "Error executing: " << cmd << COLOR_RESET << std::endl;
        exit(1);
    } else if (status != 0) {
        std::cerr << COLOR_YELLOW << "Command failed but continuing: " << cmd << COLOR_RESET << std::endl;
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
    clearScreen();

    std::cout << COLOR_RED << R"(
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚══════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
)" << COLOR_RESET << std::endl;
std::cout << COLOR_CYAN << " Advanced C++ Arch Img Iso Script Beta v2.01 24-07-2025" << COLOR_RESET << std::endl;

// Show current UK time
{
    std::lock_guard<std::mutex> lock(time_mutex);
    std::cout << COLOR_BLUE << "Current UK Time: " << COLOR_CYAN << current_time_str << COLOR_RESET << std::endl;
}

// Show disk usage information
std::cout << COLOR_GREEN << "Filesystem      Size  Used Avail Use% Mounted on" << COLOR_RESET << std::endl;
execute_command("df -h / | tail -1");
std::cout << std::endl;
}

void printConfigStatus() {
    std::cout << COLOR_CYAN << "Current Configuration:" << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(config.dependenciesInstalled);
    std::cout << " Dependencies Installed" << std::endl;

    std::cout << " ";
    printCheckbox(!config.isoTag.empty());
    std::cout << " ISO Tag: " << (config.isoTag.empty() ? COLOR_YELLOW + "Not set" : COLOR_CYAN + config.isoTag) << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(!config.isoName.empty());
    std::cout << " ISO Name: " << (config.isoName.empty() ? COLOR_YELLOW + "Not set" : COLOR_CYAN + config.isoName) << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(!config.outputDir.empty());
    std::cout << " Output Directory: " << (config.outputDir.empty() ? COLOR_YELLOW + "Not set" : COLOR_CYAN + config.outputDir) << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(!config.vmlinuzPath.empty());
    std::cout << " vmlinuz Selected: " << (config.vmlinuzPath.empty() ? COLOR_YELLOW + "Not selected" : COLOR_CYAN + config.vmlinuzPath) << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(config.mkinitcpioGenerated);
    std::cout << " mkinitcpio Generated" << std::endl;

    std::cout << " ";
    printCheckbox(config.grubEdited);
    std::cout << " GRUB Config Edited" << std::endl;
}

std::string getUserInput(const std::string& prompt) {
    std::cout << COLOR_GREEN << prompt << COLOR_RESET;
    std::string input;
    std::getline(std::cin, input);
    return input;
}

void installDependencies() {
    std::cout << COLOR_CYAN << "\nInstalling required dependencies...\n" << COLOR_RESET;

    // Build package list string
    std::string packages;
    for (const auto& pkg : DEPENDENCIES) {
        packages += pkg + " ";
    }

    std::string command = "sudo pacman -Sy --needed --noconfirm " + packages;
    execute_command(command);

    config.dependenciesInstalled = true;
    saveConfig();
    std::cout << COLOR_GREEN << "\nDependencies installed successfully!\n" << COLOR_RESET << std::endl;
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
            saveConfig();
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
    saveConfig();
    std::cout << COLOR_GREEN << "mkinitcpio generated successfully!" << COLOR_RESET << std::endl;
}

void editGrubCfg() {
    if (BUILD_DIR.empty()) {
        std::cerr << COLOR_RED << "Build directory not set!" << COLOR_RESET << std::endl;
        return;
    }

    std::string grubCfgPath = BUILD_DIR + "/boot/grub/grub.cfg";
    std::cout << COLOR_CYAN << "Editing GRUB config: " << grubCfgPath << COLOR_RESET << std::endl;
    execute_command("sudo nano " + grubCfgPath);

    config.grubEdited = true;
    saveConfig();
    std::cout << COLOR_GREEN << "GRUB config edited!" << COLOR_RESET << std::endl;
}

void setIsoTag() {
    config.isoTag = getUserInput("Enter ISO tag (e.g., 2025): ");
    saveConfig();
}

void setIsoName() {
    config.isoName = getUserInput("Enter ISO name (e.g., claudemods.iso): ");
    saveConfig();
}

void setOutputDir() {
    std::string defaultDir = "/home/" + USERNAME + "/Downloads";
    std::cout << COLOR_GREEN << "Current output directory: " << (config.outputDir.empty() ? COLOR_YELLOW + "Not set" : COLOR_CYAN + config.outputDir) << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Default directory: " << COLOR_CYAN << defaultDir << COLOR_RESET << std::endl;
    config.outputDir = getUserInput("Enter output directory (e.g., " + defaultDir + " or $USER/Downloads): ");

    // Replace $USER with actual username
    size_t user_pos;
    if ((user_pos = config.outputDir.find("$USER")) != std::string::npos) {
        config.outputDir.replace(user_pos, 5, USERNAME);
    }

    // If empty, use default
    if (config.outputDir.empty()) {
        config.outputDir = defaultDir;
    }
    saveConfig();
}

std::string getConfigFilePath() {
    return "/home/" + USERNAME + "/.config/cmi/configuration.txt";
}

void saveConfig() {
    std::string configPath = getConfigFilePath();
    std::ofstream configFile(configPath);
    if (configFile.is_open()) {
        configFile << "isoTag=" << config.isoTag << "\n";
        configFile << "isoName=" << config.isoName << "\n";
        configFile << "outputDir=" << config.outputDir << "\n";
        configFile << "vmlinuzPath=" << config.vmlinuzPath << "\n";
        configFile << "mkinitcpioGenerated=" << (config.mkinitcpioGenerated ? "1" : "0") << "\n";
        configFile << "grubEdited=" << (config.grubEdited ? "1" : "0") << "\n";
        configFile << "dependenciesInstalled=" << (config.dependenciesInstalled ? "1" : "0") << "\n";
        configFile.close();
    } else {
        std::cerr << COLOR_RED << "Failed to save configuration to " << configPath << COLOR_RESET << std::endl;
    }
}

void loadConfig() {
    std::string configPath = getConfigFilePath();
    std::ifstream configFile(configPath);
    if (configFile.is_open()) {
        std::string line;
        while (std::getline(configFile, line)) {
            size_t delimiter = line.find('=');
            if (delimiter != std::string::npos) {
                std::string key = line.substr(0, delimiter);
                std::string value = line.substr(delimiter + 1);

                if (key == "isoTag") config.isoTag = value;
                else if (key == "isoName") config.isoName = value;
                else if (key == "outputDir") config.outputDir = value;
                else if (key == "vmlinuzPath") config.vmlinuzPath = value;
                else if (key == "mkinitcpioGenerated") config.mkinitcpioGenerated = (value == "1");
                else if (key == "grubEdited") config.grubEdited = (value == "1");
                else if (key == "dependenciesInstalled") config.dependenciesInstalled = (value == "1");
            }
        }
        configFile.close();
    }
}

int showMenu(const std::string &title, const std::vector<std::string> &items, int selected) {
    clearScreen();
    printBanner();
    printConfigStatus();

    std::cout << COLOR_CYAN << "\n  " << title << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "  " << std::string(title.length(), '-') << COLOR_RESET << std::endl;

    for (size_t i = 0; i < items.size(); i++) {
        if (i == static_cast<size_t>(selected)) {
            std::cout << COLOR_HIGHLIGHT << "➤ " << items[i] << COLOR_RESET << "\n";
        } else {
            std::cout << COLOR_NORMAL << "  " << items[i] << COLOR_RESET << "\n";
        }
    }

    return getch();
}

int showFilesystemMenu(const std::vector<std::string> &items, int selected) {
    clearScreen();
    printBanner();

    std::cout << COLOR_CYAN << "\n  Choose filesystem type:" << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "  ----------------------" << COLOR_RESET << std::endl;

    for (size_t i = 0; i < items.size(); i++) {
        if (i == static_cast<size_t>(selected)) {
            std::cout << COLOR_HIGHLIGHT << "➤ " << items[i] << COLOR_RESET << "\n";
        } else {
            std::cout << COLOR_NORMAL << "  " << items[i] << COLOR_RESET << "\n";
        }
    }

    return getch();
}

void showSetupMenu() {
    std::vector<std::string> items = {
        "Install Dependencies",
        "Set ISO Tag",
        "Set ISO Name",
        "Set Output Directory",
        "Select vmlinuz",
        "Generate mkinitcpio",
        "Edit GRUB Config",
        "Back to Main Menu"
    };

    int selected = 0;
    int key;

    while (true) {
        key = showMenu("ISO Creation Setup Menu:", items, selected);

        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < static_cast<int>(items.size()) - 1) selected++;
                break;
            case '\n': // Enter key
                switch (selected) {
                    case 0: installDependencies(); break;
                    case 1: setIsoTag(); break;
                    case 2: setIsoName(); break;
                    case 3: setOutputDir(); break;
                    case 4: selectVmlinuz(); break;
                    case 5: generateMkinitcpio(); break;
                    case 6: editGrubCfg(); break;
                    case 7: return;
                }

                // Pause to show result before returning to menu
                if (selected != 7) {
                    std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                    getch();
                }
                break;
        }
    }
}

bool createImageFile(const std::string& size, const std::string& filename) {
    std::string command = "sudo truncate -s " + size + " " + filename;
    execute_command(command, true);
    return true;
}

bool formatFilesystem(const std::string& fsType, const std::string& filename) {
    if (fsType == "btrfs") {
        std::cout << COLOR_CYAN << "Creating Btrfs filesystem with " << BTRFS_COMPRESSION << " compression" << COLOR_RESET << std::endl;

        // Create temporary rootdir
        execute_command("sudo mkdir -p " + SOURCE_DIR + "/btrfs_rootdir", true);

        std::string command = "sudo mkfs.btrfs -L \"" + BTRFS_LABEL + "\" --compress=" + BTRFS_COMPRESSION +
        " --rootdir=" + SOURCE_DIR + "/btrfs_rootdir -f " + filename;
        execute_command(command, true);

        // Cleanup temporary rootdir
        execute_command("sudo rmdir " + SOURCE_DIR + "/btrfs_rootdir", true);
    } else {
        std::cout << COLOR_CYAN << "Formatting as ext4" << COLOR_RESET << std::endl;
        std::string command = "sudo mkfs.ext4 -F -L \"SYSTEM_BACKUP\" " + filename;
        execute_command(command, true);
    }
    return true;
}

bool mountFilesystem(const std::string& fsType, const std::string& filename, const std::string& mountPoint) {
    execute_command("sudo mkdir -p " + mountPoint, true);

    if (fsType == "btrfs") {
        std::string command = "sudo mount " + filename + " " + mountPoint;
        execute_command(command, true);
    } else {
        std::string options = "loop,discard,noatime";
        std::string command = "sudo mount -o " + options + " " + filename + " " + mountPoint;
        execute_command(command, true);
    }
    return true;
}

bool copyFilesWithRsync(const std::string& source, const std::string& destination, const std::string& fsType) {
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
    "--exclude=system.img "
    "--exclude=rootfs.img " +
    source + " " + destination;
    execute_command(command, true);

    if (fsType == "btrfs") {
        // Optimize compression after copy
        std::cout << COLOR_CYAN << "Optimizing compression..." << COLOR_RESET << std::endl;
        execute_command("sudo btrfs filesystem defrag -r -v -c " + BTRFS_COMPRESSION + " " + destination, true);

        // Shrink to minimum size
        std::cout << COLOR_CYAN << "Finalizing image..." << COLOR_RESET << std::endl;
        execute_command("sudo btrfs filesystem resize max " + destination, true);
    }

    return true;
}

bool unmountAndCleanup(const std::string& mountPoint) {
    sync();
    try {
        execute_command("sudo umount " + mountPoint, true);
        execute_command("sudo rmdir " + mountPoint, true);
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
    execute_command(command, true);
    return true;
}

bool createChecksum(const std::string& filename) {
    std::string command = "md5sum " + filename + " > " + filename + ".md5";
    execute_command(command, true);
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
    std::cout << COLOR_CYAN << "Size: ";
    execute_command("sudo du -h " + squashfsOutput + " | cut -f1", true);
    std::cout << COLOR_RESET;
}

void deleteOriginalImage(const std::string& imgName) {
    std::cout << COLOR_CYAN << "Deleting original image file: " << imgName << COLOR_RESET << std::endl;
    execute_command("sudo rm -f " + imgName, true);
}

std::string getOutputDirectory() {
    std::string dir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img/LiveOS";
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
        return false;
    }

    std::cout << COLOR_CYAN << "\nStarting ISO creation process...\n" << COLOR_RESET;

    std::string expandedOutputDir = expandPath(config.outputDir);

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

    execute_command(xorrisoCmd, true);
    std::cout << COLOR_CYAN << "ISO created successfully at " << expandedOutputDir << "/" << config.isoName << COLOR_RESET << std::endl;
    return true;
}

void showMainMenu() {
    std::vector<std::string> items = {
        "Setup Scripts",
        "Create Image",
        "Create ISO",
        "Show Disk Usage",
        "Exit"
    };

    int selected = 0;
    int key;

    while (true) {
        key = showMenu("Main Menu:", items, selected);

        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < static_cast<int>(items.size()) - 1) selected++;
                break;
            case '\n': // Enter key
                switch (selected) {
                    case 0:
                        showSetupMenu();
                        break;
                    case 1: {
                        std::string outputDir = getOutputDirectory();
                        std::string outputOrigImgPath = outputDir + "/" + ORIG_IMG_NAME;
                        std::string outputCompressedImgPath = outputDir + "/" + COMPRESSED_IMG_NAME;

                        // Cleanup old files
                        execute_command("sudo umount " + MOUNT_POINT + " 2>/dev/null || true", true);
                        execute_command("sudo rm -rf " + outputOrigImgPath, true);
                        execute_command("sudo mkdir -p " + MOUNT_POINT, true);
                        execute_command("sudo mkdir -p " + outputDir, true);

                        std::string imgSize = getUserInput("Enter the image size in GB (e.g., 6 for 6GB): ") + "G";

                        std::vector<std::string> fsOptions = {"btrfs", "ext4"};
                        int fsSelected = 0;
                        int fsKey;
                        bool fsChosen = false;

                        while (!fsChosen) {
                            fsKey = showFilesystemMenu(fsOptions, fsSelected);

                            switch (fsKey) {
                                case 'A': // Up arrow
                                    if (fsSelected > 0) fsSelected--;
                                    break;
                                case 'B': // Down arrow
                                    if (fsSelected < static_cast<int>(fsOptions.size()) - 1) fsSelected++;
                                    break;
                                case '\n': // Enter key
                                    fsChosen = true;
                                    break;
                            }
                        }

                        std::string fsType = (fsSelected == 0) ? "btrfs" : "ext4";

                        if (!createImageFile(imgSize, outputOrigImgPath) ||
                            !formatFilesystem(fsType, outputOrigImgPath) ||
                            !mountFilesystem(fsType, outputOrigImgPath, MOUNT_POINT) ||
                            !copyFilesWithRsync(SOURCE_DIR, MOUNT_POINT, fsType)) {
                            break;
                            }

                            unmountAndCleanup(MOUNT_POINT);
                        createSquashFS(outputOrigImgPath, outputCompressedImgPath);
                        deleteOriginalImage(outputOrigImgPath);
                        createChecksum(outputCompressedImgPath);
                        printFinalMessage(fsType, outputCompressedImgPath);

                        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                        getch();
                        break;
                    }
                                case 2:
                                    createISO();
                                    std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                                    getch();
                                    break;
                                case 3:
                                    execute_command("df -h");
                                    std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                                    getch();
                                    break;
                                case 4:
                                    return;
                }
                break;
        }
    }
}

int main() {
    // Get current username
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        USERNAME = pw->pw_name;
    } else {
        std::cerr << COLOR_RED << "Failed to get username!" << COLOR_RESET << std::endl;
        return 1;
    }

    // Set BUILD_DIR based on username
    BUILD_DIR = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";

    // Create config directory if it doesn't exist
    std::string configDir = "/home/" + USERNAME + "/.config/cmi";
    execute_command("mkdir -p " + configDir, true);

    // Load existing configuration
    loadConfig();

    // Start time update thread
    std::thread time_thread(update_time_thread);

    // Set terminal to raw mode for arrow key detection
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    showMainMenu();

    // Clean up
    time_thread_running = false;
    time_thread.join();

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return 0;
}
