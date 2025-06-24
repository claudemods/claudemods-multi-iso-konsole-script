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
void install_calamares();
void generate_calamares(); // New function declaration
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

void print_blue(const char *text) {
    printf("%s%s%s\n", BLUE, text, RESET);
}

void message_box(const char *title, const char *message) {
    printf("%s%s%s\n", GREEN, title, RESET);
    printf("%s%s%s\n", GREEN, message, RESET);
}

void error_box(const char *title, const char *message) {
    printf("%s%s%s\n", RED, title, RESET);
    printf("%s%s%s\n", RED, message, RESET);
}

void progress_dialog(const char *message) {
    printf("%s%s%s\n", BLUE, message, RESET);
}

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
    printf("%sClaudemods Arch ISO Creator Advanced C Script v1.01 24-06-2025%s\n", RED, RESET);

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
        strcpy(full_command, "sudo -S ");
        strcat(full_command, command);
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
            printf("%s➤ %s%s\n", COLOR_GOLD, items[i], RESET);
        } else {
            printf("%s  %s%s\n", COLOR_CYAN, items[i], RESET);
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
    strcpy(command, "sudo pacman -S --needed --noconfirm ");
    strcat(command, packages);
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
    strcpy(command, "sudo cp /boot/vmlinuz-");
    strcat(command, kernel_version);
    strcat(command, " /home/$USER/.config/cmi/build-image-arch/live/");
    run_command(command);
    message_box("Success", "Vmlinuz copied successfully.");
}

