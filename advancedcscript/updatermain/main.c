#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <termios.h>
#include <sys/stat.h>

#define COLOR_GREEN "\033[38;2;0;255;0m"
#define COLOR_CYAN "\033[38;2;0;255;255m"
#define COLOR_RED "\033[38;2;255;0;0m"
#define COLOR_YELLOW "\033[38;2;255;255;0m"
#define COLOR_RESET "\033[0m"

#define MAX_PATH_LENGTH 512
#define MAX_VERSION_LENGTH 64
#define LOADING_BAR_WIDTH 50

char* detected_distro = NULL;
char* executable_name = NULL;
bool commands_completed = false;
char current_version[MAX_VERSION_LENGTH] = "Not installed";
char new_version[MAX_VERSION_LENGTH] = "Unknown";
char* password = NULL;

char* get_password() {
    struct termios old_term, new_term;
    password = malloc(128);

    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    printf(COLOR_GREEN "Enter sudo password: " COLOR_RESET);
    fgets(password, 128, stdin);
    password[strcspn(password, "\n")] = '\0';

    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    printf("\n");

    // Clone immediately after getting password
    char command[1024];
    char* home = getenv("HOME");
    printf(COLOR_CYAN "\ngetting some info...\n" COLOR_RESET);
    snprintf(command, sizeof(command), "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git \"%s/claudemods-multi-iso-konsole-script\" >/dev/null 2>&1", home);
    system(command);

    return password;
}

const char* get_distro_version_path() {
    static char path[MAX_PATH_LENGTH];
    char* home = getenv("HOME");

    if (strcmp(detected_distro, "arch") == 0) {
        snprintf(path, sizeof(path), "%s/claudemods-multi-iso-konsole-script/advancedcscript/version/arch/version.txt", home);
    }
    else if (strcmp(detected_distro, "ubuntu") == 0) {
        snprintf(path, sizeof(path), "%s/claudemods-multi-iso-konsole-script/advancedcscript/version/ubuntu/version.txt", home);
    }
    else if (strcmp(detected_distro, "debian") == 0) {
        snprintf(path, sizeof(path), "%s/claudemods-multi-iso-konsole-script/advancedcscript/version/debian/version.txt", home);
    }
    return path;
}

char* detect_distro() {
    FILE *fp;
    char buffer[256];

    fp = popen("cat /etc/os-release | grep -E '^ID=' | cut -d'=' -f2", "r");
    if (fp == NULL) {
        fprintf(stderr, COLOR_RED "Failed to detect distribution\n" COLOR_RESET);
        exit(EXIT_FAILURE);
    }

    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        buffer[strcspn(buffer, "\n")] = 0;
        buffer[strcspn(buffer, "\"")] = 0;

        if (strcmp(buffer, "arch") == 0 || strcmp(buffer, "cachyos") == 0) {
            detected_distro = "arch";
            executable_name = "archisocreator";
        } else if (strcmp(buffer, "ubuntu") == 0) {
            detected_distro = "ubuntu";
            executable_name = "ubuntuisocreator";
        } else if (strcmp(buffer, "debian") == 0) {
            detected_distro = "debian";
            executable_name = "debianisocreator";
        }
    }

    pclose(fp);
    return detected_distro;
}

bool read_version_file(const char* path, char* version) {
    char command[MAX_PATH_LENGTH + 32];
    FILE *fp;

    snprintf(command, sizeof(command), "cat \"%s\"", path);
    fp = popen(command, "r");
    if (!fp) {
        return false;
    }

    if (fgets(version, MAX_VERSION_LENGTH, fp) == NULL) {
        pclose(fp);
        return false;
    }

    version[strcspn(version, "\n")] = 0;
    version[strcspn(version, "\r")] = 0;
    pclose(fp);
    return true;
}

void check_versions() {
    char* home = getenv("HOME");
    if (!home) {
        fprintf(stderr, COLOR_RED "Error: Could not determine home directory\n" COLOR_RESET);
        exit(EXIT_FAILURE);
    }

    char current_path[MAX_PATH_LENGTH];
    const char* new_path = get_distro_version_path();

    snprintf(current_path, sizeof(current_path), "%s/.config/cmi/version.txt", home);

    bool has_current = read_version_file(current_path, current_version);
    bool has_new = read_version_file(new_path, new_version);

    if (!has_current) strcpy(current_version, "Not installed");
    if (!has_new) strcpy(new_version, "Unknown");

    printf(COLOR_GREEN "Current version: %s\n" COLOR_RESET, current_version);
    printf(COLOR_GREEN "Available version: %s\n" COLOR_RESET, new_version);
}

