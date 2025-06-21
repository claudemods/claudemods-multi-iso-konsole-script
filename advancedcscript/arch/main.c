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
#define COLOR_CYAN "\033[34m"
#define COLOR_GOLD "\033[38;2;36;255;255m"
#define PASSWORD_MAX 100

// Terminal control
struct termios original_term;

// Function prototypes
void enable_raw_mode();
void disable_raw_mode();
int get_key();
void print_banner();
void print_blue(const char *text);
void message_box(const char *title, const char *message);
void error_box(const char *title, const char *message);
void progress_dialog(const char *message);
void run_command(const char *command);
char* prompt(const char *prompt_text);
void run_sudo_command(const char *command, const char *password);
bool dir_exists(const char* path);
char* get_kernel_version();
int show_menu(const char *title, const char *items[], int count, int selected);
void install_dependencies_arch();
void copy_vmlinuz_arch();
void generate_initrd_arch();
void edit_grub_cfg_arch();
void edit_isolinux_cfg_arch();
void save_clone_dir(const char* dir_path);
char* read_clone_dir();
void clone_system(const char* clone_dir);
void create_squashfs_image(void);
void delete_clone_system_temp(void);
void set_clone_directory();
void install_one_time_updater();
void squashfs_menu();
void create_iso();
void run_iso_in_qemu();
void iso_creator_menu();
void create_command_files();
void remove_command_files();
void command_installer_menu();
void setup_script_menu();

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
    printf("%sClaudemods Arch ISO Creator Advanced C Script v1.01 21-06-2025%s\n", RED, RESET);

    // Display current date/time in UK format
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char datetime[50];
    strftime(datetime, sizeof(datetime), "%d/%m/%Y %H:%M:%S", t);
    printf("%sCurrent UK Time: %s%s\n", GREEN, datetime, RESET);

    // Display system disk usage
    printf("%sDisk Usage:%s\n", GREEN, RESET);
    run_command("df -h | grep -E 'Filesystem|/$'");
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

    char command[512];
    snprintf(command, sizeof(command), "sudo pacman -S --needed --noconfirm %s", packages);
    run_command(command);
    message_box("Success", "Dependencies installed successfully.");
}

void copy_vmlinuz_arch() {
    char kernel_version[256];
    FILE* fp = popen("uname -r", "r");
    if (fp) {
        if (fgets(kernel_version, sizeof(kernel_version), fp)) {
            kernel_version[strcspn(kernel_version, "\n")] = 0;
        }
        pclose(fp);
    }

    char command[512];
    snprintf(command, sizeof(command),
             "sudo cp /boot/vmlinuz-%s /home/$USER/.config/cmi/build-image-arch/live/",
             kernel_version);
    run_command(command);
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

void save_clone_dir(const char* dir_path) {
    char config_dir[MAX_PATH];
    snprintf(config_dir, sizeof(config_dir), "/home/%s/.config/cmi", getenv("USER"));

    if (!dir_exists(config_dir)) {
        char mkdir_cmd[MAX_PATH + 10];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", config_dir);
        run_command(mkdir_cmd);
    }

    char file_path[MAX_PATH];
    snprintf(file_path, sizeof(file_path), "%s/clonedir.txt", config_dir);

    FILE* f = fopen(file_path, "w");
    if (!f) {
        perror("Failed to open clonedir.txt");
        return;
    }

    fprintf(f, "%s", dir_path);
    fclose(f);
    message_box("Success", "Clone directory path saved successfully.");
}

char* read_clone_dir() {
    static char dir_path[MAX_PATH] = "";
    char file_path[MAX_PATH];
    snprintf(file_path, sizeof(file_path), "/home/%s/.config/cmi/clonedir.txt", getenv("USER"));

    FILE* f = fopen(file_path, "r");
    if (!f) {
        return NULL;
    }

    if (fgets(dir_path, sizeof(dir_path), f)) {
        dir_path[strcspn(dir_path, "\n")] = '\0';
    }
    fclose(f);

    return dir_path;
}

void clone_system(const char* clone_dir) {
    if (dir_exists(clone_dir)) {
        printf("Skipping removal of existing clone directory: %s\n", clone_dir);
    } else {
        mkdir(clone_dir, 0755);
    }

    char command[MAX_CMD];
    snprintf(command, sizeof(command),
             "sudo rsync -aHAxSr --numeric-ids --info=progress2 "
             "--include=dev --include=usr --include=proc --include=tmp --include=sys "
             "--include=run --include=media "
             "--exclude=dev/* --exclude=proc/* --exclude=tmp/* --exclude=sys/* "
             "--exclude=run/* --exclude=media/* --exclude=%s "
             "/ %s", clone_dir, clone_dir);

    printf("Cloning system directory to: %s\n", clone_dir);
    run_command(command);
}

void create_squashfs_image(void) {
    char* clone_dir = read_clone_dir();
    if (!clone_dir || strlen(clone_dir) == 0) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }

    char command[MAX_CMD];
    snprintf(command, sizeof(command),
             "sudo mksquashfs %s /home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs "
             "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
             "-always-use-fragments -wildcards -xattrs", clone_dir);

    printf("Creating SquashFS image from: %s\n", clone_dir);
    run_command(command);
}

