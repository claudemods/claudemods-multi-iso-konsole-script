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
#include <pwd.h>
#include <ctime>
#include <termios.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <sstream>
#include <iomanip>

// Qt includes for resource system
#include <QFile>
#include <QResource>
#include <QDir>
#include <QCoreApplication>

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
const std::string ORIG_IMG_NAME = "rootfs1.img";
const std::string FINAL_IMG_NAME = "rootfs.img";
std::string MOUNT_POINT = "/mnt/ext4_temp";
const std::string SOURCE_DIR = "/";
const std::string COMPRESSION_LEVEL = "22";
const std::string SQUASHFS_COMPRESSION = "zstd";
const std::vector<std::string> SQUASHFS_COMPRESSION_ARGS = {"-Xcompression-level", "22"};
std::string BUILD_DIR = "/home/$USER/.config/cmi/build-image-arch-img";
std::string USERNAME = "";

// Configuration state
struct ConfigState {
    std::string isoTag;
    std::string isoName;
    std::string outputDir;
    std::string vmlinuzPath;
    std::string cloneDir;
    bool mkinitcpioGenerated = false;
    bool grubEdited = false;
    bool bootTextEdited = false;
    bool calamaresBrandingEdited = false;
    bool calamares1Edited = false;
    bool calamares2Edited = false;

    bool isReadyForISO() const {
        return !isoTag.empty() && !isoName.empty() && !outputDir.empty() &&
        !vmlinuzPath.empty() && mkinitcpioGenerated && grubEdited;
    }

