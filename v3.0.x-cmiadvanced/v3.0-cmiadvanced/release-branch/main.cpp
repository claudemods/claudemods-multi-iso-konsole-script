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
#include <chrono>

// REMOVED Qt includes
// #include <QFile>
// #include <QResource>
// #include <QDir>
// #include <QCoreApplication>

// ADD THIS: Include our resource manager
#include "resources.h"
// ADD THIS: Include our cloner
#include "cloner.h"
// ADD THIS: Include our setup scripts
#include "setupscript.h"

// Forward declarations (default argument only here, not in cloner.h)
void saveConfig();
std::string getUserInput(const std::string& prompt);
void clearScreen();
int getch();
int kbhit();

// Time-related globals
std::atomic<bool> time_thread_running(true);
std::mutex time_mutex;
std::string current_time_str;
bool should_reset = false;
std::atomic<bool> menu_needs_refresh(false);

// Constants
std::string MOUNT_POINT = "/mnt/ext4_temp";
const std::string SOURCE_DIR = "/";
std::string BUILD_DIR = "/home/$USER/.config/cmi/build-image-arch-img";
std::string USERNAME = "";

// Configuration state - definition is now in setupscript.h
// Only declare the instance here
ConfigState config;

// ANSI color codes
const std::string COLOR_RED = "\033[31m";
const std::string COLOR_GREEN = "\033[32m";
const std::string COLOR_BLUE = "\033[34m";
const std::string COLOR_CYAN = "\033[32m";
const std::string COLOR_YELLOW = "\033[33m";
const std::string COLOR_RESET = "\033[0m";
const std::string COLOR_HIGHLIGHT = "\033[32m";
const std::string COLOR_NORMAL = "\033[34m";
const std::string COLOR_DISABLED = "\033[90m";

// CHANGED: Remove Qt resource extraction function and replace with our new one
bool extractEmbeddedZip() {
    return ResourceManager::extractEmbeddedZip(USERNAME);
}

// CHANGED: Remove Qt Calamares extraction function and replace with our new one
bool extractCalamaresResources() {
    return ResourceManager::extractCalamaresResources(USERNAME);
}

// NEW: Function to extract needed files
void extractNeededFiles() {
    std::cout << COLOR_CYAN << "Extracting needed files..." << COLOR_RESET << std::endl;

    bool success = true;

    // Extract embedded zip files
    std::cout << COLOR_CYAN << "Extracting embedded zip resources..." << COLOR_RESET << std::endl;
    if (extractEmbeddedZip()) {
        std::cout << COLOR_GREEN << "Embedded zip resources extracted successfully!" << COLOR_RESET << std::endl;

        if (success) {
            config.filesExtracted = true;
            saveConfig();
            std::cout << COLOR_GREEN << "All needed files extracted successfully!" << COLOR_RESET << std::endl;
        }

        // Execute extrainstalls.sh after successful extraction
        std::cout << COLOR_CYAN << "Running post-extraction setup script..." << COLOR_RESET << std::endl;
        std::string extrainstallsPath = "/home/" + USERNAME + "/.config/cmi/extrainstalls.sh";

        // Run the script directly
        execute_command("bash " + extrainstallsPath, true);
        std::cout << COLOR_GREEN << "Post-extraction setup completed!" << COLOR_RESET << std::endl;
    } else {
        success = false;
    }
}

// Function to check for updates
bool checkForUpdates() {
    std::cout << COLOR_CYAN << "Checking for updates..." << COLOR_RESET << std::endl;

    struct passwd *pw = getpwuid(getuid());
    std::string username = pw ? pw->pw_name : "";
    if (username.empty()) {
        std::cerr << COLOR_RED << "Failed to get username!" << COLOR_RESET << std::endl;
        return false;
    }

    std::string cloneDir = "/home/" + username + "/claudemods-multi-iso-konsole-script";
    std::string cloneCmd = "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git " + cloneDir + " 2>/dev/null";
    int cloneResult = system(cloneCmd.c_str());

    if (cloneResult != 0) {
        std::cout << COLOR_YELLOW << "Failed to check for updates. Continuing with current version." << COLOR_RESET << std::endl;
        return false;
    }

    std::string currentVersionPath = "/home/" + username + "/.config/cmi/version.txt";
    std::string currentVersion = "";

    std::ifstream currentFile(currentVersionPath);
    if (currentFile.is_open()) {
        std::getline(currentFile, currentVersion);
        currentFile.close();
    }

    std::string newVersionPath = cloneDir + "/advancedimgscript++/version/version.txt";
    std::string newVersion = "";

    std::ifstream newFile(newVersionPath);
    if (newFile.is_open()) {
        std::getline(newFile, newVersion);
        newFile.close();
    }

    std::string cleanupCmd = "rm -rf " + cloneDir;
    system(cleanupCmd.c_str());

    if (newVersion.empty()) {
        std::cout << COLOR_YELLOW << "Could not retrieve new version information." << COLOR_RESET << std::endl;
        return false;
    }

    if (currentVersion.empty()) {
        std::cout << COLOR_YELLOW << "No current version found. Assuming first run." << COLOR_RESET << std::endl;

        if (!extractEmbeddedZip()) {
        }

        if (!extractCalamaresResources()) {
        }

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
        menu_needs_refresh = true;
        sleep(1);
    }
}