void delete_clone_system_temp(void) {
    char* clone_dir = read_clone_dir();
    if (!clone_dir || strlen(clone_dir) == 0) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }

    char command[MAX_CMD];
    snprintf(command, sizeof(command), "sudo rm -rf %s", clone_dir);
    printf("Deleting temporary clone directory: %s\n", clone_dir);
    run_command(command);

    struct stat st;
    if (stat("filesystem.squashfs", &st) == 0) {
        strcpy(command, "sudo rm -f /home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs");
        printf("Deleting SquashFS image: filesystem.squashfs\n");
        run_command(command);
    } else {
        printf("SquashFS image does not exist: filesystem.squashfs\n");
    }
}

void set_clone_directory() {
    char* dir_path = prompt("Enter full path for clone_system_temp directory: ");
    if (!dir_path || strlen(dir_path) == 0) {
        error_box("Error", "Directory path cannot be empty");
        return;
    }

    save_clone_dir(dir_path);
    free(dir_path);
}

void install_one_time_updater() {
    progress_dialog("Installing one-time updater...");

    const char* commands[] = {
        "cd /home/$USER",
        "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git >/dev/null 2>&1",
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

void squashfs_menu() {
    const char *items[] = {
        "Max compression (xz)",
        "Create SquashFS from clone directory",
        "Delete clone directory and SquashFS image",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;

    while (1) {
        key = show_menu("SquashFS Creator", items, 4, selected);

        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < 3) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0:
                    {
                        char* clone_dir = read_clone_dir();
                        if (!clone_dir || strlen(clone_dir) == 0) {
                            error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
                            break;
                        }
                        if (!dir_exists(clone_dir)) {
                            clone_system(clone_dir);
                        }
                        create_squashfs_image();
                    }
                    break;
                    case 1:
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
    int needed = snprintf(build_image_dir, sizeof(build_image_dir), "%s/build-image-arch", application_dir_path);
    if (needed >= (int)sizeof(build_image_dir)) {
        error_box("Error", "Path too long for build directory");
        free(iso_name);
        free(output_dir);
        return;
    }

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
                      "sudo xorriso -as mkisofs -o \"%s\" -V 2025 -iso-level 3 "
                      "-isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin -c isolinux/boot.cat "
                      "-b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table "
                      "-eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat \"%s\"",
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
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < 2) selected++;
                break;
            case '\n':
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

void create_command_files() {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len == -1) {
        perror("readlink");
        return;
    }
    exe_path[len] = '\0';

    // Create files directly in /usr/bin with sudo
    char *sudo_password = prompt("Enter sudo password to create command files: ");
    if (!sudo_password || strlen(sudo_password) == 0) {
        error_box("Error", "Sudo password cannot be empty");
        return;
    }

    // gen-init
    char command[MAX_CMD];
    snprintf(command, sizeof(command),
             "sudo bash -c 'cat > /usr/bin/gen-init << \"EOF\"\n"
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
    snprintf(command, sizeof(command),
             "sudo bash -c 'cat > /usr/bin/edit-isocfg << \"EOF\"\n"
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
    snprintf(command, sizeof(command),
             "sudo bash -c 'cat > /usr/bin/edit-grubcfg << \"EOF\"\n"
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
    snprintf(command, sizeof(command),
             "sudo bash -c 'cat > /usr/bin/setup-script << \"EOF\"\n"
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
    snprintf(command, sizeof(command),
             "sudo bash -c 'cat > /usr/bin/make-iso << \"EOF\"\n"
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
    snprintf(command, sizeof(command),
             "sudo bash -c 'cat > /usr/bin/make-squashfs << \"EOF\"\n"
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

    printf("%sActivated! You can now use all commands in your terminal.%s\n", GREEN, RESET);
    free(sudo_password);
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
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < 2) selected++;
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
    const char *items[] = {
        "Generate initcpio configuration (arch)",
        "Edit isolinux.cfg (arch)",
        "Edit grub.cfg (arch)",
        "Set clone directory path",
        "Install One Time Updater",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;

    while (1) {
        key = show_menu("Setup Script Menu", items, 6, selected);

        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < 5) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0: generate_initrd_arch(); break;
                    case 1: edit_isolinux_cfg_arch(); break;
                    case 2: edit_grub_cfg_arch(); break;
                    case 3: set_clone_directory(); break;
                    case 4: install_one_time_updater(); break;
                    case 5: return;
                }
                printf("\nPress Enter to continue...");
                while (getchar() != '\n');
                break;
        }
    }
}

int main(int argc, char *argv[]) {
    tcgetattr(STDIN_FILENO, &original_term);
    enable_raw_mode();

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
            generate_initrd_arch();
            disable_raw_mode();
            return 0;
        } else if (strcmp(argv[1], "6") == 0) {
            edit_isolinux_cfg_arch();
            disable_raw_mode();
            return 0;
        } else if (strcmp(argv[1], "7") == 0) {
            edit_grub_cfg_arch();
            disable_raw_mode();
            return 0;
        } else if (strcmp(argv[1], "8") == 0) {
            setup_script_menu();
            disable_raw_mode();
            return 0;
        }
    }

    const char *items[] = {
        "SquashFS Creator",
        "ISO Creator",
        "Setup Script",
        "Command Installer",
        "Exit"
    };
    int selected = 0;
    int key;

    while (1) {
        key = show_menu("Main Menu", items, 5, selected);

        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < 4) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0: squashfs_menu(); break;
                    case 1: iso_creator_menu(); break;
                    case 2: setup_script_menu(); break;
                    case 3: command_installer_menu(); break;
                    case 4:
                        disable_raw_mode();
                        return 0;
                }
                break;
        }
    }
}