    bool allCheckboxesChecked() const {
        return !isoTag.empty() && !isoName.empty() &&
        !outputDir.empty() && !vmlinuzPath.empty() && !cloneDir.empty() &&
        mkinitcpioGenerated && grubEdited && bootTextEdited &&
        calamaresBrandingEdited && calamares1Edited && calamares2Edited;
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
const std::string COLOR_DISABLED = "\033[90m"; // Added for disabled menu items

// Function to extract embedded zip using Qt resource system
bool extractEmbeddedZip() {
    std::cout << COLOR_CYAN << "Extracting build resources..." << COLOR_RESET << std::endl;

    std::string configDir = "/home/" + USERNAME + "/.config/cmi";
    std::string zipPath = configDir + "/build-image-arch-img.zip";
    std::string extractPath = configDir;

    // Create config directory
    execute_command("mkdir -p " + configDir, true);

    // Use Qt resource system to access embedded zip
    QFile embeddedZip(":/zip/build-image-arch-img.zip");
    if (!embeddedZip.exists()) {
        // Silent failure - no error reporting
        return false;
    }

    // Copy embedded resource to filesystem
    if (!embeddedZip.copy(QString::fromStdString(zipPath))) {
        // Silent failure - no error reporting
        return false;
    }

    // Set proper permissions
    QFile::setPermissions(QString::fromStdString(zipPath), QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

    // Extract zip file
    std::cout << COLOR_CYAN << "Extracting zip file..." << COLOR_RESET << std::endl;
    std::string extractCmd = "unzip -o " + zipPath + " -d " + extractPath + " >/dev/null 2>&1";
    if (system(extractCmd.c_str()) != 0) {
        // Silent failure - no error reporting
        return false;
    }

    // Clean up zip file
    execute_command("rm -f " + zipPath, true);

    std::cout << COLOR_GREEN << "Build resources extracted successfully to: " << extractPath << COLOR_RESET << std::endl;
    return true;
}

// Function to extract Calamares resources
bool extractCalamaresResources() {
    std::cout << COLOR_CYAN << "Extracting Calamares resources..." << COLOR_RESET << std::endl;

    std::string configDir = "/home/" + USERNAME + "/.config/cmi";
    std::string calamaresDir = configDir + "/calamares-files";

    // Create calamares directory
    execute_command("mkdir -p " + calamaresDir, true);

    // Step 1: Extract calamares.zip to cmi folder
    std::cout << COLOR_CYAN << "Step 1: Extracting calamares.zip to cmi folder..." << COLOR_RESET << std::endl;

    std::string calamaresZipPath = configDir + "/calamares.zip";
    QFile embeddedCalamaresZip(":/zip/calamares.zip");
    if (!embeddedCalamaresZip.exists()) {
        // Silent failure - no error reporting
        return false;
    }

    // Copy calamares.zip to filesystem
    if (!embeddedCalamaresZip.copy(QString::fromStdString(calamaresZipPath))) {
        // Silent failure - no error reporting
        return false;
    }

    // Set proper permissions
    QFile::setPermissions(QString::fromStdString(calamaresZipPath), QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

    // Extract calamares.zip
    std::string extractCalamaresCmd = "unzip -o " + calamaresZipPath + " -d " + configDir + " >/dev/null 2>&1";
    if (system(extractCalamaresCmd.c_str()) != 0) {
        // Silent failure - no error reporting
        return false;
    }

    // Clean up calamares.zip
    execute_command("rm -f " + calamaresZipPath, true);

    // Step 2: Extract branding.zip to cmi/calamares
    std::cout << COLOR_CYAN << "Step 2: Extracting branding.zip to cmi/calamares-files..." << COLOR_RESET << std::endl;

    std::string brandingZipPath = configDir + "/branding.zip";
    QFile embeddedBrandingZip(":/zip/branding.zip");
    if (!embeddedBrandingZip.exists()) {
        // Silent failure - no error reporting
        return false;
    }

    // Copy branding.zip to filesystem
    if (!embeddedBrandingZip.copy(QString::fromStdString(brandingZipPath))) {
        // Silent failure - no error reporting
        return false;
    }

    // Set proper permissions
    QFile::setPermissions(QString::fromStdString(brandingZipPath), QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

    // Extract branding.zip to calamares directory
    std::string extractBrandingCmd = "unzip -o " + brandingZipPath + " -d " + calamaresDir + " >/dev/null 2>&1";
    if (system(extractBrandingCmd.c_str()) != 0) {
        // Silent failure - no error reporting
        return false;
    }

    // Clean up branding.zip
    execute_command("rm -f " + brandingZipPath, true);

    // Step 3: Copy extras.zip to cmi/calamares
    std::cout << COLOR_CYAN << "Step 3: Copying extras.zip to cmi/calamares-files..." << COLOR_RESET << std::endl;

    std::string extrasZipPath = calamaresDir + "/extras.zip";
    QFile embeddedExtrasZip(":/zip/extras.zip");
    if (!embeddedExtrasZip.exists()) {
        // Silent failure - no error reporting
        return false;
    }

    // Copy extras.zip to calamares directory
    if (!embeddedExtrasZip.copy(QString::fromStdString(extrasZipPath))) {
        // Silent failure - no error reporting
        return false;
    }

    // Set proper permissions
    QFile::setPermissions(QString::fromStdString(extrasZipPath), QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

    std::cout << COLOR_GREEN << "Calamares resources extracted successfully!" << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "- calamares.zip extracted to: " << configDir << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "- branding.zip extracted to: " << calamaresDir << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "- extras.zip copied to: " << calamaresDir << COLOR_RESET << std::endl;

    return true;
}

// Function to check for updates
bool checkForUpdates() {
    std::cout << COLOR_CYAN << "Checking for updates..." << COLOR_RESET << std::endl;

    // Get username
    struct passwd *pw = getpwuid(getuid());
    std::string username = pw ? pw->pw_name : "";
    if (username.empty()) {
        std::cerr << COLOR_RED << "Failed to get username!" << COLOR_RESET << std::endl;
        return false;
    }

    std::string cloneDir = "/home/" + username + "/claudemods-multi-iso-konsole-script";

    // Try to clone the repository
    std::string cloneCmd = "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git " + cloneDir + " 2>/dev/null";
    int cloneResult = system(cloneCmd.c_str());

    if (cloneResult != 0) {
        std::cout << COLOR_YELLOW << "Failed to check for updates. Continuing with current version." << COLOR_RESET << std::endl;
        return false;
    }

    // Read current version
    std::string currentVersionPath = "/home/" + username + "/.config/cmi/version.txt";
    std::string currentVersion = "";

    std::ifstream currentFile(currentVersionPath);
    if (currentFile.is_open()) {
        std::getline(currentFile, currentVersion);
        currentFile.close();
    }

    // Read new version
    std::string newVersionPath = cloneDir + "/advancedimgscript+/version/version.txt";
    std::string newVersion = "";

    std::ifstream newFile(newVersionPath);
    if (newFile.is_open()) {
        std::getline(newFile, newVersion);
        newFile.close();
    }

    // Clean up cloned directory
    std::string cleanupCmd = "rm -rf " + cloneDir;
    system(cleanupCmd.c_str());

    if (newVersion.empty()) {
        std::cout << COLOR_YELLOW << "Could not retrieve new version information." << COLOR_RESET << std::endl;
        return false;
    }

    if (currentVersion.empty()) {
        std::cout << COLOR_YELLOW << "No current version found. Assuming first run." << COLOR_RESET << std::endl;

        // First run - extract embedded resources
        if (!extractEmbeddedZip()) {
            // Silent failure - no error reporting
        }

        // Also extract Calamares resources on first run
        if (!extractCalamaresResources()) {
            // Silent failure - no error reporting
        }

        // Execute extrainstalls.sh after ALL zips are finished
        std::cout << COLOR_CYAN << "Executing extra installations..." << COLOR_RESET << std::endl;
        execute_command("bash /home/" + USERNAME + "/.config/cmi/extrainstalls.sh", true);

        return false;
    }

    std::cout << COLOR_CYAN << "Current version: " << currentVersion << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "Latest version: " << newVersion << COLOR_RESET << std::endl;

    if (currentVersion != newVersion) {
        std::cout << COLOR_GREEN << "New version available!" << COLOR_RESET << std::endl;
        std::string response = getUserInput("Do you want to update? (yes/no): ");

        if (response == "yes" || response == "y" || response == "Y") {
            std::cout << COLOR_CYAN << "Starting update process..." << COLOR_RESET << std::endl;
            return true;
        }
    } else {
        std::cout << COLOR_GREEN << "You are running the latest version." << COLOR_RESET << std::endl;
    }

    return false;
}

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
    newt.c_lflag &= ~(ICANON);
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
    // Clear screen and move cursor to top
    std::cout << "\033[2J\033[1;1H";

    std::cout << COLOR_RED << R"(
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚══════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
)" << COLOR_RESET << std::endl;
std::cout << COLOR_CYAN << " Advanced C++ Arch Img Iso Script+ Beta v2.03.1 06-10-2025" << COLOR_RESET << std::endl;

{
    std::lock_guard<std::mutex> lock(time_mutex);
    std::cout << COLOR_BLUE << "Current UK Time: " << COLOR_CYAN << current_time_str << COLOR_RESET << std::endl;
}

std::cout << COLOR_GREEN << "Filesystem      Size  Used Avail Use% Mounted on" << COLOR_RESET << std::endl;
execute_command("df -h / | tail -1");
std::cout << std::endl;
}

void printConfigStatus() {
    std::cout << COLOR_CYAN << "Current Configuration:" << COLOR_RESET << std::endl;

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
    printCheckbox(!config.cloneDir.empty());
    std::cout << " Clone Directory: " << (config.cloneDir.empty() ? COLOR_YELLOW + "Not set" : COLOR_CYAN + config.cloneDir) << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(config.mkinitcpioGenerated);
    std::cout << " mkinitcpio Generated" << std::endl;

    std::cout << " ";
    printCheckbox(config.grubEdited);
    std::cout << " GRUB Config Edited" << std::endl;

    std::cout << " ";
    printCheckbox(config.bootTextEdited);
    std::cout << " Boot Text Edited" << std::endl;

    std::cout << " ";
    printCheckbox(config.calamaresBrandingEdited);
    std::cout << " Calamares Branding Edited" << std::endl;

    std::cout << " ";
    printCheckbox(config.calamares1Edited);
    std::cout << " Calamares 1st initcpio.conf Edited" << std::endl;

    std::cout << " ";
    printCheckbox(config.calamares2Edited);
    std::cout << " Calamares 2nd initcpio.conf Edited" << std::endl;
}

std::string getUserInput(const std::string& prompt) {
    std::cout << COLOR_GREEN << prompt << COLOR_RESET;
    std::string input;
    char ch;
    struct termios oldt, newt;

    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // Disable canonical mode and echo
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Track cursor position within input
    size_t cursor_pos = 0;

    while (true) {
        ch = getchar();

        if (ch == '\n') {  // Enter key pressed
            break;
        } else if (ch == 27) {  // Escape sequence (arrow keys)
            if (getchar() == '[') {
                char arrow = getchar();
                if (arrow == 'D') {  // Left arrow
                    if (cursor_pos > 0) {
                        cursor_pos--;
                        // Move cursor left
                        std::cout << "\033[D";
                    }
                } else if (arrow == 'C') {  // Right arrow
                    if (cursor_pos < input.length()) {
                        cursor_pos++;
                        // Move cursor right
                        std::cout << "\033[C";
                    }
                }
            }
        } else if (ch == 127 || ch == 8) {  // Backspace key
            if (cursor_pos > 0) {
                // Remove character at cursor position
                input.erase(cursor_pos - 1, 1);
                cursor_pos--;

                // Move cursor back, clear from cursor to end of line, then reprint remaining characters
                std::cout << "\b\033[K";
                if (cursor_pos < input.length()) {
                    std::cout << input.substr(cursor_pos);
                    // Move cursor back to correct position
                    for (size_t i = 0; i < input.length() - cursor_pos; i++) {
                        std::cout << "\033[D";
                    }
                }
                fflush(stdout);
            }
        } else if (ch >= 32 && ch <= 126) {  // Printable characters
            // Insert character at cursor position
            input.insert(cursor_pos, 1, ch);

            // Clear from cursor to end of line and reprint the rest of the string
            std::cout << "\033[K" << input.substr(cursor_pos);

            // Move cursor back to position after the inserted character
            if (cursor_pos < input.length() - 1) {
                for (size_t i = 0; i < input.length() - cursor_pos - 1; i++) {
                    std::cout << "\033[D";
                }
            }

            cursor_pos++;
            fflush(stdout);
        }
    }

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << std::endl;
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

            std::string destPath = BUILD_DIR + "/boot/vmlinuz-x86_64";
            std::string copyCmd = "sudo cp " + config.vmlinuzPath + " " + destPath;
            execute_command(copyCmd);

            std::cout << COLOR_CYAN << "Selected: " << config.vmlinuzPath << COLOR_RESET << std::endl;
            std::cout << COLOR_CYAN << "Copied to: " << destPath << COLOR_RESET << std::endl;
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

    // Set nano to use cyan color scheme
    std::string nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + grubCfgPath;
    execute_command(nanoCommand);

    config.grubEdited = true;
    saveConfig();
    std::cout << COLOR_GREEN << "GRUB config edited!" << COLOR_RESET << std::endl;
}

void editBootText() {
    if (BUILD_DIR.empty()) {
        std::cerr << COLOR_RED << "Build directory not set!" << COLOR_RESET << std::endl;
        return;
    }

    std::string bootTextPath = BUILD_DIR + "/boot/grub/kernels.cfg";
    std::cout << COLOR_CYAN << "Editing Boot Text: " << bootTextPath << COLOR_RESET << std::endl;

    // Set nano to use cyan color scheme
    std::string nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + bootTextPath;
    execute_command(nanoCommand);

    config.bootTextEdited = true;
    saveConfig();
    std::cout << COLOR_GREEN << "Boot Text edited!" << COLOR_RESET << std::endl;
}

void editCalamaresBranding() {
    std::string calamaresBrandingPath = "/usr/share/calamares/branding/claudemods/branding.desc";
    std::cout << COLOR_CYAN << "Editing Calamares Branding: " << calamaresBrandingPath << COLOR_RESET << std::endl;

    // Set nano to use cyan color scheme
    std::string nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + calamaresBrandingPath;
    execute_command(nanoCommand);

    config.calamaresBrandingEdited = true;
    saveConfig();
    std::cout << COLOR_GREEN << "Calamares Branding edited!" << COLOR_RESET << std::endl;
}

void editCalamares1() {
    std::string calamares1Path = "/etc/calamares/modules/initcpio.conf";
    std::cout << COLOR_CYAN << "Editing Calamares 1st initcpio.conf: " << calamares1Path << COLOR_RESET << std::endl;

    // Set nano to use cyan color scheme
    std::string nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + calamares1Path;
    execute_command(nanoCommand);

    config.calamares1Edited = true;
    saveConfig();
    std::cout << COLOR_GREEN << "Calamares 1st initcpio.conf edited!" << COLOR_RESET << std::endl;
}

void editCalamares2() {
    std::string calamares2Path = "/usr/share/calamares/modules/initcpio.conf";
    std::cout << COLOR_CYAN << "Editing Calamares 2nd initcpio.conf: " << calamares2Path << COLOR_RESET << std::endl;

    // Set nano to use cyan color scheme
    std::string nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + calamares2Path;
    execute_command(nanoCommand);

    config.calamares2Edited = true;
    saveConfig();
    std::cout << COLOR_GREEN << "Calamares 2nd initcpio.conf edited!" << COLOR_RESET << std::endl;
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

    size_t user_pos;
    if ((user_pos = config.outputDir.find("$USER")) != std::string::npos) {
        config.outputDir.replace(user_pos, 5, USERNAME);
    }

    if (config.outputDir.empty()) {
        config.outputDir = defaultDir;
    }

    execute_command("mkdir -p " + config.outputDir, true);

    saveConfig();
}

void setCloneDir() {
    std::string defaultDir = "/home/" + USERNAME;
    std::cout << COLOR_GREEN << "Current clone directory: " << (config.cloneDir.empty() ? COLOR_YELLOW + "Not set" : COLOR_CYAN + config.cloneDir) << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Default directory: " << COLOR_CYAN << defaultDir << COLOR_RESET << std::endl;

    // Get the parent directory from user
    std::string parentDir = getUserInput("Enter parent directory for clone_system_temp folder (e.g., " + defaultDir + " or $USER): ");

    size_t user_pos;
    if ((user_pos = parentDir.find("$USER")) != std::string::npos) {
        parentDir.replace(user_pos, 5, USERNAME);
    }

    if (parentDir.empty()) {
        parentDir = defaultDir;
    }

    // Always use clone_system_temp as the folder name
    config.cloneDir = parentDir + "/clone_system_temp";

    execute_command(" sudo mkdir -p " + config.cloneDir, true);

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
        configFile << "cloneDir=" << config.cloneDir << "\n";
        configFile << "mkinitcpioGenerated=" << (config.mkinitcpioGenerated ? "1" : "0") << "\n";
        configFile << "grubEdited=" << (config.grubEdited ? "1" : "0") << "\n";
        configFile << "bootTextEdited=" << (config.bootTextEdited ? "1" : "0") << "\n";
        configFile << "calamaresBrandingEdited=" << (config.calamaresBrandingEdited ? "1" : "0") << "\n";
        configFile << "calamares1Edited=" << (config.calamares1Edited ? "1" : "0") << "\n";
        configFile << "calamares2Edited=" << (config.calamares2Edited ? "1" : "0") << "\n";
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
                else if (key == "cloneDir") config.cloneDir = value;
                else if (key == "mkinitcpioGenerated") config.mkinitcpioGenerated = (value == "1");
                else if (key == "grubEdited") config.grubEdited = (value == "1");
                else if (key == "bootTextEdited") config.bootTextEdited = (value == "1");
                else if (key == "calamaresBrandingEdited") config.calamaresBrandingEdited = (value == "1");
                else if (key == "calamares1Edited") config.calamares1Edited = (value == "1");
                else if (key == "calamares2Edited") config.calamares2Edited = (value == "1");
            }
        }
        configFile.close();
    }
}

int showMenu(const std::string &title, const std::vector<std::string> &items, int selected) {
    // Clear screen and move cursor to top for every menu display
    std::cout << "\033[2J\033[1;1H";
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

bool validateSizeInput(const std::string& input) {
    try {
        double size = std::stod(input);
        return size > 0.1;  // Minimum 0.1GB
    } catch (...) {
        return false;
    }
}

void showSetupMenu() {
    std::vector<std::string> items = {
        "Set Clone Directory",
        "Set ISO Tag",
        "Set ISO Name",
        "Set Output Directory",
        "Select vmlinuz",
        "Generate mkinitcpio",
        "Edit GRUB Config",
        "Edit Boot Text",
        "Edit Calamares Branding",
        "Edit Calamares 1st initcpio.conf",
        "Edit Calamares 2nd initcpio.conf",
        "Back to Main Menu"
    };

    int selected = 0;
    int key;

    while (true) {
        key = showMenu("ISO Creation Setup Menu:", items, selected);

        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < static_cast<int>(items.size()) - 1) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0: setCloneDir(); break;
                    case 1: setIsoTag(); break;
                    case 2: setIsoName(); break;
                    case 3: setOutputDir(); break;
                    case 4: selectVmlinuz(); break;
                    case 5: generateMkinitcpio(); break;
                    case 6: editGrubCfg(); break;
                    case 7: editBootText(); break;
                    case 8: editCalamaresBranding(); break;
                    case 9: editCalamares1(); break;
                    case 10: editCalamares2(); break;
                    case 11: return;
                }

                if (selected != 11) {
                    std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                    getch();
                }
                break;
        }
    }
}