void clearScreen() {
    std::cout << "\033[H\033[2J\033[3J";
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

// CHANGED: Removed displayAsciiArt() function to eliminate duplication

// CHANGED: Removed printBanner() function to eliminate duplication

void printConfigStatus() {
    std::cout << COLOR_GREEN << "Current Configuration:" << COLOR_RESET << std::endl;

    // NEW: Files extracted checkbox
    std::cout << " ";
    printCheckbox(config.filesExtracted);
    std::cout << (config.filesExtracted ? COLOR_GREEN : COLOR_RED) << " Needed Files" << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(!config.isoTag.empty());
    std::cout << (config.isoTag.empty() ? COLOR_RED : COLOR_GREEN) << " ISO Tag: " << (config.isoTag.empty() ? "Not set" : config.isoTag) << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(!config.isoName.empty());
    std::cout << (config.isoName.empty() ? COLOR_RED : COLOR_GREEN) << " ISO Name: " << (config.isoName.empty() ? "Not set" : config.isoName) << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(!config.outputDir.empty());
    std::cout << (config.outputDir.empty() ? COLOR_RED : COLOR_GREEN) << " Output Directory: " << (config.outputDir.empty() ? "Not set" : config.outputDir) << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(!config.cloneDir.empty());
    std::cout << (config.cloneDir.empty() ? COLOR_RED : COLOR_GREEN) << " Clone Directory: " << (config.cloneDir.empty() ? "Not set" : config.cloneDir) << COLOR_RESET << std::endl;

    // MOVED: vmlinuz below clone directory
    std::cout << " ";
    printCheckbox(!config.vmlinuzPath.empty());
    std::cout << (config.vmlinuzPath.empty() ? COLOR_RED : COLOR_GREEN) << " vmlinuz: " << (config.vmlinuzPath.empty() ? "Not selected" : config.vmlinuzPath) << COLOR_RESET << std::endl;

    // NEW: mkinitcpio config checkbox
    std::cout << " ";
    printCheckbox(config.mkinitcpioConfigCopied);
    std::cout << (config.mkinitcpioConfigCopied ? COLOR_GREEN : COLOR_RED) << " mkinitcpio Config" << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(config.mkinitcpioGenerated);
    std::cout << (config.mkinitcpioGenerated ? COLOR_GREEN : COLOR_RED) << " mkinitcpio" << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(config.grubEdited);
    std::cout << (config.grubEdited ? COLOR_GREEN : COLOR_RED) << " GRUB Config" << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(config.bootTextEdited);
    std::cout << (config.bootTextEdited ? COLOR_GREEN : COLOR_RED) << " Boot Text" << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(config.calamaresBrandingEdited);
    std::cout << (config.calamaresBrandingEdited ? COLOR_GREEN : COLOR_RED) << " Calamares Branding" << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(config.calamares1Edited);
    std::cout << (config.calamares1Edited ? COLOR_GREEN : COLOR_RED) << " Calamares 1st initcpio.conf" << COLOR_RESET << std::endl;

    std::cout << " ";
    printCheckbox(config.calamares2Edited);
    std::cout << (config.calamares2Edited ? COLOR_GREEN : COLOR_RED) << " Calamares 2nd initcpio.conf" << COLOR_RESET << std::endl;
}

std::string getUserInput(const std::string& prompt) {
    std::cout << COLOR_GREEN << prompt << COLOR_RESET;
    std::cout.flush();

    std::string input;
    char ch;
    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Show cursor for text input
    std::cout << "\033[?25h";
    std::cout.flush();

    size_t cursor_pos = 0;

    while (true) {
        ch = getchar();

        if (ch == '\n') {
            break;
        } else if (ch == 27) {
            if (getchar() == '[') {
                char arrow = getchar();
                if (arrow == 'D') {
                    if (cursor_pos > 0) {
                        cursor_pos--;
                        std::cout << "\033[D";
                    }
                } else if (arrow == 'C') {
                    if (cursor_pos < input.length()) {
                        cursor_pos++;
                        std::cout << "\033[C";
                    }
                }
            }
        } else if (ch == 127 || ch == 8) {
            if (cursor_pos > 0) {
                input.erase(cursor_pos - 1, 1);
                cursor_pos--;

                std::cout << "\b\033[K";
                if (cursor_pos < input.length()) {
                    std::cout << input.substr(cursor_pos);
                    for (size_t i = 0; i < input.length() - cursor_pos; i++) {
                        std::cout << "\033[D";
                    }
                }
                fflush(stdout);
            }
        } else if (ch >= 32 && ch <= 126) {
            input.insert(cursor_pos, 1, ch);

            std::cout << "\033[K" << input.substr(cursor_pos);

            if (cursor_pos < input.length() - 1) {
                for (size_t i = 0; i < input.length() - cursor_pos - 1; i++) {
                    std::cout << "\033[D";
                }
            }

            cursor_pos++;
            fflush(stdout);
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << std::endl;
    return input;
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
        configFile << "mkinitcpioConfigCopied=" << (config.mkinitcpioConfigCopied ? "1" : "0") << "\n";
        configFile << "grubEdited=" << (config.grubEdited ? "1" : "0") << "\n";
        configFile << "bootTextEdited=" << (config.bootTextEdited ? "1" : "0") << "\n";
        configFile << "calamaresBrandingEdited=" << (config.calamaresBrandingEdited ? "1" : "0") << "\n";
        configFile << "calamares1Edited=" << (config.calamares1Edited ? "1" : "0") << "\n";
        configFile << "calamares2Edited=" << (config.calamares2Edited ? "1" : "0") << "\n";
        configFile << "filesExtracted=" << (config.filesExtracted ? "1" : "0") << "\n"; // NEW: Save files extracted state
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
                else if (key == "mkinitcpioConfigCopied") config.mkinitcpioConfigCopied = (value == "1");
                else if (key == "grubEdited") config.grubEdited = (value == "1");
                else if (key == "bootTextEdited") config.bootTextEdited = (value == "1");
                else if (key == "calamaresBrandingEdited") config.calamaresBrandingEdited = (value == "1");
                else if (key == "calamares1Edited") config.calamares1Edited = (value == "1");
                else if (key == "calamares2Edited") config.calamares2Edited = (value == "1");
                else if (key == "filesExtracted") config.filesExtracted = (value == "1"); // NEW: Load files extracted state
            }
        }
        configFile.close();
    }
}

