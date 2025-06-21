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
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <termios.h>

#define MAX_PATH 4096
#define MAX_CMD 16384
#define BLUE "\033[34m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"
#define COLOR_CYAN "\033[38;2;36;255;255m"
#define COLOR_GOLD "\033[33m"
#define PASSWORD_MAX 100

// Terminal control
struct termios original_term;

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

// Your exact ASCII banner
void print_banner() {
    printf("%s", RED);
    printf(
        "░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n"
        "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n"
        "██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n"
        "██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n"
        "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n"
        "░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░\n"
    );
    printf("%s", RESET);
    printf("%sClaudemods Ubuntu ISO Creator Advanced C Script v1.01%s\n", RED, RESET);
}

// Utility functions
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
    ssize_t read = getline(&input, &len, stdin);
    if (read == -1) {
        perror("getline");
        return NULL;
    }
    if (read > 0 && input[read-1] == '\n') {
        input[read-1] = '\0';
    }
    return input;
}

void run_sudo_command(const char *command, const char *password) {
    printf("%sRunning command: %s%s\n", BLUE, command, RESET);
    int pipefd[2];
    if (pipe(pipefd) == -1) {
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
        char full_command[1024];
        snprintf(full_command, sizeof(full_command), "sudo -S %s", command);
        execl("/bin/sh", "sh", "-c", full_command, (char *)NULL);
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

bool dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

char* get_kernel_version() {
    static char version[256] = "unknown";
    FILE* fp = popen("uname -r", "r");
    if (!fp) {
        perror("Failed to get kernel version");
        return version;
    }
    if (fgets(version, sizeof(version), fp)) {
        version[strcspn(version, "\n")] = 0;
    }
    pclose(fp);
    return version;
}

// Menu display function
int show_menu(const char *title, const char *items[], int count, int selected) {
    system("clear");
    print_banner();

    printf("%s  %s%s\n", COLOR_CYAN, title, RESET);
    printf("%s  %.*s%s\n", COLOR_CYAN, (int)strlen(title), "----------------", RESET);

    for (int i = 0; i < count; i++) {
        if (i == selected) {
            printf("%s➤ %s%s\n\n", COLOR_GOLD, items[i], RESET);
        } else {
            printf("%s  %s%s\n\n", COLOR_CYAN, items[i], RESET);
        }
    }

    return get_key();
}

// ==================== ISO CONFIGURATOR FUNCTIONS ====================
void install_dependencies_ubuntu() {
    progress_dialog("Installing dependencies...");
    const char *packages =
    "cryptsetup dmeventd isolinux libaio-dev libcares2 "
    "libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot "
    "live-boot-doc live-boot-initramfs-tools live-config-systemd "
    "live-tools lvm2 pxelinux syslinux syslinux-common "
    "thin-provisioning-tools squashfs-tools xorriso";
    char command[512];
    snprintf(command, sizeof(command), "sudo apt-get install -y %s", packages);
    run_command(command);
    message_box("Success", "Dependencies installed successfully.");
}

void copy_vmlinuz_noble() {
    progress_dialog("Copying vmlinuz (Noble)...");
    run_command("sudo cp /boot/vmlinuz-6.11.0-26-generic /home/$USER/.config/cui/build-image-noble/live/");
    message_box("Success", "Vmlinuz copied successfully.");
}

void copy_vmlinuz_oracular() {
    progress_dialog("Copying vmlinuz (Oracular)...");
    run_command("sudo cp /boot/vmlinuz-6.11.0-26-generic /home/$USER/.config/cui/build-image-oracular/live/");
    message_box("Success", "Vmlinuz copied successfully.");
}

void generate_initrd_noble() {
    progress_dialog("Generating Initramfs (Noble)...");
    run_command("sudo mkinitramfs -o \"/home/$USER/.config/cui/build-image-noble/live/initrd.img-$(uname -r)\" \"$(uname -r)\"");
    message_box("Success", "Initramfs generated successfully.");
}

void generate_initrd_oracular() {
    progress_dialog("Generating Initramfs (Oracular)...");
    run_command("sudo mkinitramfs -o \"/home/$USER/.config/cui/build-image-oracular/live/initrd.img-$(uname -r)\" \"$(uname -r)\"");
    message_box("Success", "Initramfs generated successfully.");
}

void edit_grub_cfg_noble() {
    progress_dialog("Opening grub.cfg (Noble)...");
    run_command("nano /home/$USER/.config/cui/build-image-noble/boot/grub/grub.cfg");
    message_box("Success", "grub.cfg opened for editing.");
}

void edit_grub_cfg_oracular() {
    progress_dialog("Opening grub.cfg (Oracular)...");
    run_command("nano /home/$USER/.config/cui/build-image-oracular/boot/grub/grub.cfg");
    message_box("Success", "grub.cfg opened for editing.");
}

void edit_isolinux_cfg_noble() {
    progress_dialog("Opening isolinux.cfg (Noble)...");
    run_command("nano /home/$USER/.config/cui/build-image-noble/isolinux/isolinux.cfg");
    message_box("Success", "isolinux.cfg opened for editing.");
}

void edit_isolinux_cfg_oracular() {
    progress_dialog("Opening isolinux.cfg (Oracular)...");
    run_command("nano /home/$USER/.config/cui/build-image-oracular/isolinux/isolinux.cfg");
    message_box("Success", "isolinux.cfg opened for editing.");
}

// ==================== SQUASHFS CREATOR FUNCTIONS ====================
void clone_system(const char* clone_dir) {
    if (dir_exists(clone_dir)) {
        printf("Skipping removal of existing clone directory: %s\n", clone_dir);
    } else {
        mkdir(clone_dir, 0755);
    }
    const char* command = "sudo rsync -aHAxSr --numeric-ids --info=progress2 "
    "--include=dev --include=usr --include=proc --include=tmp --include=sys "
    "--include=run --include=media "
    "--exclude=dev/* --exclude=proc/* --exclude=tmp/* --exclude=sys/* "
    "--exclude=run/* --exclude=media/* --exclude=clone_system_temp "
    "/ clone_system_temp";
        printf("Cloning system directory to: %s\n", clone_dir);
        run_command(command);
}

void create_squashfs_image(void) {
    const char* command = "sudo mksquashfs clone_system_temp /home/$USER/.config/cui/build-image-noble/live/filesystem.squashfs "
    "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs";
    printf("Creating SquashFS image: filesystem.squashfs\n");
    run_command(command);
}

void delete_clone_system_temp(void) {
    char command[MAX_CMD];
    strcpy(command, "sudo rm -rf clone_system_temp");
    printf("Deleting temporary clone directory: clone_system_temp\n");
    run_command(command);
    struct stat st;
    if (stat("filesystem.squashfs", &st) == 0) {
        strcpy(command, "sudo rm -f filesystem.squashfs");
        printf("Deleting SquashFS image: filesystem.squashfs\n");
        run_command(command);
    } else {
        printf("SquashFS image does not exist: filesystem.squashfs\n");
    }
}

void squashfs_menu() {
    const char *items[] = {
        "Max compression (xz)",
        "Create SquashFS from clone_system_temp",
        "Delete clone_system_temp and SquashFS image",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;

    while (1) {
        key = show_menu("SquashFS Creator", items, 4, selected);

        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < 3) selected++;
                break;
            case '\n': // Enter
                switch (selected) {
                    case 0:
                        if (!dir_exists("clone_system_temp")) {
                            clone_system("clone_system_temp");
                        }
                        create_squashfs_image();
                        break;
                    case 1:
                        if (!dir_exists("clone_system_temp")) {
                            error_box("Error", "clone_system_temp doesn't exist");
                            break;
                        }
                        create_squashfs_image();
                        break;
                    case 2:
                        delete_clone_system_temp();
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

// ==================== ISO CREATOR FUNCTIONS ====================
void create_iso() {
    char *iso_name = prompt("What do you want to name your .iso? ");
    if (!iso_name || strlen(iso_name) == 0) {
        error_box("Input Error", "ISO name cannot be empty.");
        free(iso_name);
        return;
    }

    char *output_dir = prompt("Enter the output directory path (or press Enter for current directory): ");
    if (!output_dir) {
        output_dir = strdup(".");
    } else if (strlen(output_dir) == 0) {
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

    char build_image_dir[MAX_PATH];
    int needed = snprintf(build_image_dir, sizeof(build_image_dir), "%s/build-image-noble", application_dir_path);
    if (needed >= (int)sizeof(build_image_dir)) {
        error_box("Error", "Path too long for build directory");
        free(iso_name);
        free(output_dir);
        return;
    }

    // Ensure the output directory exists
    struct stat st;
    if (stat(output_dir, &st) == -1) {
        if (mkdir(output_dir, 0755) == -1) {
            perror("mkdir");
            free(iso_name);
            free(output_dir);
            return;
        }
    }

    progress_dialog("Creating ISO...");
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H%M", t);

    char iso_file_name[MAX_PATH];
    needed = snprintf(iso_file_name, sizeof(iso_file_name), "%s/%s_amd64_%s.iso",
                      output_dir, iso_name, timestamp);
    if (needed >= (int)sizeof(iso_file_name)) {
        error_box("Error", "Path too long for ISO filename");
        free(iso_name);
        free(output_dir);
        return;
    }

    char xorriso_command[MAX_CMD];
    needed = snprintf(xorriso_command, sizeof(xorriso_command),
                      "xorriso -as mkisofs -o \"%s\" -V 2025 -iso-level 3 "
                      "-isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin "
                      "-c isolinux/boot.cat -b isolinux/isolinux.bin "
                      "-no-emul-boot -boot-load-size 4 -boot-info-table "
                      "-eltorito-alt-boot -e boot/grub/efi.img "
                      "-no-emul-boot -isohybrid-gpt-basdat \"%s\"",
                      iso_file_name, build_image_dir);
    if (needed >= (int)sizeof(xorriso_command)) {
        error_box("Error", "Command too long for buffer");
        free(iso_name);
        free(output_dir);
        return;
    }

    char *sudo_password = prompt("Enter your sudo password: ");
    if (!sudo_password || strlen(sudo_password) == 0) {
        error_box("Input Error", "Sudo password cannot be empty.");
        free(iso_name);
        free(output_dir);
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
    free(sudo_password);
    free(choice);
}

void run_iso_in_qemu() {
    const char *qemu_script = "/opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb";
    char command[1024];
    snprintf(command, sizeof(command), "ruby %s", qemu_script);
    run_command(command);
}

void iso_creator_menu() {
    const char *items[] = {
        "Create ISO",
        "Run ISO in QEMU",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;

    while (1) {
        key = show_menu("ISO Creator Menu", items, 3, selected);

        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < 2) selected++;
                break;
            case '\n': // Enter
                switch (selected) {
                    case 0:
                        create_iso();
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

// ==================== COMMAND INSTALLER FUNCTIONS ====================
void create_command_files() {
    // Get current executable path
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len == -1) {
        perror("readlink");
        return;
    }
    exe_path[len] = '\0';

    // Create gen-init command
    FILE *f = fopen("gen-init", "w");
    if (!f) {
        perror("fopen gen-init");
        return;
    }
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "if [ \"$1\" = \"--help\" ]; then\n");
    fprintf(f, "  echo \"Usage: gen-init\"\n");
    fprintf(f, "  echo \"Generate initcpio configuration\"\n");
    fprintf(f, "  exit 0\n");
    fprintf(f, "fi\n");
    fprintf(f, "exec %s 5\n", exe_path);
    fclose(f);
    chmod("gen-init", 0755);

    // Create edit-isocfg command
    f = fopen("edit-isocfg", "w");
    if (!f) {
        perror("fopen edit-isocfg");
        return;
    }
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "if [ \"$1\" = \"--help\" ]; then\n");
    fprintf(f, "  echo \"Usage: edit-isocfg\"\n");
    fprintf(f, "  echo \"Edit isolinux.cfg file\"\n");
    fprintf(f, "  exit 0\n");
    fprintf(f, "fi\n");
    fprintf(f, "exec %s 6\n", exe_path);
    fclose(f);
    chmod("edit-isocfg", 0755);

    // Create edit-grubcfg command
    f = fopen("edit-grubcfg", "w");
    if (!f) {
        perror("fopen edit-grubcfg");
        return;
    }
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "if [ \"$1\" = \"--help\" ]; then\n");
    fprintf(f, "  echo \"Usage: edit-grubcfg\"\n");
    fprintf(f, "  echo \"Edit grub.cfg file\"\n");
    fprintf(f, "  exit 0\n");
    fprintf(f, "fi\n");
    fprintf(f, "exec %s 7\n", exe_path);
    fclose(f);
    chmod("edit-grubcfg", 0755);

    // Create setup-script command
    f = fopen("setup-script", "w");
    if (!f) {
        perror("fopen setup-script");
        return;
    }
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "if [ \"$1\" = \"--help\" ]; then\n");
    fprintf(f, "  echo \"Usage: setup-script\"\n");
    fprintf(f, "  echo \"Open setup script menu\"\n");
    fprintf(f, "  exit 0\n");
    fprintf(f, "fi\n");
    fprintf(f, "exec %s 8\n", exe_path);
    fclose(f);
    chmod("setup-script", 0755);

    // Create make-iso command
    f = fopen("make-iso", "w");
    if (!f) {
        perror("fopen make-iso");
        return;
    }
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "if [ \"$1\" = \"--help\" ]; then\n");
    fprintf(f, "  echo \"Usage: make-iso\"\n");
    fprintf(f, "  echo \"Launches the ISO creation menu\"\n");
    fprintf(f, "  exit 0\n");
    fprintf(f, "fi\n");
    fprintf(f, "exec %s 3\n", exe_path);
    fclose(f);
    chmod("make-iso", 0755);

    // Create make-squashfs command
    f = fopen("make-squashfs", "w");
    if (!f) {
        perror("fopen make-squashfs");
        return;
    }
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "if [ \"$1\" = \"--help\" ]; then\n");
    fprintf(f, "  echo \"Usage: make-squashfs\"\n");
    fprintf(f, "  echo \"Launches the SquashFS creation menu\"\n");
    fprintf(f, "  exit 0\n");
    fprintf(f, "fi\n");
    fprintf(f, "exec %s 4\n", exe_path);
    fclose(f);
    chmod("make-squashfs", 0755);

    // Install commands to /usr/bin
    run_command("sudo cp gen-init /usr/bin/");
    run_command("sudo cp edit-isocfg /usr/bin/");
    run_command("sudo cp edit-grubcfg /usr/bin/");
    run_command("sudo cp setup-script /usr/bin/");
    run_command("sudo cp make-iso /usr/bin/");
    run_command("sudo cp make-squashfs /usr/bin/");
    run_command("rm -f gen-init");
    run_command("rm -f edit-isocfg");
    run_command("rm -f edit-grubcfg");
    run_command("rm -f setup-script");
    run_command("rm -f make-iso");
    run_command("rm -f make-squashfs");
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

void command_installer_menu() {
    const char *items[] = {
        "Activate terminal commands",
        "Deactivate terminal commands",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;

    while (1) {
        key = show_menu("Command Installer Menu", items, 3, selected);

        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < 2) selected++;
                break;
            case '\n': // Enter
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

// ==================== DISTRO CONFIGURATION MENUS ====================
void noble_menu() {
    const char *items[] = {
        "Install Dependencies",
        "Copy vmlinuz",
        "Generate Initrd",
        "Edit grub.cfg",
        "Edit isolinux.cfg",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;

    while (1) {
        key = show_menu("Noble 24.04 Configuration", items, 6, selected);

        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < 5) selected++;
                break;
            case '\n': // Enter
                switch (selected) {
                    case 0: install_dependencies_ubuntu(); break;
                    case 1: copy_vmlinuz_noble(); break;
                    case 2: generate_initrd_noble(); break;
                    case 3: edit_grub_cfg_noble(); break;
                    case 4: edit_isolinux_cfg_noble(); break;
                    case 5: return;
                }
                printf("\nPress Enter to continue...");
                while (getchar() != '\n');
                break;
        }
    }
}

void oracular_menu() {
    const char *items[] = {
        "Install Dependencies",
        "Copy vmlinuz",
        "Generate Initrd",
        "Edit grub.cfg",
        "Edit isolinux.cfg",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;

    while (1) {
        key = show_menu("Oracular 24.10 Configuration", items, 6, selected);

        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < 5) selected++;
                break;
            case '\n': // Enter
                switch (selected) {
                    case 0: install_dependencies_ubuntu(); break;
                    case 1: copy_vmlinuz_oracular(); break;
                    case 2: generate_initrd_oracular(); break;
                    case 3: edit_grub_cfg_oracular(); break;
                    case 4: edit_isolinux_cfg_oracular(); break;
                    case 5: return;
                }
                printf("\nPress Enter to continue...");
                while (getchar() != '\n');
                break;
        }
    }
}

// ==================== SETUP SCRIPT MENU ====================
void setup_script_menu() {
    const char *items[] = {
        "Generate initcpio configuration (Noble)",
        "Edit isolinux.cfg (Noble)",
        "Edit grub.cfg (Noble)",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;

    while (1) {
        key = show_menu("Setup Script Menu", items, 4, selected);

        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < 3) selected++;
                break;
            case '\n': // Enter
                switch (selected) {
                    case 0: generate_initrd_noble(); break;
                    case 1: edit_isolinux_cfg_noble(); break;
                    case 2: edit_grub_cfg_noble(); break;
                    case 3: return;
                }
                printf("\nPress Enter to continue...");
                while (getchar() != '\n');
                break;
        }
    }
}

// ==================== MAIN MENU ====================
int main(int argc, char *argv[]) {
    // Save original terminal settings
    tcgetattr(STDIN_FILENO, &original_term);
    enable_raw_mode();

    // Handle command line invocation
    if (argc > 1) {
        if (strcmp(argv[1], "3") == 0) {
            iso_creator_menu();
            disable_raw_mode();
            return 0;
        } else if (strcmp(argv[1], "4") == 0) {
            squashfs_menu();
            disable_raw_mode();
            return 0;
        } else if (strcmp(argv[1], "5") == 0) {
            generate_initrd_noble();
            disable_raw_mode();
            return 0;
        } else if (strcmp(argv[1], "6") == 0) {
            edit_isolinux_cfg_noble();
            disable_raw_mode();
            return 0;
        } else if (strcmp(argv[1], "7") == 0) {
            edit_grub_cfg_noble();
            disable_raw_mode();
            return 0;
        } else if (strcmp(argv[1], "8") == 0) {
            setup_script_menu();
            disable_raw_mode();
            return 0;
        }
    }

    // Normal menu operation
    const char *items[] = {
        "Noble 24.04 Configuration",
        "Oracular 24.10 Configuration",
        "SquashFS Creator",
        "ISO Creator",
        "Setup Script",
        "Command Installer",
        "Exit"
    };
    int selected = 0;
    int key;

    while (1) {
        key = show_menu("Main Menu", items, 7, selected);

        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < 6) selected++;
                break;
            case '\n': // Enter
                switch (selected) {
                    case 0: noble_menu(); break;
                    case 1: oracular_menu(); break;
                    case 2: squashfs_menu(); break;
                    case 3: iso_creator_menu(); break;
                    case 4: setup_script_menu(); break;
                    case 5: command_installer_menu(); break;
                    case 6:
                        disable_raw_mode();
                        return 0;
                }
                break;
        }
    }
}
