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
#include <pwd.h>
#include <stdatomic.h>
#include <pthread.h>

#define MAX_PATH 4096
#define MAX_CMD 16384
#define BLUE "\033[34m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"
#define COLOR_CYAN "\033[38;2;0;255;255m"
#define COLOR_GOLD "\033[38;2;36;255;255m"
#define PASSWORD_MAX 100
#define COLOR_RESET "\033[0m"

struct termios original_term;
enum Distro { ARCH, CACHYOS, UNKNOWN };
atomic_bool time_thread_running = true;
pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;
char current_time_str[50];
int should_reset = 0;

char* get_kernel_version();
char* read_clone_dir();
int dir_exists(const char *path);
void execute_command(const char* cmd);
enum Distro detect_distro();
const char* get_distro_name(enum Distro distro);
void edit_calamares_branding();
int is_init_generated(enum Distro distro);
char* get_clone_dir_status();
char* get_init_status(enum Distro distro);
char* get_iso_name();
char* get_iso_name_status();
void message_box(const char *title, const char *message);
void error_box(const char *title, const char *message);
void progress_dialog(const char *message);
void enable_raw_mode();
void disable_raw_mode();
const char* get_highlight_color(enum Distro distro);
char* get_input(const char *prompt_text, int echo);
char* prompt(const char *prompt_text);
char* password_prompt(const char *prompt_text);
void* update_time_thread(void* arg);
void print_banner(enum Distro distro);
int get_key();
void print_blue(const char *text);
void install_dependencies_arch();
void generate_initrd_arch();
void edit_grub_cfg_arch();
void edit_isolinux_cfg_arch();
void install_calamares_arch();
void install_dependencies_cachyos();
void install_calamares_cachyos();
void clone_system(const char *clone_dir);
void create_squashfs_image(enum Distro distro);
void delete_clone_system_temp(enum Distro distro);
void set_clone_directory();
void install_one_time_updater();
void squashfs_menu(enum Distro distro);
void create_iso(enum Distro distro);
void run_iso_in_qemu();
void iso_creator_menu(enum Distro distro);
void create_command_files();
void remove_command_files();
void command_installer_menu(enum Distro distro);
void setup_script_menu(enum Distro distro);
void save_iso_name(const char *name);
void set_iso_name();
void save_clone_dir(const char *dir_path);

char* get_input(const char *prompt_text, int echo) {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    if (echo) {
        newt.c_lflag |= (ICANON | ECHO);
    } else {
        newt.c_lflag &= ~(ECHO);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    printf("%s%s", GREEN, prompt_text);
    printf("%s", RESET);
    fflush(stdout);

    char *input = NULL;
    size_t len = 0;
    ssize_t read = getline(&input, &len, stdin);

    if (read == -1) {
        free(input);
        return NULL;
    }

    if (input[read-1] == '\n') {
        input[read-1] = '\0';
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return input;
}

int is_init_generated(enum Distro distro) {
    char init_path[MAX_PATH];
    const char *user = getenv("USER");

    strcpy(init_path, "/home/");
    strcat(init_path, user);
    strcat(init_path, "/.config/cmi/build-image-arch/live/initramfs-linux.img");

    struct stat st;
    return stat(init_path, &st) == 0;
}

char* get_clone_dir_status() {
    char *clone_dir = read_clone_dir();
    char *result = malloc(MAX_PATH + 100);

    if (clone_dir == NULL || strlen(clone_dir) == 0) {
        strcpy(result, RED);
        strcat(result, "✗ Clone directory not set (Use Option 4 in Setup Script Menu)");
        strcat(result, RESET);
    } else {
        strcpy(result, GREEN);
        strcat(result, "✓ Clone directory: ");
        strcat(result, clone_dir);
        strcat(result, RESET);
    }

    free(clone_dir);
    return result;
}

char* get_init_status(enum Distro distro) {
    char *result = malloc(100);
    if (is_init_generated(distro)) {
        strcpy(result, GREEN);
        strcat(result, "✓ Initramfs generated");
        strcat(result, RESET);
    } else {
        strcpy(result, RED);
        strcat(result, "✗ Initramfs not generated (Use Option 1 in Setup Script Menu)");
        strcat(result, RESET);
    }
    return result;
}

char* get_iso_name() {
    const char *user = getenv("USER");
    char file_path[MAX_PATH];
    strcpy(file_path, "/home/");
    strcat(file_path, user);
    strcat(file_path, "/.config/cmi/isoname.txt");

    FILE *f = fopen(file_path, "r");
    if (!f) return NULL;

    char *name = NULL;
    size_t len = 0;
    ssize_t read = getline(&name, &len, f);
    fclose(f);

    if (read == -1) {
        free(name);
        return NULL;
    }

    if (name[read-1] == '\n') {
        name[read-1] = '\0';
    }

    return name;
}

char* get_iso_name_status() {
    char *iso_name = get_iso_name();
    char *result = malloc(MAX_PATH + 100);

    if (iso_name == NULL || strlen(iso_name) == 0) {
        strcpy(result, RED);
        strcat(result, "✗ ISO name not set (Use Option 3 in Setup Script Menu)");
        strcat(result, RESET);
    } else {
        strcpy(result, GREEN);
        strcat(result, "✓ ISO name: ");
        strcat(result, iso_name);
        strcat(result, RESET);
    }

    free(iso_name);
    return result;
}

int dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
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
    printf("%s%s%s\n", GREEN, message, RESET);
}

void enable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_term);
}