// showMenu with echo disabled during navigation, cursor hidden
int showMenu(const std::string &title, const std::vector<std::string> &items, int selected) {
    // Save original terminal settings
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);

    // Disable canonical mode and echo for menu navigation
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Build the entire menu in a string first, then output all at once to prevent flickering
    std::string menuBuffer;

    // Clear screen completely including scrollback buffer
    menuBuffer += "\033[H\033[2J\033[3J";
    menuBuffer += "\033[?25l"; // Hide cursor

    // Banner using the ASCII art - this is the only place the ASCII art is defined now
    std::istringstream ascii_stream(
        "███████████████████████████████████████████████████████████████████████████████████╗\n"
        "░█████╗░██║░░░░░░█████╗░██║░░░██║██████╗░███████╗███╗░░░███╗░█████╗░██████╗░██████╗\n"
        "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n"
        "██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n"
        "██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n"
        "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n"
        "░╚════╝░╚══════╝╚═╝░░╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝╚═╝░░░╚═╝░╚════╝░╚═════╝░╝░\n"
        "███████████████████████████████████████████████████████████████████████████████████╝\n"
    );

    std::string line;
    while (std::getline(ascii_stream, line)) {
        menuBuffer += COLOR_RED + line + COLOR_RESET + "\n";
    }

    menuBuffer += COLOR_RED + "                    cmiadvanced Beta v3.0 17-06-2026" + COLOR_RESET + "\n";
    menuBuffer += COLOR_RED + "Sailing the 7 seas like Penguin's Eggs Remastersys, Refracta, Systemback and father Knoppix!" + COLOR_RESET + "\n";

    // Time line
    {
        std::lock_guard<std::mutex> lock(time_mutex);
        menuBuffer += COLOR_GREEN + "Current UK Time: " + COLOR_GREEN + current_time_str + COLOR_RESET + "\n";
    }

    menuBuffer += COLOR_GREEN + "Filesystem      Size  Used Avail Use% Mounted on" + COLOR_RESET + "\n";

    // Execute df command and capture output
    FILE* dfPipe = popen("df -h / | tail -1", "r");
    if (dfPipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), dfPipe) != nullptr) {
            menuBuffer += COLOR_GREEN + std::string(buffer) + COLOR_RESET;
        }
        pclose(dfPipe);
    }
    menuBuffer += "\n";

    // Config Status
    menuBuffer += COLOR_CYAN + "Current Configuration:" + COLOR_RESET + "\n";

    menuBuffer += " ";
    menuBuffer += (config.filesExtracted ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.filesExtracted ? COLOR_GREEN : COLOR_RED) + " Needed Files" + COLOR_RESET + "\n";

    menuBuffer += " ";
    menuBuffer += (!config.isoTag.empty() ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.isoTag.empty() ? COLOR_RED : COLOR_GREEN) + " ISO Tag: " + (config.isoTag.empty() ? "Not set" : config.isoTag) + COLOR_RESET + "\n";

    menuBuffer += " ";
    menuBuffer += (!config.isoName.empty() ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.isoName.empty() ? COLOR_RED : COLOR_GREEN) + " ISO Name: " + (config.isoName.empty() ? "Not set" : config.isoName) + COLOR_RESET + "\n";

    menuBuffer += " ";
    menuBuffer += (!config.outputDir.empty() ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.outputDir.empty() ? COLOR_RED : COLOR_GREEN) + " Output Directory: " + (config.outputDir.empty() ? "Not set" : config.outputDir) + COLOR_RESET + "\n";

    // MOVED: Clone Directory before vmlinuz
    menuBuffer += " ";
    menuBuffer += (!config.cloneDir.empty() ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.cloneDir.empty() ? COLOR_RED : COLOR_GREEN) + " Clone Directory: " + (config.cloneDir.empty() ? "Not set" : config.cloneDir) + COLOR_RESET + "\n";

    // MOVED: vmlinuz below clone directory
    menuBuffer += " ";
    menuBuffer += (!config.vmlinuzPath.empty() ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.vmlinuzPath.empty() ? COLOR_RED : COLOR_GREEN) + " vmlinuz: " + (config.vmlinuzPath.empty() ? "Not selected" : config.vmlinuzPath) + COLOR_RESET + "\n";

    // NEW: mkinitcpio Config status
    menuBuffer += " ";
    menuBuffer += (config.mkinitcpioConfigCopied ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.mkinitcpioConfigCopied ? COLOR_GREEN : COLOR_RED) + " mkinitcpio Config" + COLOR_RESET + "\n";

    menuBuffer += " ";
    menuBuffer += (config.mkinitcpioGenerated ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.mkinitcpioGenerated ? COLOR_GREEN : COLOR_RED) + " mkinitcpio" + COLOR_RESET + "\n";

    menuBuffer += " ";
    menuBuffer += (config.grubEdited ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.grubEdited ? COLOR_GREEN : COLOR_RED) + " GRUB Config" + COLOR_RESET + "\n";

    menuBuffer += " ";
    menuBuffer += (config.bootTextEdited ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.bootTextEdited ? COLOR_GREEN : COLOR_RED) + " Boot Text" + COLOR_RESET + "\n";

    menuBuffer += " ";
    menuBuffer += (config.calamaresBrandingEdited ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.calamaresBrandingEdited ? COLOR_GREEN : COLOR_RED) + " Calamares Branding" + COLOR_RESET + "\n";

    menuBuffer += " ";
    menuBuffer += (config.calamares1Edited ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.calamares1Edited ? COLOR_GREEN : COLOR_RED) + " Calamares 1st initcpio.conf" + COLOR_RESET + "\n";

    menuBuffer += " ";
    menuBuffer += (config.calamares2Edited ? COLOR_GREEN + "[✓]" + COLOR_RESET : COLOR_RED + "[ ]" + COLOR_RESET);
    menuBuffer += (config.calamares2Edited ? COLOR_GREEN : COLOR_RED) + " Calamares 2nd initcpio.conf" + COLOR_RESET + "\n";

    // Menu title and items
    menuBuffer += COLOR_CYAN + "\n  " + title + COLOR_RESET + "\n";
    menuBuffer += COLOR_CYAN + "  " + std::string(title.length(), '-') + COLOR_RESET + "\n";

    for (size_t i = 0; i < items.size(); i++) {
        if (i == static_cast<size_t>(selected)) {
            menuBuffer += COLOR_HIGHLIGHT + "➤ " + items[i] + COLOR_RESET + "\n";
        } else {
            menuBuffer += COLOR_RED + "  " + items[i] + COLOR_RESET + "\n";
        }
    }

    // Output entire menu at once to prevent flickering
    std::cout << menuBuffer;
    std::cout.flush();

    menu_needs_refresh = false;

    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);

    int key = -1;
    auto start = std::chrono::steady_clock::now();

    while (key == -1) {
        // Check if time updated and we need to refresh
        if (menu_needs_refresh) {
            fcntl(STDIN_FILENO, F_SETFL, oldf);
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return -1; // Signal to caller to re-render
        }

        // Set non-blocking
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

        key = getchar();

        if (key == EOF) {
            key = -1;
            // Restore blocking for a moment to let CPU rest
            fcntl(STDIN_FILENO, F_SETFL, oldf);
            usleep(50000); // 50ms
        } else if (key == 27) {
            // Check for arrow keys
            int key2 = getchar();
            if (key2 == '[') {
                int key3 = getchar();
                if (key3 == 'A') {
                    key = 'A';
                } else if (key3 == 'B') {
                    key = 'B';
                } else {
                    key = -1;
                }
            } else {
                key = -1;
            }
        }
    }

    fcntl(STDIN_FILENO, F_SETFL, oldf);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return key;
}

