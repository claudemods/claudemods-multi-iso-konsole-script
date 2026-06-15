#ifndef SETUPSCRIPT_H
#define SETUPSCRIPT_H

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

// Forward declarations
extern std::string BUILD_DIR;
extern std::string USERNAME;

void saveConfig();
void execute_command(const std::string& cmd, bool continueOnError = false);
void printCheckbox(bool checked);
std::string getUserInput(const std::string& prompt);
void clearScreen();
int getch();
int kbhit();
int showMenu(const std::string &title, const std::vector<std::string> &items, int selected);

// ANSI color codes
extern const std::string COLOR_RED;
extern const std::string COLOR_GREEN;
extern const std::string COLOR_BLUE;
extern const std::string COLOR_CYAN;
extern const std::string COLOR_YELLOW;
extern const std::string COLOR_RESET;
extern const std::string COLOR_HIGHLIGHT;
extern const std::string COLOR_NORMAL;
extern const std::string COLOR_DISABLED;

// Configuration state - needs full definition here since setup functions access members
struct ConfigState {
    std::string isoTag;
    std::string isoName;
    std::string outputDir;
    std::string vmlinuzPath;
    std::string cloneDir;
    bool mkinitcpioGenerated = false;
    bool mkinitcpioConfigCopied = false;
    bool grubEdited = false;
    bool bootTextEdited = false;
    bool calamaresBrandingEdited = false;
    bool calamares1Edited = false;
    bool calamares2Edited = false;
    bool filesExtracted = false; // NEW: Track if files have been extracted

    bool isReadyForISO() const {
        return !isoTag.empty() && !isoName.empty() && !outputDir.empty() &&
        !vmlinuzPath.empty() && mkinitcpioGenerated && grubEdited;
    }

    bool allCheckboxesChecked() const {
        return !isoTag.empty() && !isoName.empty() &&
        !outputDir.empty() && !vmlinuzPath.empty() && !cloneDir.empty() &&
        mkinitcpioGenerated && grubEdited && bootTextEdited &&
        calamaresBrandingEdited && calamares1Edited && calamares2Edited &&
        filesExtracted && mkinitcpioConfigCopied; // NEW: Include filesExtracted and mkinitcpioConfigCopied in check
    }
};

extern ConfigState config;

// Function declarations for setup operations
void extractNeededFiles();
void setCloneDir();
void setIsoTag();
void setIsoName();
void setOutputDir();
void selectVmlinuz();
void copyMkinitcpioConfig();
void generateMkinitcpio();
void editGrubCfg();
void editBootText();
void editCalamaresBranding();
void editCalamares1();
void editCalamares2();

// Main setup menu function
void showSetupMenu() {
    std::vector<std::string> items = {
        "Extract Needed Files", // NEW: Added as first option
        "Set Clone Directory",
        "Set ISO Tag",
        "Set ISO Name",
        "Set Output Directory",
        "Select vmlinuz",
        "mkinitcpio Config", // NEW: Added after Select vmlinuz
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
        key = showMenu("Setup Menu:", items, selected);

        if (key == -1) {
            // Time updated, re-render
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
                    case 0: extractNeededFiles(); break; // NEW: Call extract function
                    case 1: setCloneDir(); break;
                    case 2: setIsoTag(); break;
                    case 3: setIsoName(); break;
                    case 4: setOutputDir(); break;
                    case 5: selectVmlinuz(); break;
                    case 6: copyMkinitcpioConfig(); break; // NEW: Call mkinitcpio config copy
                    case 7: generateMkinitcpio(); break;
                    case 8: editGrubCfg(); break;
                    case 9: editBootText(); break;
                    case 10: editCalamaresBranding(); break;
                    case 11: editCalamares1(); break;
                    case 12: editCalamares2(); break;
                    case 13: return;
                }

                if (selected != 13) {
                    std::cout << COLOR_GREEN << "\nPress any key to continue..." << COLOR_RESET;
                    getch();
                }
                break;
        }
    }
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

void copyMkinitcpioConfig() {
    std::string sourceFile = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img/11-dm-initramfs.rules";
    std::string destDir = "/usr/lib/initcpio/udev";
    std::string destFile = destDir + "/11-dm-initramfs.rules";

    std::cout << COLOR_CYAN << "Copying mkinitcpio config..." << COLOR_RESET << std::endl;

    // Check if source file exists
    std::ifstream srcCheck(sourceFile);
    if (!srcCheck.good()) {
        std::cerr << COLOR_RED << "Source file not found: " << sourceFile << COLOR_RESET << std::endl;
        std::cerr << COLOR_RED << "Please extract needed files first!" << COLOR_RESET << std::endl;
        return;
    }
    srcCheck.close();

    // Create destination directory if it doesn't exist
    execute_command("sudo mkdir -p " + destDir, true);

    // Copy the file
    std::string copyCmd = "sudo cp " + sourceFile + " " + destFile;
    execute_command(copyCmd);

    config.mkinitcpioConfigCopied = true;
    saveConfig();
    std::cout << COLOR_GREEN << "mkinitcpio config copied successfully!" << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "Copied to: " << destFile << COLOR_RESET << std::endl;
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

    std::string nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + bootTextPath;
    execute_command(nanoCommand);

    config.bootTextEdited = true;
    saveConfig();
    std::cout << COLOR_GREEN << "Boot Text edited!" << COLOR_RESET << std::endl;
}

void editCalamaresBranding() {
    std::string calamaresBrandingPath = "/usr/share/calamares/branding/claudemods/branding.desc";
    std::cout << COLOR_CYAN << "Editing Calamares Branding: " << calamaresBrandingPath << COLOR_RESET << std::endl;

    std::string nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + calamaresBrandingPath;
    execute_command(nanoCommand);

    config.calamaresBrandingEdited = true;
    saveConfig();
    std::cout << COLOR_GREEN << "Calamares Branding edited!" << COLOR_RESET << std::endl;
}

void editCalamares1() {
    std::string calamares1Path = "/etc/calamares/modules/initcpio.conf";
    std::cout << COLOR_CYAN << "Editing Calamares 1st initcpio.conf: " << calamares1Path << COLOR_RESET << std::endl;

    std::string nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + calamares1Path;
    execute_command(nanoCommand);

    config.calamares1Edited = true;
    saveConfig();
    std::cout << COLOR_GREEN << "Calamares 1st initcpio.conf edited!" << COLOR_RESET << std::endl;
}

void editCalamares2() {
    std::string calamares2Path = "/usr/share/calamares/modules/initcpio.conf";
    std::cout << COLOR_CYAN << "Editing Calamares 2nd initcpio.conf: " << calamares2Path << COLOR_RESET << std::endl;

    std::string nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + calamares2Path;
    execute_command(nanoCommand);

    config.calamares2Edited = true;
    saveConfig();
    std::cout << COLOR_GREEN << "Calamares 2nd initcpio.conf edited!" << COLOR_RESET << std::endl;
}

void setIsoTag() {
    config.isoTag = getUserInput("Enter ISO tag (e.g., default is 2026): ");
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

    std::string parentDir = getUserInput("Enter parent directory for clone_system_temp folder (e.g., " + defaultDir + " or $USER): ");

    size_t user_pos;
    if ((user_pos = parentDir.find("$USER")) != std::string::npos) {
        parentDir.replace(user_pos, 5, USERNAME);
    }

    if (parentDir.empty()) {
        parentDir = defaultDir;
    }

    config.cloneDir = parentDir + "/clone_system_temp";

    execute_command(" sudo mkdir -p " + config.cloneDir, true);

    saveConfig();
}

#endif // SETUPSCRIPT_H