void execute_command(const char* cmd) {
    printf("%s", COLOR_CYAN);
    fflush(stdout);

    char full_cmd[MAX_CMD + 10];
    strcpy(full_cmd, " ");
    strcat(full_cmd, cmd);

    int status = system(full_cmd);
    printf("%s", COLOR_RESET);

    if (status != 0) {
        fprintf(stderr, "%sError executing: %s%s\n", RED, full_cmd, RESET);
        exit(1);
    }
}

enum Distro detect_distro() {
    FILE *os_release = fopen("/etc/os-release", "r");
    if (!os_release) return UNKNOWN;

    char line[256];
    while (fgets(line, sizeof(line), os_release)) {
        if (strstr(line, "ID=") == line) {
            if (strstr(line, "arch")) {
                fclose(os_release);
                return ARCH;
            }
            if (strstr(line, "cachyos")) {
                fclose(os_release);
                return CACHYOS;
            }
        }
    }

    fclose(os_release);
    return UNKNOWN;
}

const char* get_highlight_color(enum Distro distro) {
    switch(distro) {
        case ARCH: return "\033[34m";
        case CACHYOS: return "\033[34m";
        default: return "\033[36m";
    }
}

const char* get_distro_name(enum Distro distro) {
    switch(distro) {
        case ARCH: return "Arch";
        case CACHYOS: return "CachyOS";
        default: return "Unknown";
    }
}

char* get_kernel_version() {
    FILE* fp = popen("uname -r", "r");
    if (!fp) {
        perror("Failed to get kernel version");
        return strdup("unknown");
    }

    char buffer[256];
    if (!fgets(buffer, sizeof(buffer), fp)) {
        pclose(fp);
        return strdup("unknown");
    }
    pclose(fp);

    char *newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';

    return strdup(buffer);
}

char* read_clone_dir() {
    const char *user = getenv("USER");
    char file_path[MAX_PATH];
    strcpy(file_path, "/home/");
    strcat(file_path, user);
    strcat(file_path, "/.config/cmi/clonedir.txt");

    FILE *f = fopen(file_path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size == 0) {
        fclose(f);
        return NULL;
    }

    char *dir_path = malloc(size + 1);
    if (!dir_path) {
        fclose(f);
        return NULL;
    }

    fread(dir_path, 1, size, f);
    fclose(f);
    dir_path[size] = '\0';

    char *newline = strchr(dir_path, '\n');
    if (newline) *newline = '\0';

    if (dir_path[strlen(dir_path)-1] != '/') {
        char *new_path = realloc(dir_path, strlen(dir_path) + 2);
        if (!new_path) {
            free(dir_path);
            return NULL;
        }
        strcat(new_path, "/");
        return new_path;
    }

    return dir_path;
}

void save_clone_dir(const char *dir_path) {
    const char *user = getenv("USER");
    char config_dir[MAX_PATH];
    strcpy(config_dir, "/home/");
    strcat(config_dir, user);
    strcat(config_dir, "/.config/cmi");

    if (!dir_exists(config_dir)) {
        char mkdir_cmd[MAX_PATH + 20];
        strcpy(mkdir_cmd, "mkdir -p ");
        strcat(mkdir_cmd, config_dir);
        execute_command(mkdir_cmd);
    }

    char full_clone_path[MAX_PATH];
    strcpy(full_clone_path, dir_path);
    if (full_clone_path[strlen(full_clone_path)-1] != '/') {
        strcat(full_clone_path, "/");
    }

    char file_path[MAX_PATH];
    strcpy(file_path, config_dir);
    strcat(file_path, "/clonedir.txt");
    FILE *f = fopen(file_path, "w");
    if (!f) {
        perror("Failed to open clonedir.txt");
        return;
    }
    fputs(full_clone_path, f);
    fclose(f);

    char clone_folder[MAX_PATH];
    strcpy(clone_folder, full_clone_path);
    strcat(clone_folder, "clone_system_temp");
    if (!dir_exists(clone_folder)) {
        char mkdir_cmd[MAX_PATH];
        strcpy(mkdir_cmd, "mkdir -p ");
        strcat(mkdir_cmd, clone_folder);
        execute_command(mkdir_cmd);
    }

    for (int i = 5; i > 0; i--) {
        sleep(1);
    }
    should_reset = 1;
}