// Helper functions needed by Cloner
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

bool isDeviceMounted(const std::string& device) {
    std::string command = "mount | grep " + device + " > /dev/null 2>&1";
    return system(command.c_str()) == 0;
}

bool mountDevice(const std::string& device, const std::string& mountPoint) {
    std::cout << COLOR_CYAN << "Mounting " << device << " to " << mountPoint << "..." << COLOR_RESET << std::endl;
    execute_command("sudo mkdir -p " + mountPoint, true);
    std::string mountCmd = "sudo mount " + device + " " + mountPoint;
    return system(mountCmd.c_str()) == 0;
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
    execute_command("bash -c \"$(curl -fsSL https://raw.githubusercontent.com/claudemods/claudemods-multi-iso-konsole-script/main/advancedimgscript++/installer/patch.sh )\"");
    std::cout << COLOR_GREEN << "\nScript updated successfully!" << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Press any key to continue..." << COLOR_RESET;
    getch();
}

void showMainMenu() {
    std::vector<std::string> items = {
        "Guide",
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
        bool allChecked = config.allCheckboxesChecked();

        key = showMenu("cmiadvanced main menu:", items, selected);

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
                switch (selected) {
                    case 0:
                        showGuide();
                        break;
                    case 1:
                        showSetupMenu();
                        break;
                    case 2:
                        if (!allChecked) {
                            std::cerr << COLOR_RED << "Cannot create image - all setup steps must be completed first!" << COLOR_RESET << std::endl;
                            std::cerr << COLOR_RED << "Please complete all checkboxes in the Setup Scripts menu." << COLOR_RESET << std::endl;
                            std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                            getch();
                        } else {
                            Cloner::showCloneOptionsMenu(allChecked, config.cloneDir);
                        }
                        break;
                    case 3:
                        createISO();
                        break;
                    case 4:
                        execute_command("df -h");
                        std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                        getch();
                        break;
                    case 5:
                        installISOToUSB();
                        break;
                    case 6:
                        runCMIInstaller();
                        break;
                    case 7:
                        runCalamares();
                        break;
                    case 8:
                        updateScript();
                        break;
                    case 9:
                        return;
                }
                break;
        }
    }
}

int main(int argc, char *argv[]) {
    // REMOVED: QCoreApplication app(argc, argv);

    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        USERNAME = pw->pw_name;
    } else {
        std::cerr << COLOR_RED << "Failed to get username!" << COLOR_RESET << std::endl;
        return 1;
    }

    BUILD_DIR = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";

    std::string configDir = "/home/" + USERNAME + "/.config/cmi";
    execute_command("mkdir -p " + configDir, true);

    // REMOVED: Update check at startup
    // Now directly load config and start the application

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
