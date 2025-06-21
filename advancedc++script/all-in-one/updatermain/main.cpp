#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <ctime>
#include <string>
#include <pthread.h>
#include <termios.h>
#include <sys/stat.h>
#include <memory>
#include <fstream>

#define COLOR_GREEN "\033[38;2;0;255;0m"
#define COLOR_CYAN "\033[38;2;0;255;255m"
#define COLOR_RED "\033[38;2;255;0;0m"
#define COLOR_YELLOW "\033[38;2;255;255;0m"
#define COLOR_RESET "\033[0m"

#define MAX_PATH_LENGTH 512
#define MAX_VERSION_LENGTH 64
#define LOADING_BAR_WIDTH 50

std::string detected_distro;
std::string executable_name;
bool commands_completed = false;
char current_version[MAX_VERSION_LENGTH] = "Not installed";
char new_version[MAX_VERSION_LENGTH] = "Unknown";
std::string password;

// Function prototypes
std::string get_password();
std::string get_distro_version_path();
std::string detect_distro();
bool read_version_file(const std::string& path, char* version);
void check_versions();
void clean_old_installation();
void* execute_update_thread(void* arg);
void show_loading_bar();
void print_installed_version();
bool prompt_launch_script();
void launch_script();

std::string get_password() {
    struct termios old_term, new_term;

    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    std::cout << COLOR_GREEN << "Enter sudo password: " << COLOR_RESET;
    std::getline(std::cin, password);

    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    std::cout << std::endl;

    char command[1024];
    const char* home = getenv("HOME");
    std::cout << COLOR_CYAN << "\nGetting some info...\n" << COLOR_RESET;
    snprintf(command, sizeof(command), "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git \"%s/claudemods-multi-iso-konsole-script\" >/dev/null 2>&1", home);
    system(command);

    return password;
}

std::string get_distro_version_path() {
    char path[MAX_PATH_LENGTH];
    const char* home = getenv("HOME");

    snprintf(path, sizeof(path), "%s/claudemods-multi-iso-konsole-script/advancedc++script/version/%s/version.txt",
             home, detected_distro.c_str());
    return path;
}

std::string detect_distro() {
    std::unique_ptr<FILE, int(*)(FILE*)> fp(popen("cat /etc/os-release | grep -E '^ID=' | cut -d'=' -f2", "r"), pclose);
    if (!fp) {
        std::cerr << COLOR_RED << "Failed to detect distribution\n" << COLOR_RESET;
        exit(EXIT_FAILURE);
    }

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp.get())) {
        buffer[strcspn(buffer, "\n")] = 0;
        buffer[strcspn(buffer, "\"")] = 0;

        if (strcmp(buffer, "arch") == 0 || strcmp(buffer, "cachyos") == 0) {
            detected_distro = "arch";
            executable_name = "cmi.bin";
        } else if (strcmp(buffer, "ubuntu") == 0) {
            detected_distro = "ubuntu";
            executable_name = "cmi.bin";
        } else if (strcmp(buffer, "debian") == 0) {
            detected_distro = "debian";
            executable_name = "cmi.bin";
        }
    }

    return detected_distro;
}

bool read_version_file(const std::string& path, char* version) {
    std::ifstream version_file(path);
    if (!version_file.is_open()) {
        return false;
    }

    version_file.getline(version, MAX_VERSION_LENGTH);
    version_file.close();
    return true;
}

void check_versions() {
    const char* home = getenv("HOME");
    if (!home) {
        std::cerr << COLOR_RED << "Error: Could not determine home directory\n" << COLOR_RESET;
        exit(EXIT_FAILURE);
    }

    char current_path[MAX_PATH_LENGTH];
    std::string new_path = get_distro_version_path();

    snprintf(current_path, sizeof(current_path), "%s/.config/cmi/version.txt", home);

    bool has_current = read_version_file(current_path, current_version);
    bool has_new = read_version_file(new_path, new_version);

    if (!has_current) strcpy(current_version, "Not installed");
    if (!has_new) strcpy(new_version, "Unknown");

    std::cout << COLOR_GREEN << "Current version: " << current_version << "\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "Available version: " << new_version << "\n" << COLOR_RESET;
}