void save_iso_name(const char *name) {
    const char *user = getenv("USER");
    char config_dir[MAX_PATH];
    strcpy(config_dir, "/home/");
    strcat(config_dir, user);
    strcat(config_dir, "/.config/cmi");

    if (!dir_exists(config_dir)) {
        char mkdir_cmd[MAX_PATH + 20];
        strcpy(mkdir_cmd, "mkdir -p ");
        strcat(mkdir_cmd, config_dir);
        execute_command(mkdir_cmd);
    }

    char file_path[MAX_PATH];
    strcpy(file_path, config_dir);
    strcat(file_path, "/isoname.txt");
    FILE *f = fopen(file_path, "w");
    if (!f) {
        perror("Failed to open isoname.txt");
        return;
    }
    fputs(name, f);
    fclose(f);
    should_reset = 1;
}

void set_iso_name() {
    char *name = prompt("Enter ISO name (without extension): ");
    if (name == NULL || strlen(name) == 0) {
        error_box("Error", "ISO name cannot be empty");
        free(name);
        return;
    }
    save_iso_name(name);
    free(name);
}

void print_banner(enum Distro distro) {
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
    printf("%sClaudemods Multi Iso Creator Advanced C Script v2.0 DevBranch 28-06-2025%s\n", RED, RESET);

    pthread_mutex_lock(&time_mutex);
    printf("%sCurrent UK Time: %s%s\n", GREEN, current_time_str, RESET);
    pthread_mutex_unlock(&time_mutex);

    char *clone_status = get_clone_dir_status();
    printf("%s\n", clone_status);
    free(clone_status);

    char *init_status = get_init_status(distro);
    printf("%s\n", init_status);
    free(init_status);

    char *iso_status = get_iso_name_status();
    printf("%s\n", iso_status);
    free(iso_status);

    printf("%sDisk Usage:%s\n", GREEN, RESET);
    execute_command("df -h /");
}

int get_key() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    int retval = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

    if (retval == -1) {
        perror("select()");
        return 0;
    } else if (retval) {
        int c = getchar();
        if (c == '\033') {
            getchar();
            return getchar();
        }
        return c;
    }
    return 0;
}

void print_blue(const char *text) {
    printf("%s%s%s\n", BLUE, text, RESET);
}

char* prompt(const char *prompt_text) {
    return get_input(prompt_text, 1);
}

char* password_prompt(const char *prompt_text) {
    return get_input(prompt_text, 0);
}

void* update_time_thread(void* arg) {
    (void)arg;
    while (time_thread_running) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char datetime[50];
        strftime(datetime, sizeof(datetime), "%d/%m/%Y %H:%M:%S", t);

        pthread_mutex_lock(&time_mutex);
        strcpy(current_time_str, datetime);
        pthread_mutex_unlock(&time_mutex);

        sleep(1);
    }
    return NULL;
}