bool copyFilesWithRsync(const std::string& source, const std::string& destination) {
    std::cout << COLOR_CYAN << "Copying files..." << COLOR_RESET << std::endl;

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
    "--exclude=clone_system_temp "
    "--include=dev "
    "--include=proc "
    "--include=tmp "
    "--include=sys "
    "--include=run "
    "--include=dev "
    "--include=proc "
    "--include=tmp "
    "--include=sys "
    "--include=usr " +
    source + "/ " + destination + "/";

    execute_command(command, true);

    return true;
}

bool createSquashFS(const std::string& inputDir, const std::string& outputFile) {
    std::cout << COLOR_CYAN << "Creating SquashFS image, this may take some time..." << COLOR_RESET << std::endl;

    // Use exact same mksquashfs arguments as in the bash script
    std::string command = "sudo mksquashfs " + inputDir + " " + outputFile +
    " -noappend -comp xz -b 256K -Xbcj x86";

    execute_command(command, true);
    return true;
}

bool createChecksum(const std::string& filename) {
    std::string command = "sha512sum " + filename + " > " + filename + ".sha512";
    execute_command(command, true);
    return true;
}

void printFinalMessage(const std::string& outputFile) {
    std::cout << std::endl;
    std::cout << COLOR_CYAN << "SquashFS image created successfully: " << outputFile << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "Checksum file: " << outputFile + ".sha512" << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "Size: ";
    execute_command("sudo du -h " + outputFile + " | cut -f1", true);
    std::cout << COLOR_RESET;
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
    if (!config.allCheckboxesChecked()) {
        std::cerr << COLOR_RED << "Cannot create ISO - all setup steps must be completed first!" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
        getch();
        return false;
    }

    if (!config.isReadyForISO()) {
        std::cerr << COLOR_RED << "Cannot create ISO - setup is incomplete!" << COLOR_RESET << std::endl;
        return false;
    }

    std::cout << COLOR_CYAN << "\nStarting ISO creation process...\n" << COLOR_RESET;

    std::string expandedOutputDir = expandPath(config.outputDir);

    execute_command("mkdir -p " + expandedOutputDir, true);

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

    // Change ownership of the created ISO to the current user
    std::string isoPath = expandedOutputDir + "/" + config.isoName;
    std::string chownCmd = "sudo chown " + USERNAME + ":" + USERNAME + " \"" + isoPath + "\"";
    execute_command(chownCmd, true);

    std::cout << COLOR_CYAN << "ISO created successfully at " << isoPath << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Ownership changed to current user: " << USERNAME << COLOR_RESET << std::endl;

    return true;
}

