#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <libgen.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <termios.h>
#include <stdbool.h>

#define MAX_PATH 4096
#define MAX_CMD 16384
#define BLUE "\033[34m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"
#define COLOR_CYAN "\033[36m"
#define COLOR_GOLD "\033[38;2;36;255;255m"
#define PASSWORD_MAX 100

// Terminal control
struct termios original_term;

// Distribution detection
typedef enum { ARCH, UBUNTU, DEBIAN, UNKNOWN } Distro;

Distro detect_distro() {
    FILE *os_release = fopen("/etc/os-release", "r");
    if (!os_release) return UNKNOWN;

    char line[256];
    while (fgets(line, sizeof(line), os_release)) {
        if (strstr(line, "ID=") == line) {
            if (strstr(line, "arch")) { fclose(os_release); return ARCH; }
            if (strstr(line, "ubuntu")) { fclose(os_release); return UBUNTU; }
            if (strstr(line, "debian")) { fclose(os_release); return DEBIAN; }
        }
    }
    fclose(os_release);
    return UNKNOWN;
}

// Forward declarations
char* read_clone_dir();
void save_clone_dir(const char *dir_path);
void print_banner(const char* distro_name);

void enable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_term);
}

int get_key() {
    int c = getchar();
    if (c == '\033') {
        getchar(); // Skip [
        return getchar(); // Return actual key code
    }
    return c;
}

void print_blue(const char *text) { printf("%s%s%s\n", BLUE, text, RESET); }

void message_box(const char *title, const char *message) {
    printf("%s%s%s\n", GREEN, title, RESET);
    printf("%s%s%s\n", GREEN, message, RESET);
}

void error_box(const char *title, const char *message) {
    printf("%s%s%s\n", RED, title, RESET);
    printf("%s%s%s\n", RED, message, RESET);
}

void progress_dialog(const char *message) { printf("%s%s%s\n", BLUE, message, RESET); }

void run_command(const char *command) {
    printf("%sRunning command: %s%s\n", BLUE, command, RESET);
    int status = system(command);
    if (status != 0) {
        printf("%sCommand failed with exit code: %d%s\n", RED, WEXITSTATUS(status), RESET);
    }
}

char* prompt(const char *prompt_text) {
    printf("%s%s%s", BLUE, prompt_text, RESET);
    char *input = NULL;
    size_t len = 0;
    getline(&input, &len, stdin);
    // Remove newline character
    if (input[strlen(input)-1] == '\n') {
        input[strlen(input)-1] = '\0';
    }
    return input;
}

void run_sudo_command(const char *command, const char *password) {
    printf("%sRunning command: %s%s\n", BLUE, command, RESET);
    int pipefd[2];
    if (pipe(pipefd)) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        char *full_command = malloc(strlen(command) + 20);
        sprintf(full_command, "sudo -S %s", command);
        execl("/bin/sh", "sh", "-c", full_command, NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    } else {
        close(pipefd[0]);
        write(pipefd[1], password, strlen(password));
        write(pipefd[1], "\n", 1);
        close(pipefd[1]);
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0) {
                printf("%sCommand failed with exit code %d.%s\n", RED, exit_code, RESET);
            }
        }
    }
}

bool dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

char* get_kernel_version() {
    char *version = strdup("unknown");
    FILE* fp = popen("uname -r", "r");
    if (!fp) {
        perror("Failed to get kernel version");
        return version;
    }
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp)) {
        free(version);
        version = strdup(buffer);
        version[strcspn(version, "\n")] = '\0';
    }
    pclose(fp);
    return version;
}

void print_banner(const char* distro_name) {
    printf("%s", RED);
    printf(
        "░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n"
        "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n"
        "██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n"
        "██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n"
        "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n"
        "░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░\n");
    printf("%s", RESET);
    printf("%sClaudemods %s ISO Creator Advanced C Script v1.01 21-06-2025%s\n", RED, distro_name, RESET);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char datetime[50];
    strftime(datetime, sizeof(datetime), "%d/%m/%Y %H:%M:%S", t);
    printf("%sCurrent UK Time: %s%s\n", GREEN, datetime, RESET);

    printf("%sDisk Usage:%s\n", GREEN, RESET);
    const char *cmd = "df -h /";
    FILE *pipe = popen(cmd, "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            printf("%s", buffer);
        }
        pclose(pipe);
    }
}

int show_menu(const char *title, const char **items, int item_count, int selected, const char* distro_name) {
    system("clear");
    print_banner(distro_name);
    printf("%s  %s%s\n", COLOR_CYAN, title, RESET);
    printf("%s  %*s%s\n", COLOR_CYAN, (int)strlen(title), "----------------------------------------", RESET);
    for (int i = 0; i < item_count; i++) {
        if (i == selected) {
            printf("%s➤ %s%s\n", COLOR_GOLD, items[i], RESET);
        } else {
            printf("%s  %s%s\n", COLOR_CYAN, items[i], RESET);
        }
    }
    return get_key();
}