int show_menu(const char *title, const char **items, int item_count, int selected, enum Distro distro) {
    system("clear");
    print_banner(distro);

    const char *highlight_color = get_highlight_color(distro);

    printf("%s  %s%s\n", COLOR_CYAN, title, RESET);
    printf("%s  %.*s%s\n", COLOR_CYAN, (int)strlen(title), "----------------------------------------", RESET);

    for (int i = 0; i < item_count; i++) {
        if (i == selected) {
            printf("%s➤ %s%s\n", highlight_color, items[i], RESET);
        } else {
            printf("%s  %s%s\n", COLOR_CYAN, items[i], RESET);
        }
    }

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    int retval = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

    int key = 0;
    if (retval == -1) {
        perror("select()");
    } else if (retval) {
        int c = getchar();
        if (c == '\033') {
            getchar();
            key = getchar();
        } else if (c == '\n') {
            key = '\n';
        } else {
            key = c;
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return key;
}

void install_dependencies_arch() {
    progress_dialog("Installing dependencies...");
    const char *packages =
    "arch-install-scripts "
    "bash-completion "
    "dosfstools "
    "erofs-utils "
    "findutils "
    "grub "
    "jq "
    "libarchive "
    "libisoburn "
    "lsb-release "
    "lvm2 "
    "mkinitcpio-archiso "
    "mkinitcpio-nfs-utils "
    "mtools "
    "nbd "
    "pacman-contrib "
    "parted "
    "procps-ng "
    "pv "
    "python "
    "rsync "
    "squashfs-tools "
    "sshfs "
    "syslinux "
    "xdg-utils "
    "bash-completion "
    "zsh-completions "
    "kernel-modules-hook "
    "virt-manager ";

    char cmd[1024];
    strcpy(cmd, "sudo pacman -S --needed --noconfirm ");
    strcat(cmd, packages);
    execute_command(cmd);
    message_box("Success", "Dependencies installed successfully.");
}

void generate_initrd_arch() {
    progress_dialog("Generating Initramfs And Copying Vmlinuz (Arch)...");
    execute_command("cd /home/$USER/.config/cmi/build-image-arch >/dev/null 2>&1 && sudo mkinitcpio -c live.conf -g /home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img");
    execute_command("sudo cp /boot/vmlinuz-linux* /home/$USER/.config/cmi/build-image-arch/live/ 2>/dev/null");
    message_box("Success", "Initramfs And Vmlinuz generated successfully.");
}

void edit_grub_cfg_arch() {
    progress_dialog("Opening grub.cfg (arch)...");
    execute_command("nano /home/$USER/.config/cmi/build-image-arch/boot/grub/grub.cfg");
    message_box("Success", "grub.cfg opened for editing.");
}

void edit_isolinux_cfg_arch() {
    progress_dialog("Opening isolinux.cfg (arch)...");
    execute_command("nano /home/$USER/.config/cmi/build-image-arch/isolinux/isolinux.cfg");
    message_box("Success", "isolinux.cfg opened for editing.");
}

void install_calamares_arch() {
    progress_dialog("Installing Calamares for Arch Linux...");
    execute_command("cd /home/$USER/.config/cmi/calamares-per-distro/arch >/dev/null 2>&1 && sudo pacman -U calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar ckbcomp-1.227-2-any.pkg.tar.zst");
    message_box("Success", "Calamares installed successfully for Arch Linux.");
}

void edit_calamares_branding() {
    progress_dialog("Opening Calamares branding configuration...");
    execute_command("sudo nano /usr/share/calamares/branding/claudemods/branding.desc");
    message_box("Success", "Calamares branding configuration opened for editing.");
}

void install_dependencies_cachyos() {
    progress_dialog("Installing dependencies...");
    const char *packages =
    "arch-install-scripts "
    "bash-completion "
    "dosfstools "
    "erofs-utils "
    "findutils "
    "grub "
    "jq "
    "libarchive "
    "libisoburn "
    "lsb-release "
    "lvm2 "
    "mkinitcpio-archiso "
    "mkinitcpio-nfs-utils "
    "mtools "
    "nbd "
    "pacman-contrib "
    "parted "
    "procps-ng "
    "pv "
    "python "
    "rsync "
    "squashfs-tools "
    "sshfs "
    "syslinux "
    "xdg-utils "
    "bash-completion "
    "zsh-completions "
    "kernel-modules-hook "
    "virt-manager ";

    char cmd[1024];
    strcpy(cmd, "sudo pacman -S --needed --noconfirm ");
    strcat(cmd, packages);
    execute_command(cmd);
    message_box("Success", "Dependencies installed successfully.");
}

void install_calamares_cachyos() {
    progress_dialog("Installing Calamares for CachyOS...");
    execute_command("sudo pacman -U --noconfirm calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar ckbcomp-1.227-2-any.pkg.tar.zst");
    message_box("Success", "Calamares installed successfully for CachyOS.");
}

void clone_system(const char *clone_dir) {
    char full_clone_path[MAX_PATH];
    strcpy(full_clone_path, clone_dir);
    if (full_clone_path[strlen(full_clone_path)-1] != '/') {
        strcat(full_clone_path, "/");
    }
    strcat(full_clone_path, "clone_system_temp");

    if (!dir_exists(full_clone_path)) {
        char mkdir_cmd[MAX_PATH];
        strcpy(mkdir_cmd, "mkdir -p ");
        strcat(mkdir_cmd, full_clone_path);
        execute_command(mkdir_cmd);
    }

    char parent_dir[MAX_PATH];
    strcpy(parent_dir, clone_dir);
    if (parent_dir[strlen(parent_dir)-1] == '/') {
        parent_dir[strlen(parent_dir)-1] = '\0';
    }
    char *last_slash = strrchr(parent_dir, '/');
    if (last_slash != NULL) {
        strcpy(parent_dir, last_slash + 1);
    }

    char command[MAX_CMD];
    strcpy(command, "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
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
    "--include=usr "
    "--include=etc "
    "/ ");
    strcat(command, full_clone_path);

    printf("%sCloning system into directory: %s%s\n", GREEN, full_clone_path, RESET);
    execute_command(command);
}

void create_squashfs_image(enum Distro distro) {
    char *clone_dir = read_clone_dir();
    if (clone_dir == NULL || strlen(clone_dir) == 0) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }

    char full_clone_path[MAX_PATH];
    strcpy(full_clone_path, clone_dir);
    if (full_clone_path[strlen(full_clone_path)-1] != '/') {
        strcat(full_clone_path, "/");
    }
    strcat(full_clone_path, "clone_system_temp");

    char output_path[MAX_PATH];
    const char *user = getenv("USER");
    if (distro == ARCH || distro == CACHYOS) {
        strcpy(output_path, "/home/");
        strcat(output_path, user);
        strcat(output_path, "/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs");
    }

    char command[MAX_CMD];
    strcpy(command, "sudo mksquashfs ");
    strcat(command, full_clone_path);
    strcat(command, " ");
    strcat(command, output_path);
    strcat(command, " -comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs");

    printf("%sCreating SquashFS image from: %s%s\n", GREEN, full_clone_path, RESET);
    execute_command(command);

    char del_cmd[MAX_PATH];
    strcpy(del_cmd, "sudo rm -rf ");
    strcat(del_cmd, full_clone_path);
    execute_command(del_cmd);
    free(clone_dir);
}

void delete_clone_system_temp(enum Distro distro) {
    char *clone_dir = read_clone_dir();
    if (clone_dir == NULL || strlen(clone_dir) == 0) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }

    char full_clone_path[MAX_PATH];
    strcpy(full_clone_path, clone_dir);
    if (full_clone_path[strlen(full_clone_path)-1] != '/') {
        strcat(full_clone_path, "/");
    }
    strcat(full_clone_path, "clone_system_temp");

    char command[MAX_PATH];
    strcpy(command, "sudo rm -rf ");
    strcat(command, full_clone_path);
    printf("%sDeleting clone directory: %s%s\n", GREEN, full_clone_path, RESET);
    execute_command(command);

    char squashfs_path[MAX_PATH];
    const char *user = getenv("USER");
    if (distro == ARCH || distro == CACHYOS) {
        strcpy(squashfs_path, "/home/");
        strcat(squashfs_path, user);
        strcat(squashfs_path, "/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs");
    }

    struct stat st;
    if (stat(squashfs_path, &st) == 0) {
        strcpy(command, "sudo rm -f ");
        strcat(command, squashfs_path);
        printf("%sDeleting SquashFS image: %s%s\n", GREEN, squashfs_path, RESET);
        execute_command(command);
    } else {
        printf("%sSquashFS image does not exist: %s%s\n", GREEN, squashfs_path, RESET);
    }
    free(clone_dir);
}