void showGuide() {
    std::string readmePath = "/home/" + USERNAME + "/.config/cmi/readme.txt";
    execute_command("mkdir -p /home/" + USERNAME + "/.config/cmi", true);

    std::cout << COLOR_CYAN;
    execute_command("nano " + readmePath, true);
    std::cout << COLOR_RESET;
}

void installISOToUSB() {
    if (config.outputDir.empty()) {
        std::cerr << COLOR_RED << "Output directory not set!" << COLOR_RESET << std::endl;
        return;
    }

    DIR *dir;
    struct dirent *ent;
    std::vector<std::string> isoFiles;

    std::string expandedOutputDir = expandPath(config.outputDir);
    if ((dir = opendir(expandedOutputDir.c_str())) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            std::string filename = ent->d_name;
            if (filename.find(".iso") != std::string::npos) {
                isoFiles.push_back(filename);
            }
        }
        closedir(dir);
    } else {
        std::cerr << COLOR_RED << "Could not open output directory: " << expandedOutputDir << COLOR_RESET << std::endl;
        return;
    }

    if (isoFiles.empty()) {
        std::cerr << COLOR_RED << "No ISO files found in output directory!" << COLOR_RESET << std::endl;
        return;
    }

    std::cout << COLOR_GREEN << "Available ISO files:" << COLOR_RESET << std::endl;
    for (size_t i = 0; i < isoFiles.size(); i++) {
        std::cout << COLOR_GREEN << (i+1) << ") " << isoFiles[i] << COLOR_RESET << std::endl;
    }

    std::string selection = getUserInput("Select ISO file (1-" + std::to_string(isoFiles.size()) + "): ");
    int choice;
    try {
        choice = std::stoi(selection);
        if (choice < 1 || choice > static_cast<int>(isoFiles.size())) {
            std::cerr << COLOR_RED << "Invalid selection!" << COLOR_RESET << std::endl;
            return;
        }
    } catch (...) {
        std::cerr << COLOR_RED << "Invalid input!" << COLOR_RESET << std::endl;
        return;
    }

    std::string selectedISO = expandedOutputDir + "/" + isoFiles[choice-1];

    std::cout << COLOR_CYAN << "\nAvailable drives:" << COLOR_RESET << std::endl;
    execute_command("lsblk -d -o NAME,SIZE,MODEL | grep -v 'loop'", true);

    std::string targetDrive = getUserInput("Enter target drive (e.g., /dev/sda): ");
    if (targetDrive.empty()) {
        std::cerr << COLOR_RED << "No drive specified!" << COLOR_RESET << std::endl;
        return;
    }

    std::cout << COLOR_RED << "\nWARNING: This will overwrite all data on " << targetDrive << "!" << COLOR_RESET << std::endl;
    std::string confirm = getUserInput("Are you sure you want to continue? (y/N): ");
    if (confirm != "y" && confirm != "Y") {
        std::cout << COLOR_CYAN << "Operation cancelled." << COLOR_RESET << std::endl;
        return;
    }

    std::cout << COLOR_CYAN << "\nWriting " << selectedISO << " to " << targetDrive << "..." << COLOR_RESET << std::endl;
    std::string ddCommand = "sudo dd if=" + selectedISO + " of=" + targetDrive + " bs=4M status=progress oflag=sync";
    execute_command(ddCommand, true);

    std::cout << COLOR_GREEN << "\nISO successfully written to USB drive!" << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Press any key to continue..." << COLOR_RESET;
    getch();
}

