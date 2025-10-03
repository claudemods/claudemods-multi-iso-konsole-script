#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <array>
#include <memory>
#include <string>
#include <limits>

#define COLOR_GREEN "\033[38;2;0;255;0m"
#define COLOR_CYAN "\033[38;2;0;255;255m"
#define COLOR_RED "\033[38;2;255;0;0m"
#define COLOR_YELLOW "\033[38;2;255;255;0m"
#define COLOR_RESET "\033[0m"

char detected_distro[64] = "";
const char* executable_name = "cmiimg";
bool commands_completed = false;
bool loading_complete = false;
char current_version[64] = "unknown";
char downloaded_version[64] = "unknown";
char installed_version[64] = "unknown";

void silent_command(const char* cmd) {
    char full_cmd[512];
    snprintf(full_cmd, sizeof(full_cmd), "%s >/dev/null 2>&1", cmd);
    system(full_cmd);
}

std::string run_command(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

void* execute_update_thread(void* /*arg*/) {
    while (!loading_complete) usleep(10000);

    // 1. GIT CLONE
    silent_command("cd /home/$USER/ && git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git");

    // 2. CURRENT VERSION
    try {
        std::string version_output = run_command("cat /home/$USER/.config/cmi/version.txt");
        strncpy(current_version, version_output.empty() ? "not installed" : version_output.c_str(), 
               sizeof(current_version) - 1);
    } catch (...) {
        strcpy(current_version, "not installed");
    }

    // 3. DETECT DISTRO (ONLY ARCH AND CACHYOS SUPPORTED)
    try {
        std::string distro_output = run_command("cat /etc/os-release | grep '^ID=' | cut -d'=' -f2 | tr -d '\"'");
        if (distro_output == "arch" || distro_output == "cachyos") {
            strcpy(detected_distro, distro_output.c_str());
        } else {
            std::cout << COLOR_RED << "\nError: Unsupported distribution. Only Arch Linux and CachyOS are supported.\n" << COLOR_RESET;
            exit(EXIT_FAILURE);
        }
    } catch (...) {
        strcpy(detected_distro, "unknown");
        std::cout << COLOR_RED << "\nError: Could not detect distribution.\n" << COLOR_RESET;
        exit(EXIT_FAILURE);
    }

    // 4. DOWNLOADED VERSION
    if (strcmp(detected_distro, "arch") == 0 || strcmp(detected_distro, "cachyos") == 0) {
        try {
            std::string version_output = run_command(
                "cat /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript/version/version.txt");
            strncpy(downloaded_version, version_output.c_str(), sizeof(downloaded_version) - 1);
        } catch (...) {
            strcpy(downloaded_version, "unknown");
        }
    }

    // INSTALLATION PROCESS
    silent_command("rm -rf /home/$USER/.config/cmi");
    silent_command("sudo rm -rf /usr/bin/cmiimg");
    silent_command("mkdir -p /home/$USER/.config/cmi");

    // ARCH AND CACHYOS INSTALLATION
    if (strcmp(detected_distro, "arch") == 0 || strcmp(detected_distro, "cachyos") == 0) {
        silent_command("cp /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript/version/version.txt /home/$USER/.config/cmi/");
        silent_command("wget https://sourceforge.net/projects/claudemods/files/build-image-arch-img.zip/download");
        silent_command("unzip -o download -d /home/$USER/.config/cmi/");
        silent_command("rm -rf download");
        silent_command("cd /home/$USER/.config/cmi/working-hooks-btrfs-ext4 && sudo cp -r * /etc/initcpio");
        silent_command("cd /home/$USER/claudemods-multi-iso-konsole-script/btrfs-and-ext4-installer && qmake6 && make >/dev/null 2>&1");
        silent_command("sudo cp /home/$USER/claudemods-multi-iso-konsole-script/btrfs-and-ext4-installer/cmirsyncinstaller /usr/bin/cmirsyncinstaller");
        
        // Build and install cmiimg
        silent_command("cd /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript && qmake6 && make >/dev/null 2>&1");
        silent_command("sudo cp /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript/cmiimg /usr/bin/cmiimg");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript/cmiimg/calamares /home/$USER/.config/cmi");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript/guide/readme.txt /home/$USER/.config/cmi");
        silent_command("cp -r /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript/changes.txt /home/$USER/.config/cmi");
        silent_command("cp /home/$USER/claudemods-multi-iso-konsole-script/advancedimgscript/installer/patch.sh /home/$USER/.config/cmi >/dev/null 2>&1");
        
        // Apply remote patch
        silent_command("bash -c \"$(curl -fsSL https://raw.githubusercontent.com/claudemods/arch-calamares/refs/heads/main/3.4.0.1/claudemods/patch.sh)\"");
    }

    // Cleanup
    silent_command("rm -rf /home/$USER/claudemods-multi-iso-konsole-script");

    // GET INSTALLED VERSION
    try {
        std::string installed_version_output = run_command("cat /home/$USER/.config/cmi/version.txt");
        strncpy(installed_version, installed_version_output.c_str(), sizeof(installed_version) - 1);
    } catch (...) {
        strcpy(installed_version, "unknown");
    }

    commands_completed = true;
    return nullptr;
}

void show_loading_bar() {
    std::cout << COLOR_GREEN << "Progress: [" << COLOR_RESET;
    for (int i = 0; i < 50; i++) {
        std::cout << COLOR_YELLOW << "=" << COLOR_RESET;
        std::cout.flush();
        usleep(50000);
    }
    std::cout << COLOR_GREEN << "] 100%\n" << COLOR_RESET;
    loading_complete = true;
}

int main() {
    pthread_t thread;
    pthread_create(&thread, nullptr, execute_update_thread, nullptr);

    show_loading_bar();

    while (!commands_completed) usleep(10000);
    pthread_join(thread, nullptr);

    // >>> NEW: BTRFS CONFIG SELECTION PROMPT <<<
    std::cout << COLOR_GREEN << "\nSelect Btrfs configuration for Calamares:\n"
              << COLOR_GREEN << "1) Default Calamares config\n"
              << "2) Claudemods custom config (new mounts + zstd level 22 compression)\n"
              << COLOR_GREEN << "Enter choice (1 or 2): " << COLOR_RESET;

    std::string choice;
    std::getline(std::cin >> std::ws, choice); // std::ws eats leftover newlines

    if (choice == "2") {
        // Copy custom mount.conf to Calamares modules
        silent_command("cp /home/$USER/.config/cmi/btrfs-custom-config/mount.conf /usr/share/calamares/modules/ 2>/dev/null");
        std::cout << COLOR_GREEN << "\nCustom Btrfs config applied.\n" << COLOR_RESET;
    } else if (choice == "1") {
        std::cout << COLOR_GREEN << "\nUsing default Calamares Btrfs config.\n" << COLOR_RESET;
    } else {
        std::cout << COLOR_YELLOW << "\nInvalid choice. Using default Calamares config.\n" << COLOR_RESET;
    }

    // >>> ORIGINAL SUMMARY <<<
    std::cout << COLOR_GREEN << "\nInstallation complete!\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "Executable installed to: /usr/bin/cmiimg\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "Configuration files placed in: /home/$USER/.config/cmi/\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "Detected distro: " << detected_distro << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Current version: " << current_version << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Downloaded version: " << downloaded_version << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "Installed version: " << installed_version << COLOR_RESET << std::endl;

    std::cout << COLOR_CYAN << "\nLaunch now? (y/n): " << COLOR_RESET;
    char response;
    std::cin >> response;

    if (response == 'y' || response == 'Y') {
        system(executable_name);
    }

    return EXIT_SUCCESS;
}