void set_clone_directory() {
    char *dir_path = prompt("Enter full path for clone directory e.g /home/$USER/Pictures ");
    if (dir_path == NULL || strlen(dir_path) == 0) {
        error_box("Error", "Directory path cannot be empty");
        free(dir_path);
        return;
    }

    if (dir_path[strlen(dir_path)-1] != '/') {
        char *new_path = realloc(dir_path, strlen(dir_path) + 2);
        if (!new_path) {
            free(dir_path);
            error_box("Error", "Memory allocation failed");
            return;
        }
        strcat(new_path, "/");
        dir_path = new_path;
    }

    save_clone_dir(dir_path);
    free(dir_path);
}

void install_one_time_updater() {
    progress_dialog("Installing one-time updater...");
    execute_command("./home/$USER/.config/cmi/patch.sh");
    message_box("Success", "One-time updater installed successfully in /home/$USER/.config/cmi");
}

void squashfs_menu(enum Distro distro) {
    const char *items[] = {
        "Max compression (xz)",
        "Create SquashFS from clone directory",
        "Delete clone directory and SquashFS image",
        "Back to Main Menu"
    };
    int item_count = sizeof(items) / sizeof(items[0]);
    int selected = 0;
    int key;
    while (1) {
        key = show_menu("SquashFS Creator", items, item_count, selected, distro);
        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < item_count - 1) selected++;
                break;
            case '\n': // Enter key
                switch (selected) {
                    case 0:
                    {
                        char *clone_dir = read_clone_dir();
                        if (clone_dir == NULL || strlen(clone_dir) == 0) {
                            error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
                            free(clone_dir);
                            break;
                        }
                        if (!dir_exists(strcat(strdup(clone_dir), "clone_system_temp"))) {
                            clone_system(clone_dir);
                        }
                        create_squashfs_image(distro);
                        free(clone_dir);
                    }
                    break;
                    case 1:
                        create_squashfs_image(distro);
                        break;
                    case 2:
                        delete_clone_system_temp(distro);
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

void create_iso(enum Distro distro) {
    char *iso_name = get_iso_name();
    if (iso_name == NULL || strlen(iso_name) == 0) {
        error_box("Error", "ISO name not set. Please set it in Setup Script menu (Option 3)");
        free(iso_name);
        return;
    }

    char *output_dir = prompt("Enter the output directory path: ");
    if (output_dir == NULL || strlen(output_dir) == 0) {
        error_box("Input Error", "Output directory cannot be empty.");
        free(iso_name);
        free(output_dir);
        return;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H%M", t);

    char iso_file_name[MAX_PATH];
    strcpy(iso_file_name, output_dir);
    strcat(iso_file_name, "/");
    strcat(iso_file_name, iso_name);
    strcat(iso_file_name, "_amd64_");
    strcat(iso_file_name, timestamp);
    strcat(iso_file_name, ".iso");

    char build_image_dir[MAX_PATH];
    const char *user = getenv("USER");
    if (distro == ARCH || distro == CACHYOS) {
        strcpy(build_image_dir, "/home/");
        strcat(build_image_dir, user);
        strcat(build_image_dir, "/.config/cmi/build-image-arch");
    }

    if (!dir_exists(output_dir)) {
        char mkdir_cmd[MAX_PATH];
        strcpy(mkdir_cmd, "mkdir -p ");
        strcat(mkdir_cmd, output_dir);
        execute_command(mkdir_cmd);
    }

    char xorriso_cmd[MAX_CMD];
    strcpy(xorriso_cmd, "sudo xorriso -as mkisofs -o ");
    strcat(xorriso_cmd, iso_file_name);
    strcat(xorriso_cmd, " -V 2025 -iso-level 3");

    if (distro == ARCH || distro == CACHYOS) {
        strcat(xorriso_cmd, " -isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin"
        " -c isolinux/boot.cat"
        " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
        " -eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat");
    }

    strcat(xorriso_cmd, " ");
    strcat(xorriso_cmd, build_image_dir);

    execute_command(xorriso_cmd);
    message_box("Success", "ISO creation completed.");

    char *choice = prompt("Press 'm' to go back to main menu or Enter to exit: ");
    if (choice != NULL && (choice[0] == 'm' || choice[0] == 'M')) {
        execute_command("ruby /opt/claudemods-iso-konsole-script/demo.rb");
    }
    free(iso_name);
    free(output_dir);
    free(choice);
}

void run_iso_in_qemu() {
    execute_command("ruby /opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb");
}

void iso_creator_menu(enum Distro distro) {
    const char *items[] = {
        "Create ISO",
        "Run ISO in QEMU",
        "Back to Main Menu"
    };
    int item_count = sizeof(items) / sizeof(items[0]);
    int selected = 0;
    int key;
    while (1) {
        key = show_menu("ISO Creator Menu", items, item_count, selected, distro);
        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < item_count - 1) selected++;
                break;
            case '\n': // Enter key
                switch (selected) {
                    case 0:
                        create_iso(distro);
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

    char command[MAX_CMD];
    char sudo_cmd[MAX_CMD + 10];

    // gen-init
    strcpy(command, "bash -c 'cat > /usr/bin/gen-init << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: gen-init\"\n"
    "  echo \"Generate initcpio configuration\"\n"
    "  exit 0\n"
    "fi\n"
    "exec ");
    strcat(command, exe_path);
    strcat(command, " 5\n"
    "EOF\n"
    "chmod 755 /usr/bin/gen-init'");
    strcpy(sudo_cmd, "sudo ");
    strcat(sudo_cmd, command);
    execute_command(sudo_cmd);

    // edit-isocfg
    strcpy(command, "bash -c 'cat > /usr/bin/edit-isocfg << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-isocfg\"\n"
    "  echo \"Edit isolinux.cfg file\"\n"
    "  exit 0\n"
    "fi\n"
    "exec ");
    strcat(command, exe_path);
    strcat(command, " 6\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-isocfg'");
    strcpy(sudo_cmd, "sudo ");
    strcat(sudo_cmd, command);
    execute_command(sudo_cmd);

    // edit-grubcfg
    strcpy(command, "bash -c 'cat > /usr/bin/edit-grubcfg << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-grubcfg\"\n"
    "  echo \"Edit grub.cfg file\"\n"
    "  exit 0\n"
    "fi\n"
    "exec ");
    strcat(command, exe_path);
    strcat(command, " 7\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-grubcfg'");
    strcpy(sudo_cmd, "sudo ");
    strcat(sudo_cmd, command);
    execute_command(sudo_cmd);

    // setup-script
    strcpy(command, "bash -c 'cat > /usr/bin/setup-script << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: setup-script\"\n"
    "  echo \"Open setup script menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec ");
    strcat(command, exe_path);
    strcat(command, " 8\n"
    "EOF\n"
    "chmod 755 /usr/bin/setup-script'");
    strcpy(sudo_cmd, "sudo ");
    strcat(sudo_cmd, command);
    execute_command(sudo_cmd);

    // make-iso
    strcpy(command, "bash -c 'cat > /usr/bin/make-iso << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: make-iso\"\n"
    "  echo \"Launches the ISO creation menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec ");
    strcat(command, exe_path);
    strcat(command, " 3\n"
    "EOF\n"
    "chmod 755 /usr/bin/make-iso'");
    strcpy(sudo_cmd, "sudo ");
    strcat(sudo_cmd, command);
    execute_command(sudo_cmd);

    // make-squashfs
    strcpy(command, "bash -c 'cat > /usr/bin/make-squashfs << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: make-squashfs\"\n"
    "  echo \"Launches the SquashFS creation menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec ");
    strcat(command, exe_path);
    strcat(command, " 4\n"
    "EOF\n"
    "chmod 755 /usr/bin/make-squashfs'");
    strcpy(sudo_cmd, "sudo ");
    strcat(sudo_cmd, command);
    execute_command(sudo_cmd);

    // gen-calamares
    strcpy(command, "bash -c 'cat > /usr/bin/gen-calamares << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: gen-calamares\"\n"
    "  echo \"Install Calamares installer\"\n"
    "  exit 0\n"
    "fi\n"
    "exec ");
    strcat(command, exe_path);
    strcat(command, " 9\n"
    "EOF\n"
    "chmod 755 /usr/bin/gen-calamares'");
    strcpy(sudo_cmd, "sudo ");
    strcat(sudo_cmd, command);
    execute_command(sudo_cmd);

    // edit-branding
    strcpy(command, "bash -c 'cat > /usr/bin/edit-branding << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-branding\"\n"
    "  echo \"Edit Calamares branding\"\n"
    "  exit 0\n"
    "fi\n"
    "exec ");
    strcat(command, exe_path);
    strcat(command, " 10\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-branding'");
    strcpy(sudo_cmd, "sudo ");
    strcat(sudo_cmd, command);
    execute_command(sudo_cmd);

    printf("%sActivated! You can now use all commands in your terminal.%s\n", GREEN, RESET);
}

void remove_command_files() {
    execute_command("sudo rm -f /usr/bin/gen-init");
    execute_command("sudo rm -f /usr/bin/edit-isocfg");
    execute_command("sudo rm -f /usr/bin/edit-grubcfg");
    execute_command("sudo rm -f /usr/bin/setup-script");
    execute_command("sudo rm -f /usr/bin/make-iso");
    execute_command("sudo rm -f /usr/bin/make-squashfs");
    execute_command("sudo rm -f /usr/bin/gen-calamares");
    execute_command("sudo rm -f /usr/bin/edit-branding");
    printf("%sCommands deactivated and removed from system.%s\n", GREEN, RESET);
}

void command_installer_menu(enum Distro distro) {
    const char *items[] = {
        "Activate terminal commands",
        "Deactivate terminal commands",
        "Back to Main Menu"
    };
    int item_count = sizeof(items) / sizeof(items[0]);
    int selected = 0;
    int key;
    while (1) {
        key = show_menu("Command Installer Menu", items, item_count, selected, distro);
        switch (key) {
            case 'A': // Up arrow
                if (selected > 0) selected--;
                break;
            case 'B': // Down arrow
                if (selected < item_count - 1) selected++;
                break;
            case '\n': // Enter key
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

void setup_script_menu(enum Distro distro) {
    const char **items;
    int item_count;

    switch(distro) {
        case ARCH:
        case CACHYOS:
        {
            const char *arch_items[] = {
                "Install Dependencies (Arch)",
                "Generate initcpio configuration (arch)",
                "Set ISO Name",
                "Edit isolinux.cfg (arch)",
                "Edit grub.cfg (arch)",
                "Set clone directory path",
                "Install Calamares",
                "Edit Calamares Branding",
                "Install One Time Updater",
                "Back to Main Menu"
            };
            items = arch_items;
            item_count = sizeof(arch_items) / sizeof(arch_items[0]);
        }
        break;
        default:
            error_box("Error", "Unsupported distribution");
            return;
    }

    int selected = 0;
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (1) {
        system("clear");
        print_banner(distro);

        printf("%s  Setup Script Menu%s\n", COLOR_CYAN, RESET);
        printf("%s  -----------------%s\n", COLOR_CYAN, RESET);

        for (int i = 0; i < item_count; i++) {
            if (i == selected) {
                printf("%s➤ %s%s\n", get_highlight_color(distro), items[i], RESET);
            } else {
                printf("%s  %s%s\n", COLOR_CYAN, items[i], RESET);
            }
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int retval = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

        if (retval == -1) {
            perror("select()");
        } else if (retval) {
            int c = getchar();
            if (c == '\033') {
                getchar();
                c = getchar();

                if (c == 'A' && selected > 0) selected--;
                else if (c == 'B' && selected < item_count - 1) selected++;
            }
            else if (c == '\n') {
                switch(selected) {
                    case 0:
                        if (distro == ARCH) install_dependencies_arch();
                        else if (distro == CACHYOS) install_dependencies_cachyos();
                        break;
                    case 1:
                        if (distro == ARCH || distro == CACHYOS) generate_initrd_arch();
                        break;
                    case 2:
                        set_iso_name();
                        break;
                    case 3:
                        if (distro == ARCH || distro == CACHYOS) edit_isolinux_cfg_arch();
                        break;
                    case 4:
                        if (distro == ARCH || distro == CACHYOS) edit_grub_cfg_arch();
                        break;
                    case 5:
                        set_clone_directory();
                        break;
                    case 6:
                        if (distro == ARCH) install_calamares_arch();
                        else if (distro == CACHYOS) install_calamares_cachyos();
                        break;
                    case 7:
                        edit_calamares_branding();
                        break;
                    case 8:
                        install_one_time_updater();
                        break;
                    case 9:
                        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                        return;
                }
                printf("\nPress Enter to continue...");
                while (getchar() != '\n');
            }
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int main(int argc, char* argv[]) {
    tcgetattr(STDIN_FILENO, &original_term);
    pthread_t time_thread;
    pthread_create(&time_thread, NULL, update_time_thread, NULL);

    if (argc > 1) {
        int option = atoi(argv[1]);
        enum Distro distro = UNKNOWN;

        switch(option) {
            case 5:
                distro = detect_distro();
                if (distro == ARCH || distro == CACHYOS) generate_initrd_arch();
                break;
            case 6:
                distro = detect_distro();
                if (distro == ARCH || distro == CACHYOS) edit_isolinux_cfg_arch();
                break;
            case 7:
                distro = detect_distro();
                if (distro == ARCH || distro == CACHYOS) edit_grub_cfg_arch();
                break;
            case 8:
                setup_script_menu(detect_distro());
                break;
            case 3:
                iso_creator_menu(detect_distro());
                break;
            case 4:
                squashfs_menu(detect_distro());
                break;
            case 9:
                distro = detect_distro();
                if (distro == ARCH) install_calamares_arch();
                else if (distro == CACHYOS) install_calamares_cachyos();
                break;
            case 10:
                edit_calamares_branding();
                break;
            default:
                printf("Invalid option\n");
        }

        time_thread_running = false;
        pthread_join(time_thread, NULL);
        return 0;
    }

    enum Distro distro = detect_distro();
    const char *items[] = {
        "Guide",
        "Setup Script",
        "SquashFS Creator",
        "ISO Creator",
        "Command Installer",
        "Changelog",
        "Exit"
    };
    int item_count = sizeof(items) / sizeof(items[0]);
    int selected = 0;
    int key;
    while (1) {
        if (should_reset) {
            should_reset = 0;
            selected = 0;
            system("clear");
            continue;
        }

        key = show_menu("Main Menu", items, item_count, selected, distro);
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
                        execute_command("nano /home/$USER/.config/cmi/readme.txt");
                        break;
                    case 1:
                        setup_script_menu(distro);
                        break;
                    case 2:
                        squashfs_menu(distro);
                        break;
                    case 3:
                        iso_creator_menu(distro);
                        break;
                    case 4:
                        command_installer_menu(distro);
                        break;
                    case 5:
                        execute_command("nano /home/$USER/.config/cmi/changes.txt");
                        break;
                    case 6:
                        time_thread_running = false;
                        pthread_join(time_thread, NULL);
                        disable_raw_mode();
                        return 0;
                }
                break;
        }
    }
}