void runCMIInstaller() {
    execute_command("cmirsyncinstaller", true);
    std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
    getch();
}

void runCalamares() {
    execute_command("sudo calamares", true);
    std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
    getch();
}

void updateScript() {
    std::cout << COLOR_CYAN << "\nUpdating script from GitHub..." << COLOR_RESET << std::endl;
    execute_command("bash -c \"$(curl -fsSL https://raw.githubusercontent.com/claudemods/claudemods-multi-iso-konsole-script/main/advancedimgscript+/installer/patch.sh )\"");
    std::cout << COLOR_GREEN << "\nScript updated successfully!" << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Press any key to continue..." << COLOR_RESET;
    getch();
}

// New function to check if a device is mounted
bool isDeviceMounted(const std::string& device) {
    std::string command = "mount | grep " + device + " > /dev/null 2>&1";
    return system(command.c_str()) == 0;
}

// New function to mount a device
bool mountDevice(const std::string& device, const std::string& mountPoint) {
    std::cout << COLOR_CYAN << "Mounting " << device << " to " << mountPoint << "..." << COLOR_RESET << std::endl;
    execute_command("sudo mkdir -p " + mountPoint, true);
    std::string mountCmd = "sudo mount " + device + " " + mountPoint;
    return system(mountCmd.c_str()) == 0;
}