void clean_old_installation() {
    char command[1024];
    char* home = getenv("HOME");

    printf(COLOR_CYAN "\nCleaning old installation...\n" COLOR_RESET);

    // Remove installed binary
    snprintf(command, sizeof(command), "echo '%s' | sudo -S rm -f /usr/bin/%s >/dev/null 2>&1",
             password, executable_name);
    system(command);

    // Remove config directory
    snprintf(command, sizeof(command), "echo '%s' | sudo -S rm -rf \"%s/.config/cmi\" >/dev/null 2>&1",
             password, home);
    system(command);
}

void* execute_update_thread(void* arg) {
    const char* password = (const char*)arg;
    char command[1024];
    char* home = getenv("HOME");

    // Clean old installation first
    clean_old_installation();

    // Create fresh config directory
    printf(COLOR_CYAN "Creating config directory...\n" COLOR_RESET);
    snprintf(command, sizeof(command), "mkdir -p \"%s/.config/cmi\" >/dev/null 2>&1", home);
    system(command);

    if (strcmp(detected_distro, "arch") == 0) {
        printf(COLOR_GREEN "\nBuilding Arch version...\n" COLOR_RESET);
        snprintf(command, sizeof(command), "cd \"%s/claudemods-multi-iso-konsole-script/advancedcscript/arch\" && make >/dev/null 2>&1", home);
        system(command);

        // Install new binary
        snprintf(command, sizeof(command), "echo '%s' | sudo -S cp -f \"%s/claudemods-multi-iso-konsole-script/advancedcscript/arch/archisocreator.bin\" /usr/bin/archisocreator >/dev/null 2>&1",
                 password, home);
        system(command);

        // Install version file
        printf(COLOR_GREEN "Installing version file...\n" COLOR_RESET);
        snprintf(command, sizeof(command), "cp \"%s/claudemods-multi-iso-konsole-script/advancedcscript/version/arch/version.txt\" \"%s/.config/cmi/version.txt\"", home, home);
        system(command);

        printf(COLOR_GREEN "Extracting Arch build images...\n" COLOR_RESET);
        snprintf(command, sizeof(command), "echo '%s' | sudo -S unzip -o \"%s/claudemods-multi-iso-konsole-script/advancedcscript/buildimages/build-image-arch.zip\" -d \"%s/.config/cmi/\" >/dev/null 2>&1",
                 password, home, home);
        system(command);
    }
    else if (strcmp(detected_distro, "ubuntu") == 0) {
        printf(COLOR_GREEN "\nBuilding Ubuntu version...\n" COLOR_RESET);
        snprintf(command, sizeof(command), "cd \"%s/claudemods-multi-iso-konsole-script/advancedcscript/ubuntu/noble\" && make >/dev/null 2>&1", home);
        system(command);

        // Install new binary
        snprintf(command, sizeof(command), "echo '%s' | sudo -S cp -f \"%s/claudemods-multi-iso-konsole-script/advancedcscript/ubuntu/noble/ubuntuisocreator.bin\" /usr/bin/ubuntuisocreator >/dev/null 2>&1",
                 password, home);
        system(command);

        // Install version file
        printf(COLOR_GREEN "Installing version file...\n" COLOR_RESET);
        snprintf(command, sizeof(command), "cp \"%s/claudemods-multi-iso-konsole-script/advancedcscript/version/ubuntu/version.txt\" \"%s/.config/cmi/version.txt\"", home, home);
        system(command);

        printf(COLOR_GREEN "Extracting Ubuntu build images...\n" COLOR_RESET);
        snprintf(command, sizeof(command), "echo '%s' | sudo -S unzip -o \"%s/claudemods-multi-iso-konsole-script/advancedcscript/buildimages/build-image-ubuntu.zip\" -d \"%s/.config/cmi/\" >/dev/null 2>&1",
                 password, home, home);
        system(command);
    }
    else if (strcmp(detected_distro, "debian") == 0) {
        printf(COLOR_GREEN "\nBuilding Debian version...\n" COLOR_RESET);
        snprintf(command, sizeof(command), "cd \"%s/claudemods-multi-iso-konsole-script/advancedcscript/debian/bookworm\" && make >/dev/null 2>&1", home);
        system(command);

        // Install new binary
        snprintf(command, sizeof(command), "echo '%s' | sudo -S cp -f \"%s/claudemods-multi-iso-konsole-script/advancedcscript/debian/bookworm/debianisocreator.bin\" /usr/bin/debianisocreator >/dev/null 2>&1",
                 password, home);
        system(command);

        // Install version file
        printf(COLOR_GREEN "Installing version file...\n" COLOR_RESET);
        snprintf(command, sizeof(command), "cp \"%s/claudemods-multi-iso-konsole-script/advancedcscript/version/debian/version.txt\" \"%s/.config/cmi/version.txt\"", home, home);
        system(command);

        printf(COLOR_GREEN "Extracting Debian build images...\n" COLOR_RESET);
        snprintf(command, sizeof(command), "echo '%s' | sudo -S unzip -o \"%s/claudemods-multi-iso-konsole-script/advancedcscript/buildimages/build-image-debian.zip\" -d \"%s/.config/cmi/\" >/dev/null 2>&1",
                 password, home, home);
        system(command);
    }

    // Clean up repository
    snprintf(command, sizeof(command), "rm -rf \"%s/claudemods-multi-iso-konsole-script\" >/dev/null 2>&1", home);
    system(command);

    commands_completed = true;
    return NULL;
}