// ============ ARCH FUNCTIONS ============
void install_dependencies_arch() {
    progress_dialog("Installing dependencies...");
    const char *packages =
    "cryptsetup "
    "dmeventd "
    "isolinux "
    "libaio-dev "
    "libcares2 "
    "libdevmapper-event1.02.1 "
    "liblvm2cmd2.03 "
    "live-boot "
    "live-boot-doc "
    "live-boot-initramfs-tools "
    "live-config-systemd "
    "live-tools "
    "lvm2 "
    "pxelinux "
    "syslinux "
    "syslinux-common "
    "thin-provisioning-tools "
    "squashfs-tools "
    "xorriso";
    char *command = malloc(strlen(packages) + 50);
    sprintf(command, "sudo pacman -S --needed --noconfirm %s", packages);
    run_command(command);
    free(command);
    message_box("Success", "Dependencies installed successfully.");
}

void copy_vmlinuz_arch() {
    char *kernel_version = get_kernel_version();
    char *command = malloc(strlen(kernel_version) + 100);
    sprintf(command, "sudo cp /boot/vmlinuz-%s /home/$USER/.config/cmi/build-image-arch/live/", kernel_version);
    run_command(command);
    free(command);
    free(kernel_version);
    message_box("Success", "Vmlinuz copied successfully.");
}

void generate_initrd_arch() {
    progress_dialog("Generating Initramfs (Arch)...");
    run_command("cd /home/$USER/.config/cmi");
    run_command("sudo mkinitcpio -c live.conf -g /home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img");
    message_box("Success", "Initramfs generated successfully.");
}

void edit_grub_cfg_arch() {
    progress_dialog("Opening grub.cfg (arch)...");
    run_command("nano /home/$USER/.config/cmi/build-image-arch/boot/grub/grub.cfg");
    message_box("Success", "grub.cfg opened for editing.");
}

void edit_isolinux_cfg_arch() {
    progress_dialog("Opening isolinux.cfg (arch)...");
    run_command("nano /home/$USER/.config/cmi/build-image-arch/isolinux/isolinux.cfg");
    message_box("Success", "isolinux.cfg opened for editing.");
}

// ============ UBUNTU FUNCTIONS ============
void install_dependencies_ubuntu() {
    progress_dialog("Installing Ubuntu dependencies...");
    const char *packages =
    "cryptsetup "
    "dmeventd "
    "isolinux "
    "libaio-dev "
    "libcares2 "
    "libdevmapper-event1.02.1 "
    "liblvm2cmd2.03 "
    "live-boot "
    "live-boot-doc "
    "live-boot-initramfs-tools "
    "live-config-systemd "
    "live-tools "
    "lvm2 "
    "pxelinux "
    "syslinux "
    "syslinux-common "
    "thin-provisioning-tools "
    "squashfs-tools "
    "xorriso "
    "ubuntu-defaults-builder";

    char *command = malloc(strlen(packages) + 50);
    sprintf(command, "sudo apt install %s", packages);
    run_command(command);
    free(command);
    message_box("Success", "Ubuntu dependencies installed successfully.");
}

void copy_vmlinuz_ubuntu() {
    char *kernel_version = get_kernel_version();
    char *command = malloc(strlen(kernel_version) + 100);
    sprintf(command, "sudo cp /boot/vmlinuz-%s /home/$USER/.config/cmi/build-image-noble/live/", kernel_version);
    run_command(command);
    free(command);
    free(kernel_version);
    message_box("Success", "Vmlinuz copied successfully for Ubuntu.");
}

void generate_initrd_ubuntu() {
    progress_dialog("Generating Initramfs for Ubuntu...");
    run_command("cd /home/$USER/.config/cmi");
    run_command("sudo mkinitramfs -o \"/home/$USER/.config/cmi/build-image-noble/live/initrd.img-$(uname -r)\" \"$(uname -r)\"");
    message_box("Success", "Ubuntu initramfs generated successfully.");
}

void edit_grub_cfg_ubuntu() {
    progress_dialog("Opening Ubuntu grub.cfg...");
    run_command("nano /home/$USER/.config/cmi/build-image-noble/boot/grub/grub.cfg");
    message_box("Success", "Ubuntu grub.cfg opened for editing.");
}

void edit_isolinux_cfg_ubuntu() {
    progress_dialog("Opening Ubuntu isolinux.cfg...");
    run_command("nano /home/$USER/.config/cmi/build-image-noble/isolinux/isolinux.cfg");
    message_box("Success", "Ubuntu isolinux.cfg opened for editing.");
}