// New function to clone current system
void cloneCurrentSystem(const std::string& cloneDir) {
    if (!config.allCheckboxesChecked()) {
        std::cerr << COLOR_RED << "Cannot create image - all setup steps must be completed first!" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
        getch();
        return;
    }

    std::cout << COLOR_CYAN << "Cloning current system to " << cloneDir << "..." << COLOR_RESET << std::endl;

    if (!copyFilesWithRsync(SOURCE_DIR, cloneDir)) {
        std::cerr << COLOR_RED << "Failed to clone current system!" << COLOR_RESET << std::endl;
        return;
    }

    std::cout << COLOR_GREEN << "Current system cloned successfully!" << COLOR_RESET << std::endl;
}

// New function to clone another drive
void cloneAnotherDrive(const std::string& cloneDir) {
    if (!config.allCheckboxesChecked()) {
        std::cerr << COLOR_RED << "Cannot create image - all setup steps must be completed first!" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
        getch();
        return;
    }

    std::cout << COLOR_CYAN << "\nAvailable drives:" << COLOR_RESET << std::endl;
    execute_command("lsblk -f -o NAME,FSTYPE,SIZE,MOUNTPOINT | grep -v 'loop'", true);

    std::string drive = getUserInput("Enter drive to clone (e.g., /dev/sda2): ");
    if (drive.empty()) {
        std::cerr << COLOR_RED << "No drive specified!" << COLOR_RESET << std::endl;
        return;
    }

    // Check if drive exists
    std::string checkCmd = "ls " + drive + " > /dev/null 2>&1";
    if (system(checkCmd.c_str()) != 0) {
        std::cerr << COLOR_RED << "Drive " << drive + " does not exist!" << COLOR_RESET << std::endl;
        return;
    }

    std::string tempMountPoint = "/mnt/temp_clone_mount";

    // Mount the drive if not already mounted
    if (!isDeviceMounted(drive)) {
        if (!mountDevice(drive, tempMountPoint)) {
            std::cerr << COLOR_RED << "Failed to mount " << drive << "!" << COLOR_RESET << std::endl;
            return;
        }
    } else {
        // Get existing mount point
        std::string mountCmd = "mount | grep " + drive + " | awk '{print $3}'";
        FILE* fp = popen(mountCmd.c_str(), "r");
        if (fp) {
            char mountPath[256];
            if (fgets(mountPath, sizeof(mountPath), fp)) {
                tempMountPoint = mountPath;
                // Remove newline
                tempMountPoint.erase(tempMountPoint.find_last_not_of("\n") + 1);
            }
            pclose(fp);
        }
    }

    std::cout << COLOR_CYAN << "Cloning " << drive << " from " << tempMountPoint << " to " << cloneDir << "..." << COLOR_RESET << std::endl;

    if (!copyFilesWithRsync(tempMountPoint, cloneDir)) {
        std::cerr << COLOR_RED << "Failed to clone drive " << drive << "!" << COLOR_RESET << std::endl;
    } else {
        std::cout << COLOR_GREEN << "Drive " << drive << " cloned successfully!" << COLOR_RESET << std::endl;
    }

    // Unmount if we mounted it
    if (!isDeviceMounted(drive) || system(("mount | grep " + drive + " | grep " + tempMountPoint).c_str()) == 0) {
        execute_command("sudo umount " + tempMountPoint, true);
        execute_command("sudo rmdir " + tempMountPoint, true);
    }
}

// New function to clone folder or file using rsync
void cloneFolderOrFile(const std::string& cloneDir) {
    if (!config.allCheckboxesChecked()) {
        std::cerr << COLOR_RED << "Cannot create image - all setup steps must be completed first!" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
        getch();
        return;
    }

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

    // Check if source exists
    std::string checkCmd = "sudo test -e " + sourcePath + " > /dev/null 2>&1";
    if (system(checkCmd.c_str()) != 0) {
        std::cerr << COLOR_RED << "Source path does not exist: " << sourcePath << COLOR_RESET << std::endl;
        return;
    }

    // Create userfiles directory in clone_system_temp
    std::string userfilesDir = cloneDir + "/home/userfiles";
    execute_command("sudo mkdir -p " + userfilesDir, true);

    std::cout << COLOR_CYAN << "Cloning " << sourcePath << " to " << userfilesDir << "..." << COLOR_RESET << std::endl;

    // Use rsync to copy the folder or file
    std::string rsyncCmd = "sudo rsync -aHAXSr --numeric-ids --info=progress2 " +
                          sourcePath + " " + userfilesDir + "/";

    execute_command(rsyncCmd, true);

    std::cout << COLOR_GREEN << "Successfully cloned " << sourcePath << " to " << userfilesDir << "!" << COLOR_RESET << std::endl;

    // Show what was copied
    std::string listCmd = "sudo ls -la " + userfilesDir + " | head -20";
    std::cout << COLOR_CYAN << "Contents of userfiles directory:" << COLOR_RESET << std::endl;
    execute_command(listCmd, true);
}