void generate_initrd_arch() {
    progress_dialog("Generating Initramfs (Arch)...");
    run_command("cd /home/$USER/.config/cmi");
    run_command("cd /home/$USER/.config/cmi/build-image-arch && sudo mkinitcpio -c live.conf -g /home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img");
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
    strcpy(config_dir, "/home/$USER/.config/cmi");
    if (!dir_exists(config_dir)) {
        char mkdir_cmd[MAX_PATH + 10];
        strcpy(mkdir_cmd, "mkdir -p ");
        strcat(mkdir_cmd, config_dir);
        run_command(mkdir_cmd);
    }
    char file_path[MAX_PATH];
    strcpy(file_path, config_dir);
    strcat(file_path, "/clonedir.txt");
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
    strcpy(file_path, "/home/$USER/.config/cmi/clonedir.txt");
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
    strcpy(command, "sudo rsync -aHAxSr --numeric-ids --info=progress2 ");
    strcat(command, "--include=dev --include=usr --include=proc --include=tmp --include=sys ");
    strcat(command, "--include=run --include=media ");
    strcat(command, "--exclude=dev/* --exclude=proc/* --exclude=tmp/* --exclude=sys/* ");
    strcat(command, "--exclude=run/* --exclude=media/* --exclude=");
    strcat(command, clone_dir);
    strcat(command, " / ");
    strcat(command, clone_dir);
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
    strcpy(command, "sudo mksquashfs ");
    strcat(command, clone_dir);
    strcat(command, " /home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs ");
    strcat(command, "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery ");
    strcat(command, "-always-use-fragments -wildcards -xattrs");
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
    strcpy(command, "sudo rm -rf ");
    strcat(command, clone_dir);
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

void install_calamares() {
    progress_dialog("Installing Calamares...");
    run_command("cd /home/$USER/.config/cmi/calamares-per-distro/arch && sudo pacman -U calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar ckbcomp-1.227-2-any.pkg.tar.zst");
    message_box("Success", "Calamares installed successfully.");
}

void generate_calamares() {
    progress_dialog("Installing Calamares...");
    run_command("cd /home/$USER/.config/cmi/calamares-per-distro/arch && sudo pacman -U calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar ckbcomp-1.227-2-any.pkg.tar.zst");
    message_box("Success", "Calamares installed successfully.");
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
    strcpy(build_image_dir, application_dir_path);
    strcat(build_image_dir, "/build-image-arch");
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
    strcpy(iso_file_name, output_dir);
    strcat(iso_file_name, "/");
    strcat(iso_file_name, iso_name);
    strcat(iso_file_name, "_amd64_");
    strcat(iso_file_name, timestamp);
    strcat(iso_file_name, ".iso");
    char xorriso_command[MAX_CMD];
    strcpy(xorriso_command, "sudo xorriso -as mkisofs -o \"");
    strcat(xorriso_command, iso_file_name);
    strcat(xorriso_command, "\" -V 2025 -iso-level 3 ");
    strcat(xorriso_command, "-isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin -c isolinux/boot.cat ");
    strcat(xorriso_command, "-b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table ");
    strcat(xorriso_command, "-eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat \"");
    strcat(xorriso_command, build_image_dir);
    strcat(xorriso_command, "\"");
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
    strcpy(command, "ruby ");
    strcat(command, qemu_script);
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

    char *sudo_password = prompt("Enter sudo password to create command files: ");
    if (!sudo_password || strlen(sudo_password) == 0) {
        error_box("Error", "Sudo password cannot be empty");
        return;
    }

    // gen-init
    char command[MAX_CMD];
    strcpy(command, "sudo bash -c 'cat > /usr/bin/gen-init << \"EOF\"\n");
    strcat(command, "#!/bin/sh\n");
    strcat(command, "if [ \"$1\" = \"--help\" ]; then\n");
    strcat(command, "  echo \"Usage: gen-init\"\n");
    strcat(command, "  echo \"Generate initcpio configuration\"\n");
    strcat(command, "  exit 0\n");
    strcat(command, "fi\n");
    strcat(command, "exec ");
    strcat(command, exe_path);
    strcat(command, " 5\n");
    strcat(command, "EOF\n");
    strcat(command, "chmod 755 /usr/bin/gen-init'");
    run_sudo_command(command, sudo_password);

    // edit-isocfg
    strcpy(command, "sudo bash -c 'cat > /usr/bin/edit-isocfg << \"EOF\"\n");
    strcat(command, "#!/bin/sh\n");
    strcat(command, "if [ \"$1\" = \"--help\" ]; then\n");
    strcat(command, "  echo \"Usage: edit-isocfg\"\n");
    strcat(command, "  echo \"Edit isolinux.cfg file\"\n");
    strcat(command, "  exit 0\n");
    strcat(command, "fi\n");
    strcat(command, "exec ");
    strcat(command, exe_path);
    strcat(command, " 6\n");
    strcat(command, "EOF\n");
    strcat(command, "chmod 755 /usr/bin/edit-isocfg'");
    run_sudo_command(command, sudo_password);

    // edit-grubcfg
    strcpy(command, "sudo bash -c 'cat > /usr/bin/edit-grubcfg << \"EOF\"\n");
    strcat(command, "#!/bin/sh\n");
    strcat(command, "if [ \"$1\" = \"--help\" ]; then\n");
    strcat(command, "  echo \"Usage: edit-grubcfg\"\n");
    strcat(command, "  echo \"Edit grub.cfg file\"\n");
    strcat(command, "  exit 0\n");
    strcat(command, "fi\n");
    strcat(command, "exec ");
    strcat(command, exe_path);
    strcat(command, " 7\n");
    strcat(command, "EOF\n");
    strcat(command, "chmod 755 /usr/bin/edit-grubcfg'");
    run_sudo_command(command, sudo_password);

    // setup-script
    strcpy(command, "sudo bash -c 'cat > /usr/bin/setup-script << \"EOF\"\n");
    strcat(command, "#!/bin/sh\n");
    strcat(command, "if [ \"$1\" = \"--help\" ]; then\n");
    strcat(command, "  echo \"Usage: setup-script\"\n");
    strcat(command, "  echo \"Open setup script menu\"\n");
    strcat(command, "  exit 0\n");
    strcat(command, "fi\n");
    strcat(command, "exec ");
    strcat(command, exe_path);
    strcat(command, " 8\n");
    strcat(command, "EOF\n");
    strcat(command, "chmod 755 /usr/bin/setup-script'");
    run_sudo_command(command, sudo_password);

    // make-iso
    strcpy(command, "sudo bash -c 'cat > /usr/bin/make-iso << \"EOF\"\n");
    strcat(command, "#!/bin/sh\n");
    strcat(command, "if [ \"$1\" = \"--help\" ]; then\n");
    strcat(command, "  echo \"Usage: make-iso\"\n");
    strcat(command, "  echo \"Launches the ISO creation menu\"\n");
    strcat(command, "  exit 0\n");
    strcat(command, "fi\n");
    strcat(command, "exec ");
    strcat(command, exe_path);
    strcat(command, " 3\n");
    strcat(command, "EOF\n");
    strcat(command, "chmod 755 /usr/bin/make-iso'");
    run_sudo_command(command, sudo_password);

    // make-squashfs
    strcpy(command, "sudo bash -c 'cat > /usr/bin/make-squashfs << \"EOF\"\n");
    strcat(command, "#!/bin/sh\n");
    strcat(command, "if [ \"$1\" = \"--help\" ]; then\n");
    strcat(command, "  echo \"Usage: make-squashfs\"\n");
    strcat(command, "  echo \"Launches the SquashFS creation menu\"\n");
    strcat(command, "  exit 0\n");
    strcat(command, "fi\n");
    strcat(command, "exec ");
    strcat(command, exe_path);
    strcat(command, " 4\n");
    strcat(command, "EOF\n");
    strcat(command, "chmod 755 /usr/bin/make-squashfs'");
    run_sudo_command(command, sudo_password);

    // gen-calamares
    strcpy(command, "sudo bash -c 'cat > /usr/bin/gen-calamares << \"EOF\"\n");
    strcat(command, "#!/bin/sh\n");
    strcat(command, "if [ \"$1\" = \"--help\" ]; then\n");
    strcat(command, "  echo \"Usage: gen-calamares\"\n");
    strcat(command, "  echo \"Install Calamares installer framework\"\n");
    strcat(command, "  exit 0\n");
    strcat(command, "fi\n");
    strcat(command, "exec ");
    strcat(command, exe_path);
    strcat(command, " 9\n");
    strcat(command, "EOF\n");
    strcat(command, "chmod 755 /usr/bin/gen-calamares'");
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
    run_command("sudo rm -f /usr/bin/gen-calamares");
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
        "Install One Time Updater",
        "Install Calamares",
        "Set clone directory path",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;
    while (1) {
        key = show_menu("Setup Script Menu", items, 7, selected);
        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < 6) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0: generate_initrd_arch(); break;
                    case 1: edit_isolinux_cfg_arch(); break;
                    case 2: edit_grub_cfg_arch(); break;
                    case 3: install_one_time_updater(); break;
                    case 4: install_calamares(); break;
                    case 5: set_clone_directory(); break;
                    case 6: return;
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
        } else if (strcmp(argv[1], "9") == 0) {
            generate_calamares();
            disable_raw_mode();
            return 0;
        }
    }

    const char *items[] = {
        "Guide",
        "Setup Script",
        "SquashFS Creator",
        "ISO Creator",
        "Command Installer",
        "Exit"
    };
    int selected = 0;
    int key;

    while (1) {
        key = show_menu("Main Menu", items, 6, selected);
        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < 5) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0:
                        run_command("nano /home/$USER/.config/cmi/readme.txt");
                        break;
                    case 1: 
                        setup_script_menu(); 
                        break;
                    case 2: 
                        squashfs_menu(); 
                        break;
                    case 3: 
                        iso_creator_menu(); 
                        break;
                    case 4: 
                        command_installer_menu(); 
                        break;
                    case 5:
                        disable_raw_mode();
                        return 0;
                }
                break;
        }
    }
}