void create_squashfs_image_ubuntu() {
    char *clone_dir = read_clone_dir();
    if (!clone_dir || strlen(clone_dir) == 0) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        free(clone_dir);
        return;
    }
    char *command = malloc(strlen(clone_dir) + 200);
    sprintf(command, "sudo mksquashfs %s /home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs "
    "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs", clone_dir);
    printf("Creating Ubuntu SquashFS image from: %s\n", clone_dir);
    run_command(command);
    free(command);
    free(clone_dir);
}

void delete_clone_system_temp_ubuntu() {
    char *clone_dir = read_clone_dir();
    if (!clone_dir || strlen(clone_dir) == 0) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        free(clone_dir);
        return;
    }
    char *command = malloc(strlen(clone_dir) + 20);
    sprintf(command, "sudo rm -rf %s", clone_dir);
    printf("Deleting temporary clone directory: %s\n", clone_dir);
    run_command(command);
    free(command);

    struct stat st;
    if (stat("filesystem.squashfs", &st) == 0) {
        run_command("sudo rm -f /home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs");
        printf("Deleting SquashFS image: filesystem.squashfs\n");
    } else {
        printf("SquashFS image does not exist: filesystem.squashfs\n");
    }
    free(clone_dir);
}

// ============ DEBIAN FUNCTIONS ============
void install_dependencies_debian() {
    progress_dialog("Installing Debian dependencies...");
    const char *packages =
    "cryptsetup "
    "dmeventd "
    "isolinux "
    "libaio-dev "
    "libcares2 "
    "libdevmapper-event1.02.1 "
    "liblvm2cmd2.03 "
    "live-boot "
    "live-boot-doc "
    "live-boot-initramfs-tools "
    "live-config-systemd "
    "live-tools "
    "lvm2 "
    "pxelinux "
    "syslinux "
    "syslinux-common "
    "thin-provisioning-tools "
    "squashfs-tools "
    "xorriso "
    "debian-goodies";

    char *command = malloc(strlen(packages) + 50);
    sprintf(command, "sudo apt install %s", packages);
    run_command(command);
    free(command);
    message_box("Success", "Debian dependencies installed successfully.");
}

void copy_vmlinuz_debian() {
    char *kernel_version = get_kernel_version();
    char *command = malloc(strlen(kernel_version) + 100);
    sprintf(command, "sudo cp /boot/vmlinuz-%s /home/$USER/.config/cmi/build-image-debian/live/", kernel_version);
    run_command(command);
    free(command);
    free(kernel_version);
    message_box("Success", "Vmlinuz copied successfully for Debian.");
}

void generate_initrd_debian() {
    progress_dialog("Generating Initramfs for Debian...");
    run_command("cd /home/$USER/.config/cmi");
    run_command("sudo mkinitramfs -o \"/home/$USER/.config/cmi/build-image-debian/live/initrd.img-$(uname -r)\" \"$(uname -r)\"");
    message_box("Success", "Debian initramfs generated successfully.");
}

void edit_grub_cfg_debian() {
    progress_dialog("Opening Debian grub.cfg...");
    run_command("nano /home/$USER/.config/cmi/build-image-debian/boot/grub/grub.cfg");
    message_box("Success", "Debian grub.cfg opened for editing.");
}

void edit_isolinux_cfg_debian() {
    progress_dialog("Opening Debian isolinux.cfg...");
    run_command("nano /home/$USER/.config/cmi/build-image-debian/isolinux/isolinux.cfg");
    message_box("Success", "Debian isolinux.cfg opened for editing.");
}

void create_squashfs_image_debian() {
    char *clone_dir = read_clone_dir();
    if (!clone_dir || strlen(clone_dir) == 0) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        free(clone_dir);
        return;
    }
    char *command = malloc(strlen(clone_dir) + 200);
    sprintf(command, "sudo mksquashfs %s /home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs "
    "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs", clone_dir);
    printf("Creating Debian SquashFS image from: %s\n", clone_dir);
    run_command(command);
    free(command);
    free(clone_dir);
}

void delete_clone_system_temp_debian() {
    char *clone_dir = read_clone_dir();
    if (!clone_dir || strlen(clone_dir) == 0) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        free(clone_dir);
        return;
    }
    char *command = malloc(strlen(clone_dir) + 20);
    sprintf(command, "sudo rm -rf %s", clone_dir);
    printf("Deleting temporary clone directory: %s\n", clone_dir);
    run_command(command);
    free(command);

    struct stat st;
    if (stat("filesystem.squashfs", &st) == 0) {
        run_command("sudo rm -f /home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs");
        printf("Deleting SquashFS image: filesystem.squashfs\n");
    } else {
        printf("SquashFS image does not exist: filesystem.squashfs\n");
    }
    free(clone_dir);
}