void clean_old_installation() {
    char command[1024];
    const char* home = getenv("HOME");

    std::cout << COLOR_CYAN << "\nCleaning old installation...\n" << COLOR_RESET;

    snprintf(command, sizeof(command), "echo '%s' | sudo -S rm -f /usr/bin/%s >/dev/null 2>&1",
             password.c_str(), executable_name.c_str());
    system(command);

    snprintf(command, sizeof(command), "echo '%s' | sudo -S rm -rf \"%s/.config/cmi\" >/dev/null 2>&1",
             password.c_str(), home);
    system(command);
}

void* execute_update_thread(void* arg) {
    const std::string* password = static_cast<const std::string*>(arg);
    char command[1024];
    const char* home = getenv("HOME");

    clean_old_installation();

    std::cout << COLOR_CYAN << "Creating config directory...\n" << COLOR_RESET;
    snprintf(command, sizeof(command), "mkdir -p \"%s/.config/cmi\" >/dev/null 2>&1", home);
    system(command);

    std::cout << COLOR_GREEN << "\nBuilding application...\n" << COLOR_RESET;
    snprintf(command, sizeof(command),
             "cd \"%s/claudemods-multi-iso-konsole-script/advancedc++script\" && "
             "qmake && make >/dev/null 2>&1", home);
    system(command);

    // Install binary
    snprintf(command, sizeof(command),
             "echo '%s' | sudo -S cp -f \"%s/claudemods-multi-iso-konsole-script/advancedc++script/cmi.bin\" /usr/bin/cmi.bin >/dev/null 2>&1",
             password->c_str(), home);
    system(command);

    // Install version file
    std::cout << COLOR_GREEN << "Installing version file...\n" << COLOR_RESET;
    snprintf(command, sizeof(command),
             "cp \"%s/claudemods-multi-iso-konsole-script/advancedc++script/version/%s/version.txt\" \"%s/.config/cmi/version.txt\"",
             home, detected_distro.c_str(), home);
    system(command);

    // Clean up repository
    snprintf(command, sizeof(command),
             "rm -rf \"%s/claudemods-multi-iso-konsole-script\" >/dev/null 2>&1", home);
    system(command);

    commands_completed = true;
    return nullptr;
}

void show_loading_bar() {
    std::cout << "\n" << COLOR_GREEN << "ClaudeMods Multi ISO Creator Updater\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "Progress: [" << COLOR_RESET;

    for (int i = 0; i < LOADING_BAR_WIDTH; i++) {
        std::cout << COLOR_GREEN << "=" << COLOR_RESET;
        std::cout.flush();
        usleep(50000);
    }
    std::cout << COLOR_GREEN << "] 100%\n\n" << COLOR_RESET;
}

void print_installed_version() {
    char path[MAX_PATH_LENGTH];
    char version[MAX_VERSION_LENGTH];

    snprintf(path, sizeof(path), "%s/.config/cmi/version.txt", getenv("HOME"));

    if (read_version_file(path, version)) {
        std::cout << COLOR_GREEN << "\nInstalled version: " << version << "\n" << COLOR_RESET;
    } else {
        std::cout << COLOR_RED << "\nFailed to read installed version\n" << COLOR_RESET;
    }
}

bool prompt_launch_script() {
    std::string response;
    std::cout << COLOR_YELLOW << "\nDo you want to open the script now? [y/N]: " << COLOR_RESET;
    std::getline(std::cin, response);

    return (response == "y" || response == "Y" ||
    response == "yes" || response == "YES");
}

void launch_script() {
    std::cout << COLOR_CYAN << "\nLaunching cmi.bin...\n" << COLOR_RESET;
    system("cmi.bin");
}

int main() {
    password = get_password();

    if (detect_distro().empty()) {
        std::cerr << COLOR_RED << "Unsupported distribution!\n" << COLOR_RESET;
        return EXIT_FAILURE;
    }

    check_versions();

    pthread_t thread;
    pthread_create(&thread, nullptr, execute_update_thread, &password);

    show_loading_bar();

    while (!commands_completed) {
        usleep(10000);
    }
    pthread_join(thread, nullptr);

    std::cout << COLOR_GREEN << "\nUpdate complete!\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "Successfully updated to version: " << new_version << "\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "The executable has been installed to /usr/bin/cmi.bin\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "Configuration files placed in ~/.config/cmi/\n" << COLOR_RESET;
    std::cout << COLOR_GREEN << "Start with command: cmi.bin\n" << COLOR_RESET;

    print_installed_version();

    if (prompt_launch_script()) {
        launch_script();
    }

    return EXIT_SUCCESS;
}