void show_loading_bar() {
    printf("\n" COLOR_GREEN "ClaudeMods Multi ISO Creator Updater\n" COLOR_RESET);
    printf(COLOR_GREEN "Progress: [" COLOR_RESET);

    for (int i = 0; i < LOADING_BAR_WIDTH; i++) {
        printf(COLOR_GREEN "=" COLOR_RESET);
        fflush(stdout);
        usleep(50000);
    }
    printf(COLOR_GREEN "] 100%%\n\n" COLOR_RESET);
}

void print_installed_version() {
    char path[MAX_PATH_LENGTH];
    char command[MAX_PATH_LENGTH + 32];
    char version[MAX_VERSION_LENGTH];

    snprintf(path, sizeof(path), "%s/.config/cmi/version.txt", getenv("HOME"));
    snprintf(command, sizeof(command), "cat \"%s\"", path);

    FILE *fp = popen(command, "r");
    if (fp) {
        if (fgets(version, sizeof(version), fp)) {
            version[strcspn(version, "\n")] = 0;
            printf(COLOR_GREEN "\nInstalled version: %s\n" COLOR_RESET, version);
        }
        pclose(fp);
    } else {
        printf(COLOR_RED "\nFailed to read installed version\n" COLOR_RESET);
    }
}

bool prompt_launch_script() {
    char response[4];
    printf(COLOR_YELLOW "\nDo you want to open the script now? [y/N]: " COLOR_RESET);
    if (fgets(response, sizeof(response), stdin) == NULL) {
        return false;
    }

    response[strcspn(response, "\n")] = '\0';
    return (strcasecmp(response, "y") == 0 || strcasecmp(response, "yes") == 0);
}

void launch_script() {
    char command[256];
    snprintf(command, sizeof(command), "%s", executable_name);
    printf(COLOR_CYAN "\nLaunching %s...\n" COLOR_RESET, executable_name);
    system(command);
}

int main() {
    // 1. Get password FIRST (which clones repo)
    password = get_password();

    // 2. Detect distribution
    if (detect_distro() == NULL) {
        fprintf(stderr, COLOR_RED "Unsupported distribution!\n" COLOR_RESET);
        free(password);
        return EXIT_FAILURE;
    }

    // 3. Show version info (but proceed with update regardless)
    check_versions();

    // 4. Start update process
    pthread_t thread;
    pthread_create(&thread, NULL, execute_update_thread, (void*)password);

    show_loading_bar();

    while (!commands_completed) {
        usleep(10000);
    }
    pthread_join(thread, NULL);

    // Completion message
    printf(COLOR_GREEN "\nUpdate complete!\n" COLOR_RESET);
    printf(COLOR_GREEN "Successfully updated to version: %s\n" COLOR_RESET, new_version);
    printf(COLOR_GREEN "The executable has been installed to /usr/bin/\n" COLOR_RESET);
    printf(COLOR_GREEN "Configuration files placed in ~/.config/cmi/\n" COLOR_RESET);
    printf(COLOR_GREEN "Start with command: %s\n" COLOR_RESET, executable_name);

    print_installed_version();

    // Ask if user wants to launch the script now
    if (prompt_launch_script()) {
        launch_script();
    }

    free(password);
    return EXIT_SUCCESS;
}