// ============ COMMON FUNCTIONS ============
void save_clone_dir(const char *dir_path) {
    char *user = getenv("USER");
    char *config_dir = malloc(strlen(user) + 50);
    sprintf(config_dir, "/home/%s/.config/cmi", user);

    if (!dir_exists(config_dir)) {
        char *mkdir_cmd = malloc(strlen(config_dir) + 20);
        sprintf(mkdir_cmd, "mkdir -p %s", config_dir);
        run_command(mkdir_cmd);
        free(mkdir_cmd);
    }

    char *file_path = malloc(strlen(config_dir) + 20);
    sprintf(file_path, "%s/clonedir.txt", config_dir);
    FILE *f = fopen(file_path, "w");
    if (!f) {
        perror("Failed to open clonedir.txt");
        free(config_dir);
        free(file_path);
        return;
    }
    fprintf(f, "%s", dir_path);
    fclose(f);
    free(config_dir);
    free(file_path);
    message_box("Success", "Clone directory path saved successfully.");
}

char* read_clone_dir() {
    char *user = getenv("USER");
    char *file_path = malloc(strlen(user) + 50);
    sprintf(file_path, "/home/%s/.config/cmi/clonedir.txt", user);

    FILE *f = fopen(file_path, "r");
    if (!f) {
        free(file_path);
        return strdup("");
    }

    char *dir_path = NULL;
    size_t len = 0;
    ssize_t read = getline(&dir_path, &len, f);
    fclose(f);
    free(file_path);

    if (read == -1) {
        free(dir_path);
        return strdup("");
    }

    // Remove newline if present
    if (dir_path[strlen(dir_path)-1] == '\n') {
        dir_path[strlen(dir_path)-1] = '\0';
    }

    return dir_path;
}

void clone_system(const char *clone_dir) {
    if (dir_exists(clone_dir)) {
        printf("Skipping removal of existing clone directory: %s\n", clone_dir);
    } else {
        mkdir(clone_dir, 0755);
    }
    char *command = malloc(strlen(clone_dir) + 500);
    sprintf(command, "sudo rsync -aHAxSr --numeric-ids --info=progress2 "
    "--include=dev --include=usr --include=proc --include=tmp --include=sys "
    "--include=run --include=media "
    "--exclude=dev/* --exclude=proc/* --exclude=tmp/* --exclude=sys/* "
    "--exclude=run/* --exclude=media/* --exclude=%s "
    "/ %s", clone_dir, clone_dir);
    printf("Cloning system directory to: %s\n", clone_dir);
    run_command(command);
    free(command);
}

void create_squashfs_image() {
    char *clone_dir = read_clone_dir();
    if (!clone_dir || strlen(clone_dir) == 0) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        free(clone_dir);
        return;
    }
    char *command = malloc(strlen(clone_dir) + 200);
    sprintf(command, "sudo mksquashfs %s /home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs "
    "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs", clone_dir);
    printf("Creating SquashFS image from: %s\n", clone_dir);
    run_command(command);
    free(command);
    free(clone_dir);
}

void delete_clone_system_temp() {
    char *clone_dir = read_clone_dir();
    if (!clone_dir || strlen(clone_dir) == 0) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        free(clone_dir);
        return;
    }
    char *command = malloc(strlen(clone_dir) + 20);
    sprintf(command, "sudo rm -rf %s", clone_dir);
    printf("Deleting temporary clone directory: %s\n", clone_dir);
    run_command(command);
    free(command);

    struct stat st;
    if (stat("filesystem.squashfs", &st) == 0) {
        run_command("sudo rm -f /home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs");
        printf("Deleting SquashFS image: filesystem.squashfs\n");
    } else {
        printf("SquashFS image does not exist: filesystem.squashfs\n");
    }
    free(clone_dir);
}

void set_clone_directory() {
    char *dir_path = prompt("Enter full path for clone_system_temp directory: ");
    if (!dir_path || strlen(dir_path) == 0) {
        error_box("Error", "Directory path cannot be empty");
        free(dir_path);
        return;
    }
    save_clone_dir(dir_path);
    free(dir_path);
}