// New function to show clone options menu
void showCloneOptionsMenu() {
    std::vector<std::string> items = {
        "Clone Current System (as it is now)",
        "Clone Another Drive (e.g., /dev/sda2)",
        "Clone Folder or File",
        "Back to Main Menu"
    };

    int selected = 0;
    int key;

    while (true) {
        key = showMenu("Clone Options - Select Source:", items, selected);

        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < static_cast<int>(items.size()) - 1) selected++;
                break;
            case '\n':
                if (config.cloneDir.empty()) {
                    std::cerr << COLOR_RED << "Clone directory not set! Please set it in Setup Scripts menu." << COLOR_RESET << std::endl;
                    std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                    getch();
                    break;
                }

                std::string cloneDir = expandPath(config.cloneDir);
                execute_command("sudo mkdir -p " + cloneDir, true);

                switch (selected) {
                    case 0:
                        cloneCurrentSystem(cloneDir);
                        // Create SquashFS after cloning current system
                        {
                            std::string outputDir = getOutputDirectory();
                            std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;

                            createSquashFS(cloneDir, finalImgPath);

                            // Clean up the clone_system_temp directory after SquashFS creation
                            std::cout << COLOR_CYAN << "Cleaning up temporary clone directory..." << COLOR_RESET << std::endl;
                            std::string cleanupCmd = "sudo rm -rf " + cloneDir;
                            execute_command(cleanupCmd, true);
                            std::cout << COLOR_GREEN << "Temporary directory cleaned up: " << cloneDir << COLOR_RESET << std::endl;

                            createChecksum(finalImgPath);
                            printFinalMessage(finalImgPath);
                        }
                        break;
                    case 1:
                        cloneAnotherDrive(cloneDir);
                        // Create SquashFS after cloning another drive
                        {
                            std::string outputDir = getOutputDirectory();
                            std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;

                            createSquashFS(cloneDir, finalImgPath);

                            // Clean up the clone_system_temp directory after SquashFS creation
                            std::cout << COLOR_CYAN << "Cleaning up temporary clone directory..." << COLOR_RESET << std::endl;
                            std::string cleanupCmd = "sudo rm -rf " + cloneDir;
                            execute_command(cleanupCmd, true);
                            std::cout << COLOR_GREEN << "Temporary directory cleaned up: " << cloneDir << COLOR_RESET << std::endl;

                            createChecksum(finalImgPath);
                            printFinalMessage(finalImgPath);
                        }
                        break;
                    case 2:
                        cloneFolderOrFile(cloneDir);
                        // Note: No SquashFS creation for individual files/folders
                        // User can choose to create ISO with the cloned files
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

// New function for Auto Mode
void runAutoMode() {
    std::cout << COLOR_CYAN << "\n=== Starting Auto Mode ===" << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "This will run all setup steps sequentially and then create an image." << COLOR_RESET << std::endl;
    
    // Step 1: Set Clone Directory
    std::cout << COLOR_CYAN << "\nStep 1: Setting Clone Directory..." << COLOR_RESET << std::endl;
    setCloneDir();
    
    // Step 2: Set ISO Tag
    std::cout << COLOR_CYAN << "\nStep 2: Setting ISO Tag..." << COLOR_RESET << std::endl;
    setIsoTag();
    
    // Step 3: Set ISO Name
    std::cout << COLOR_CYAN << "\nStep 3: Setting ISO Name..." << COLOR_RESET << std::endl;
    setIsoName();
    
    // Step 4: Set Output Directory
    std::cout << COLOR_CYAN << "\nStep 4: Setting Output Directory..." << COLOR_RESET << std::endl;
    setOutputDir();
    
    // Step 5: Select vmlinuz
    std::cout << COLOR_CYAN << "\nStep 5: Selecting vmlinuz..." << COLOR_RESET << std::endl;
    selectVmlinuz();
    
    // Step 6: Generate mkinitcpio
    std::cout << COLOR_CYAN << "\nStep 6: Generating mkinitcpio..." << COLOR_RESET << std::endl;
    generateMkinitcpio();
    
    // Step 7: Edit GRUB Config
    std::cout << COLOR_CYAN << "\nStep 7: Editing GRUB Config..." << COLOR_RESET << std::endl;
    editGrubCfg();
    
    // Step 8: Edit Boot Text
    std::cout << COLOR_CYAN << "\nStep 8: Editing Boot Text..." << COLOR_RESET << std::endl;
    editBootText();
    
    // Step 9: Edit Calamares Branding
    std::cout << COLOR_CYAN << "\nStep 9: Editing Calamares Branding..." << COLOR_RESET << std::endl;
    editCalamaresBranding();
    
    // Step 10: Edit Calamares 1st initcpio.conf
    std::cout << COLOR_CYAN << "\nStep 10: Editing Calamares 1st initcpio.conf..." << COLOR_RESET << std::endl;
    editCalamares1();
    
    // Step 11: Edit Calamares 2nd initcpio.conf
    std::cout << COLOR_CYAN << "\nStep 11: Editing Calamares 2nd initcpio.conf..." << COLOR_RESET << std::endl;
    editCalamares2();
    
    std::cout << COLOR_GREEN << "\nAll setup steps completed successfully!" << COLOR_RESET << std::endl;
    
    // Ask about file/folder copy
    std::string copyChoice = getUserInput("Would you like to copy a file or folder to the image? (yes/no): ");
    if (copyChoice == "yes" || copyChoice == "y" || copyChoice == "Y") {
        std::cout << COLOR_CYAN << "\nCopying file/folder..." << COLOR_RESET << std::endl;
        cloneFolderOrFile(config.cloneDir);
    }
    
    // Ask about cloning
    std::string cloneChoice = getUserInput("Would you like to clone current system or another system? (current/another/no): ");
    if (cloneChoice == "current" || cloneChoice == "c") {
        std::cout << COLOR_CYAN << "\nCloning current system..." << COLOR_RESET << std::endl;
        cloneCurrentSystem(config.cloneDir);
        
        // Create SquashFS after cloning
        std::string outputDir = getOutputDirectory();
        std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;
        
        std::cout << COLOR_CYAN << "\nCreating SquashFS image..." << COLOR_RESET << std::endl;
        createSquashFS(config.cloneDir, finalImgPath);
        
        // Clean up temporary directory
        std::cout << COLOR_CYAN << "Cleaning up temporary clone directory..." << COLOR_RESET << std::endl;
        std::string cleanupCmd = "sudo rm -rf " + config.cloneDir;
        execute_command(cleanupCmd, true);
        
        createChecksum(finalImgPath);
        printFinalMessage(finalImgPath);
        
    } else if (cloneChoice == "another" || cloneChoice == "a") {
        std::cout << COLOR_CYAN << "\nCloning another system..." << COLOR_RESET << std::endl;
        cloneAnotherDrive(config.cloneDir);
        
        // Create SquashFS after cloning
        std::string outputDir = getOutputDirectory();
        std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;
        
        std::cout << COLOR_CYAN << "\nCreating SquashFS image..." << COLOR_RESET << std::endl;
        createSquashFS(config.cloneDir, finalImgPath);
        
        // Clean up temporary directory
        std::cout << COLOR_CYAN << "Cleaning up temporary clone directory..." << COLOR_RESET << std::endl;
        std::string cleanupCmd = "sudo rm -rf " + config.cloneDir;
        execute_command(cleanupCmd, true);
        
        createChecksum(finalImgPath);
        printFinalMessage(finalImgPath);
    }
    
    // Create ISO
    std::string createIsoChoice = getUserInput("Would you like to create an ISO now? (yes/no): ");
    if (createIsoChoice == "yes" || createIsoChoice == "y" || createIsoChoice == "Y") {
        std::cout << COLOR_CYAN << "\nCreating ISO..." << COLOR_RESET << std::endl;
        createISO();
    }
    
    std::cout << COLOR_GREEN << "\nAuto Mode completed!" << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Press any key to continue..." << COLOR_RESET;
    getch();
}

// Modified showMainMenu function to include Auto Mode
void showMainMenu() {
    std::vector<std::string> items = {
        "Guide",
        "Run Auto Mode",
        "Setup Scripts",
        "Create Image",
        "Create ISO",
        "Show Disk Usage",
        "Install ISO to USB",
        "CMI BTRFS/EXT4 Installer",
        "Calamares",
        "Update Script",
        "Exit"
    };

    int selected = 0;
    int key;

    while (true) {
        // Check if all checkboxes are checked for the "Create Image" option
        bool allChecked = config.allCheckboxesChecked();

        key = showMenu("Main Menu:", items, selected);

        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < static_cast<int>(items.size()) - 1) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0:
                        showGuide();
                        break;
                    case 1: // New Auto Mode option
                        runAutoMode();
                        break;
                    case 2:
                        showSetupMenu();
                        break;
                    case 3: // Create Image option
                        if (!allChecked) {
                            std::cerr << COLOR_RED << "Cannot create image - all setup steps must be completed first!" << COLOR_RESET << std::endl;
                            std::cout << COLOR_RED << "Please complete all checkboxes in the Setup Scripts menu." << COLOR_RESET << std::endl;
                            std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                            getch();
                        } else {
                            showCloneOptionsMenu();
                        }
                        break;
                    case 4:
                        createISO();
                        break;
                    case 5:
                        execute_command("df -h");
                        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                        getch();
                        break;
                    case 6:
                        installISOToUSB();
                        break;
                    case 7:
                        runCMIInstaller();
                        break;
                    case 8:
                        runCalamares();
                        break;
                    case 9:
                        updateScript();
                        break;
                    case 10:
                        return;
                }
                break;
        }
    }
}

int main(int argc, char *argv[]) {
    // Initialize Qt application for resource system
    QCoreApplication app(argc, argv);

    // Get username first
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        USERNAME = pw->pw_name;
    } else {
        std::cerr << COLOR_RED << "Failed to get username!" << COLOR_RESET << std::endl;
        return 1;
    }

    // Set build directory
    BUILD_DIR = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";

    // Create config directory
    std::string configDir = "/home/" + USERNAME + "/.config/cmi";
    execute_command("mkdir -p " + configDir, true);

    // Check for updates first
    if (checkForUpdates()) {
        // If update is available and user wants to install, run the update script
        updateScript();
        return 0;
    }

    loadConfig();

    std::thread time_thread(update_time_thread);

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    showMainMenu();

    time_thread_running = false;
    time_thread.join();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return 0;
}