void install_one_time_updater() {
    progress_dialog("Installing one-time updater...");
    const char *commands[] = {
        "cd /home/$USER",
        "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git  >/dev/null 2>&1",
        "mkdir -p /home/$USER/.config/cmi >/dev/null 2>&1",
        "cp /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/updatermain/advancedcscriptupdater /home/$USER/.config/cmi >/dev/null 2>&1",
        "cp /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/installer/patch.sh /home/$USER/.config/cmi >/dev/null 2>&1",
        "chmod +x /home/$USER/.config/cmi/patch.sh >/dev/null 2>&1",
        "chmod +x /home/$USER/.config/cmi/advancedcscriptupdater >/dev/null 2>&1",
        "rm -rf /home/$USER/claudemods-multi-iso-konsole-script >/dev/null 2>&1",
        NULL
    };

    for (int i = 0; commands[i] != NULL; i++) {
        run_command(commands[i]);
    }
    message_box("Success", "One-time updater installed successfully in /home/$USER/.config/cmi");
}

void squashfs_menu(const char* distro_name) {
    const char *items[] = {
        "Max compression (xz)",
        "Create SquashFS from clone directory",
        "Delete clone directory and SquashFS image",
        "Back to Main Menu",
        NULL
    };
    int item_count = 4;
    int selected = 0;
    int key;
    while (1) {
        key = show_menu("SquashFS Creator", items, item_count, selected, distro_name);
        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < item_count - 1) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0:
                    {
                        char *clone_dir = read_clone_dir();
                        if (!clone_dir || strlen(clone_dir) == 0) {
                            error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
                            free(clone_dir);
                            break;
                        }
                        if (!dir_exists(clone_dir)) {
                            clone_system(clone_dir);
                        }
                        if (strcmp(distro_name, "ubuntu") == 0) {
                            create_squashfs_image_ubuntu();
                        } else if (strcmp(distro_name, "debian") == 0) {
                            create_squashfs_image_debian();
                        } else {
                            create_squashfs_image();
                        }
                        free(clone_dir);
                    }
                    break;
                    case 1:
                        if (strcmp(distro_name, "ubuntu") == 0) {
                            create_squashfs_image_ubuntu();
                        } else if (strcmp(distro_name, "debian") == 0) {
                            create_squashfs_image_debian();
                        } else {
                            create_squashfs_image();
                        }
                        break;
                    case 2:
                        if (strcmp(distro_name, "ubuntu") == 0) {
                            delete_clone_system_temp_ubuntu();
                        } else if (strcmp(distro_name, "debian") == 0) {
                            delete_clone_system_temp_debian();
                        } else {
                            delete_clone_system_temp();
                        }
                        break;
                    case 3:
                        return;
                }
                printf("\nPress Enter to continue...");
                while (getchar() != '\n');
                break;
        }
    }
}

void create_iso(const char* distro_name) {
    char *iso_name = prompt("What do you want to name your .iso? ");
    if (!iso_name || strlen(iso_name) == 0) {
        error_box("Input Error", "ISO name cannot be empty.");
        free(iso_name);
        return;
    }

    char *output_dir = prompt("Enter the output directory path (or press Enter for current directory): ");
    if (!output_dir || strlen(output_dir) == 0) {
        free(output_dir);
        output_dir = strdup(".");
    }

    char application_dir_path[MAX_PATH];
    if (getcwd(application_dir_path, sizeof(application_dir_path)) == NULL) {
        perror("getcwd");
        free(iso_name);
        free(output_dir);
        return;
    }

    char *build_image_dir;
    if (strcmp(distro_name, "ubuntu") == 0) {
        build_image_dir = malloc(strlen(application_dir_path) + 20);
        sprintf(build_image_dir, "%s/build-image-noble", application_dir_path);
    } else if (strcmp(distro_name, "debian") == 0) {
        build_image_dir = malloc(strlen(application_dir_path) + 20);
        sprintf(build_image_dir, "%s/build-image-debian", application_dir_path);
    } else {
        build_image_dir = malloc(strlen(application_dir_path) + 20);
        sprintf(build_image_dir, "%s/build-image-arch", application_dir_path);
    }

    if (strlen(build_image_dir) >= MAX_PATH) {
        error_box("Error", "Path too long for build directory");
        free(iso_name);
        free(output_dir);
        free(build_image_dir);
        return;
    }

    struct stat st;
    if (stat(output_dir, &st) == -1) {
        if (mkdir(output_dir, 0755) == -1) {
            perror("mkdir");
            free(iso_name);
            free(output_dir);
            free(build_image_dir);
            return;
        }
    }

    progress_dialog("Creating ISO...");
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H%M", t);

    char *iso_file_name = malloc(strlen(output_dir) + strlen(iso_name) + strlen(timestamp) + 20);
    sprintf(iso_file_name, "%s/%s_amd64_%s.iso", output_dir, iso_name, timestamp);

    if (strlen(iso_file_name) >= MAX_PATH) {
        error_box("Error", "Path too long for ISO filename");
        free(iso_name);
        free(output_dir);
        free(build_image_dir);
        free(iso_file_name);
        return;
    }

    char *xorriso_command;
    if (strcmp(distro_name, "ubuntu") == 0 || strcmp(distro_name, "debian") == 0) {
        xorriso_command = malloc(strlen(iso_file_name) + strlen(build_image_dir) + 300);
        sprintf(xorriso_command, "sudo xorriso -as mkisofs -o \"%s\" -V 2025 -iso-level 3 "
        "-isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin "
        "-c isolinux/boot.cat -b isolinux/isolinux.bin "
        "-no-emul-boot -boot-load-size 4 -boot-info-table "
        "-eltorito-alt-boot -e boot/grub/efi.img "
        "-no-emul-boot -isohybrid-gpt-basdat \"%s\"", iso_file_name, build_image_dir);
    } else {
        xorriso_command = malloc(strlen(iso_file_name) + strlen(build_image_dir) + 300);
        sprintf(xorriso_command, "sudo xorriso -as mkisofs -o \"%s\" -V 2025 -iso-level 3 "
        "-isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin -c isolinux/boot.cat "
        "-b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table "
        "-eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat \"%s\"",
        iso_file_name, build_image_dir);
    }

    if (strlen(xorriso_command) >= MAX_CMD) {
        error_box("Error", "Command too long for buffer");
        free(iso_name);
        free(output_dir);
        free(build_image_dir);
        free(iso_file_name);
        free(xorriso_command);
        return;
    }

    char *sudo_password = prompt("Enter your sudo password: ");
    if (!sudo_password || strlen(sudo_password) == 0) {
        error_box("Input Error", "Sudo password cannot be empty.");
        free(iso_name);
        free(output_dir);
        free(build_image_dir);
        free(iso_file_name);
        free(xorriso_command);
        free(sudo_password);
        return;
    }

    run_sudo_command(xorriso_command, sudo_password);
    message_box("Success", "ISO creation completed.");

    char *choice = prompt("Press 'm' to go back to main menu or Enter to exit: ");
    if (choice && (choice[0] == 'm' || choice[0] == 'M')) {
        run_command("ruby /opt/claudemods-iso-konsole-script/demo.rb");
    }

    free(iso_name);
    free(output_dir);
    free(build_image_dir);
    free(iso_file_name);
    free(xorriso_command);
    free(sudo_password);
    free(choice);
}

void run_iso_in_qemu() {
    const char *qemu_script = "/opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb";
    char *command = malloc(strlen(qemu_script) + 20);
    sprintf(command, "ruby %s", qemu_script);
    run_command(command);
    free(command);
}

void iso_creator_menu(const char* distro_name) {
    const char *items[] = {
        "Create ISO",
        "Run ISO in QEMU",
        "Back to Main Menu",
        NULL
    };
    int item_count = 3;
    int selected = 0;
    int key;
    while (1) {
        key = show_menu("ISO Creator Menu", items, item_count, selected, distro_name);
        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < item_count - 1) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0:
                        create_iso(distro_name);
                        break;
                    case 1:
                        run_iso_in_qemu();
                        break;
                    case 2:
                        return;
                }
                break;
        }
    }
}

void create_command_files() {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len == -1) {
        perror("readlink");
        return;
    }
    exe_path[len] = '\0';

    char *sudo_password = prompt("Enter sudo password to create command files: ");
    if (!sudo_password || strlen(sudo_password) == 0) {
        error_box("Error", "Sudo password cannot be empty");
        free(sudo_password);
        return;
    }

    // gen-init
    char *command = malloc(strlen(exe_path) + 200);
    sprintf(command, "sudo bash -c 'cat > /usr/bin/gen-init << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: gen-init\"\n"
    "  echo \"Generate initcpio configuration\"\n"
    "  exit 0\n"
    "fi\n"
    "exec %s 5\n"
    "EOF\n"
    "chmod 755 /usr/bin/gen-init'", exe_path);
    run_sudo_command(command, sudo_password);

    // edit-isocfg
    sprintf(command, "sudo bash -c 'cat > /usr/bin/edit-isocfg << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-isocfg\"\n"
    "  echo \"Edit isolinux.cfg file\"\n"
    "  exit 0\n"
    "fi\n"
    "exec %s 6\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-isocfg'", exe_path);
    run_sudo_command(command, sudo_password);

    // edit-grubcfg
    sprintf(command, "sudo bash -c 'cat > /usr/bin/edit-grubcfg << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-grubcfg\"\n"
    "  echo \"Edit grub.cfg file\"\n"
    "  exit 0\n"
    "fi\n"
    "exec %s 7\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-grubcfg'", exe_path);
    run_sudo_command(command, sudo_password);

    // setup-script
    sprintf(command, "sudo bash -c 'cat > /usr/bin/setup-script << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: setup-script\"\n"
    "  echo \"Open setup script menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec %s 8\n"
    "EOF\n"
    "chmod 755 /usr/bin/setup-script'", exe_path);
    run_sudo_command(command, sudo_password);

    // make-iso
    sprintf(command, "sudo bash -c 'cat > /usr/bin/make-iso << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: make-iso\"\n"
    "  echo \"Launches the ISO creation menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec %s 3\n"
    "EOF\n"
    "chmod 755 /usr/bin/make-iso'", exe_path);
    run_sudo_command(command, sudo_password);

    // make-squashfs
    sprintf(command, "sudo bash -c 'cat > /usr/bin/make-squashfs << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: make-squashfs\"\n"
    "  echo \"Launches the SquashFS creation menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec %s 4\n"
    "EOF\n"
    "chmod 755 /usr/bin/make-squashfs'", exe_path);
    run_sudo_command(command, sudo_password);

    free(command);
    free(sudo_password);
    printf("%sActivated! You can now use all commands in your terminal.%s\n", GREEN, RESET);
}

void remove_command_files() {
    run_command("sudo rm -f /usr/bin/gen-init");
    run_command("sudo rm -f /usr/bin/edit-isocfg");
    run_command("sudo rm -f /usr/bin/edit-grubcfg");
    run_command("sudo rm -f /usr/bin/setup-script");
    run_command("sudo rm -f /usr/bin/make-iso");
    run_command("sudo rm -f /usr/bin/make-squashfs");
    printf("%sCommands deactivated and removed from system.%s\n", GREEN, RESET);
}

void command_installer_menu(const char* distro_name) {
    const char *items[] = {
        "Activate terminal commands",
        "Deactivate terminal commands",
        "Back to Main Menu",
        NULL
    };
    int item_count = 3;
    int selected = 0;
    int key;
    while (1) {
        key = show_menu("Command Installer Menu", items, item_count, selected, distro_name);
        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < item_count - 1) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0:
                        create_command_files();
                        break;
                    case 1:
                        remove_command_files();
                        break;
                    case 2:
                        return;
                }
                printf("\nPress Enter to continue...");
                while (getchar() != '\n');
                break;
        }
    }
}

void setup_script_menu() {
    Distro distro = detect_distro();
    char *distro_name;

    switch(distro) {
        case ARCH:
            distro_name = strdup("Arch");
            {
                const char *items[] = {
                    "Generate initcpio configuration (arch)",
                    "Edit isolinux.cfg (arch)",
                    "Edit grub.cfg (arch)",
                    "Set clone directory path",
                    "Install One Time Updater",
                    "Back to Main Menu",
                    NULL
                };
                int item_count = 6;
                int selected = 0;
                int key;
                while (1) {
                    key = show_menu("Setup Script Menu", items, item_count, selected, distro_name);
                    switch (key) {
                        case 'A':
                            if (selected > 0) selected--;
                            break;
                        case 'B':
                            if (selected < item_count - 1) selected++;
                            break;
                        case '\n':
                            switch (selected) {
                                case 0: generate_initrd_arch(); break;
                                case 1: edit_isolinux_cfg_arch(); break;
                                case 2: edit_grub_cfg_arch(); break;
                                case 3: set_clone_directory(); break;
                                case 4: install_one_time_updater(); break;
                                case 5: free(distro_name); return;
                            }
                            printf("\nPress Enter to continue...");
                            while (getchar() != '\n');
                            break;
                    }
                }
            }
            break;

                                case UBUNTU:
                                    distro_name = strdup("Ubuntu");
                                    {
                                        const char *items[] = {
                                            "Generate initcpio configuration (ubuntu)",
                                            "Edit isolinux.cfg (ubuntu)",
                                            "Edit grub.cfg (ubuntu)",
                                            "Set clone directory path",
                                            "Install One Time Updater",
                                            "Back to Main Menu",
                                            NULL
                                        };
                                        int item_count = 6;
                                        int selected = 0;
                                        int key;
                                        while (1) {
                                            key = show_menu("Setup Script Menu", items, item_count, selected, distro_name);
                                            switch (key) {
                                                case 'A':
                                                    if (selected > 0) selected--;
                                                    break;
                                                case 'B':
                                                    if (selected < item_count - 1) selected++;
                                                    break;
                                                case '\n':
                                                    switch (selected) {
                                                        case 0: generate_initrd_ubuntu(); break;
                                                        case 1: edit_isolinux_cfg_ubuntu(); break;
                                                        case 2: edit_grub_cfg_ubuntu(); break;
                                                        case 3: set_clone_directory(); break;
                                                        case 4: install_one_time_updater(); break;
                                                        case 5: free(distro_name); return;
                                                    }
                                                    printf("\nPress Enter to continue...");
                                                    while (getchar() != '\n');
                                                    break;
                                            }
                                        }
                                    }
                                    break;

                                                        case DEBIAN:
                                                            distro_name = strdup("Debian");
                                                            {
                                                                const char *items[] = {
                                                                    "Generate initcpio configuration (debian)",
                                                                    "Edit isolinux.cfg (debian)",
                                                                    "Edit grub.cfg (debian)",
                                                                    "Set clone directory path",
                                                                    "Install One Time Updater",
                                                                    "Back to Main Menu",
                                                                    NULL
                                                                };
                                                                int item_count = 6;
                                                                int selected = 0;
                                                                int key;
                                                                while (1) {
                                                                    key = show_menu("Setup Script Menu", items, item_count, selected, distro_name);
                                                                    switch (key) {
                                                                        case 'A':
                                                                            if (selected > 0) selected--;
                                                                            break;
                                                                        case 'B':
                                                                            if (selected < item_count - 1) selected++;
                                                                            break;
                                                                        case '\n':
                                                                            switch (selected) {
                                                                                case 0: generate_initrd_debian(); break;
                                                                                case 1: edit_isolinux_cfg_debian(); break;
                                                                                case 2: edit_grub_cfg_debian(); break;
                                                                                case 3: set_clone_directory(); break;
                                                                                case 4: install_one_time_updater(); break;
                                                                                case 5: free(distro_name); return;
                                                                            }
                                                                            printf("\nPress Enter to continue...");
                                                                            while (getchar() != '\n');
                                                                            break;
                                                                    }
                                                                }
                                                            }
                                                            break;

                                                                                case UNKNOWN:
                                                                                    error_box("Error", "Unsupported Linux distribution");
                                                                                    distro_name = strdup("Linux");
                                                                                    free(distro_name);
                                                                                    break;
    }
}

int main(int argc, char *argv[]) {
    tcgetattr(STDIN_FILENO, &original_term);
    enable_raw_mode();

    Distro distro = detect_distro();
    char *distro_name;
    switch(distro) {
        case ARCH: distro_name = strdup("Arch"); break;
        case UBUNTU: distro_name = strdup("Ubuntu"); break;
        case DEBIAN: distro_name = strdup("Debian"); break;
        default: distro_name = strdup("Linux");
    }

    if (argc > 1) {
        if (strcmp(argv[1], "3") == 0) {
            iso_creator_menu(distro_name);
            disable_raw_mode();
            free(distro_name);
            return 0;
        } else if (strcmp(argv[1], "4") == 0) {
            squashfs_menu(distro_name);
            disable_raw_mode();
            free(distro_name);
            return 0;
        } else if (strcmp(argv[1], "5") == 0) {
            if (distro == ARCH) {
                generate_initrd_arch();
            } else if (distro == UBUNTU) {
                generate_initrd_ubuntu();
            } else if (distro == DEBIAN) {
                generate_initrd_debian();
            }
            disable_raw_mode();
            free(distro_name);
            return 0;
        } else if (strcmp(argv[1], "6") == 0) {
            if (distro == ARCH) {
                edit_isolinux_cfg_arch();
            } else if (distro == UBUNTU) {
                edit_isolinux_cfg_ubuntu();
            } else if (distro == DEBIAN) {
                edit_isolinux_cfg_debian();
            }
            disable_raw_mode();
            free(distro_name);
            return 0;
        } else if (strcmp(argv[1], "7") == 0) {
            if (distro == ARCH) {
                edit_grub_cfg_arch();
            } else if (distro == UBUNTU) {
                edit_grub_cfg_ubuntu();
            } else if (distro == DEBIAN) {
                edit_grub_cfg_debian();
            }
            disable_raw_mode();
            free(distro_name);
            return 0;
        } else if (strcmp(argv[1], "8") == 0) {
            setup_script_menu();
            disable_raw_mode();
            free(distro_name);
            return 0;
        }
    }

    const char *items[] = {
        "SquashFS Creator",
        "ISO Creator",
        "Setup Script",
        "Command Installer",
        "Exit",
        NULL
    };
    int item_count = 5;
    int selected = 0;
    int key;
    while (1) {
        key = show_menu("Main Menu", items, item_count, selected, distro_name);
        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < item_count - 1) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0: squashfs_menu(distro_name); break;
                    case 1: iso_creator_menu(distro_name); break;
                    case 2: setup_script_menu(); break;
                    case 3: command_installer_menu(distro_name); break;
                    case 4:
                        disable_raw_mode();
                        free(distro_name);
                        return 0;
                }
                break;
        }
    }
}
