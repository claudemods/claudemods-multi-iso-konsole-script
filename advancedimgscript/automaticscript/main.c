#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <dirent.h>
#include <pwd.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>

// Forward declarations
void saveConfig();
void execute_command(const char* cmd, bool continueOnError);
void printCheckbox(bool checked);
char* getUserInput(const char* prompt);
void clearScreen();
int getch();
int kbhit();
void update_time_thread();
void printBanner();
void printConfigStatus();
void installDependencies();
void selectVmlinuz();
void generateMkinitcpio();
void editGrubCfg();
void setIsoTag();
void setIsoName();
void setOutputDir();
void setCloneDir();
char* getConfigFilePath();
void loadConfig();
bool validateSizeInput(const char* input);
bool copyFilesWithRsync(const char* source, const char* destination);
bool createSquashFS(const char* inputDir, const char* outputFile);
bool createChecksum(const char* filename);
void printFinalMessage(const char* outputFile);
char* getOutputDirectory();
char* expandPath(const char* path);
bool createISO();
void autoSetup();

// Time-related globals
pthread_t time_thread;
bool time_thread_running = true;
pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;
char current_time_str[50];
bool should_reset = false;

// Constants
const char* ORIG_IMG_NAME = "rootfs1.img";
const char* FINAL_IMG_NAME = "rootfs.img";
char* MOUNT_POINT = "/mnt/ext4_temp";
const char* SOURCE_DIR = "/";
const char* COMPRESSION_LEVEL = "22";
const char* SQUASHFS_COMPRESSION = "zstd";
const char* SQUASHFS_COMPRESSION_ARGS[] = {"-Xcompression-level", "22"};
char BUILD_DIR[256] = "/home/$USER/.config/cmi/build-image-arch-img";
char USERNAME[64] = "";

// Dependencies list
const char* DEPENDENCIES[] = {
    "rsync", "squashfs-tools", "xorriso", "grub", "dosfstools",
    "unzip", "nano", "arch-install-scripts", "bash-completion",
    "erofs-utils", "findutils", "unzip", "jq", "libarchive",
    "libisoburn", "lsb-release", "lvm2", "mkinitcpio-archiso",
    "mkinitcpio-nfs-utils", "mtools", "nbd", "pacman-contrib",
    "parted", "procps-ng", "pv", "python", "sshfs", "syslinux",
    "xdg-utils", "zsh-completions", "kernel-modules-hook",
    "virt-manager", "qt6-tools", "qt5-tools"
};
const int DEPENDENCIES_COUNT = 34;

// Configuration state
typedef struct {
    char isoTag[256];
    char isoName[256];
    char outputDir[256];
    char vmlinuzPath[256];
    char cloneDir[256];
    bool mkinitcpioGenerated;
    bool grubEdited;
    bool dependenciesInstalled;
} ConfigState;

ConfigState config;

// ANSI color codes
const char* COLOR_RED = "\033[31m";
const char* COLOR_GREEN = "\033[32m";
const char* COLOR_BLUE = "\033[34m";
const char* COLOR_CYAN = "\033[38;2;0;255;255m";
const char* COLOR_YELLOW = "\033[33m";
const char* COLOR_RESET = "\033[0m";
const char* COLOR_HIGHLIGHT = "\033[38;2;0;255;255m";
const char* COLOR_NORMAL = "\033[34m";

void* update_time_thread(void* arg) {
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

void clearScreen() {
    printf("\033[2J\033[1;1H");
}

int getch() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON);
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

void execute_command(const char* cmd, bool continueOnError) {
    printf("%s", COLOR_CYAN);
    fflush(stdout);
    int status = system(cmd);
    printf("%s", COLOR_RESET);
    if (status != 0 && !continueOnError) {
        fprintf(stderr, "%sError executing: %s%s\n", COLOR_RED, cmd, COLOR_RESET);
        exit(1);
    } else if (status != 0) {
        fprintf(stderr, "%sCommand failed but continuing: %s%s\n", COLOR_YELLOW, cmd, COLOR_RESET);
    }
}

void printCheckbox(bool checked) {
    if (checked) {
        printf("%s[✓]%s", COLOR_GREEN, COLOR_RESET);
    } else {
        printf("%s[ ]%s", COLOR_RED, COLOR_RESET);
    }
}

void printBanner() {
    clearScreen();

    printf("%s\n", COLOR_RED);
    printf("░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n");
    printf("██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n");
    printf("██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n");
    printf("██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n");
    printf("╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n");
    printf("░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚══════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░\n");
    printf("%s\n", COLOR_RESET);
    printf("%s Advanced C++ Arch Img Iso Script Beta v2.01 26-07-2025%s\n", COLOR_CYAN, COLOR_RESET);

    pthread_mutex_lock(&time_mutex);
    printf("%sCurrent UK Time: %s%s%s\n", COLOR_BLUE, COLOR_CYAN, current_time_str, COLOR_RESET);
    pthread_mutex_unlock(&time_mutex);

    printf("%sFilesystem      Size  Used Avail Use%% Mounted on%s\n", COLOR_GREEN, COLOR_RESET);
    execute_command("df -h / | tail -1", false);
    printf("\n");
}

void printConfigStatus() {
    printf("%sCurrent Configuration:%s\n", COLOR_CYAN, COLOR_RESET);

    printf(" ");
    printCheckbox(config.dependenciesInstalled);
    printf(" Dependencies Installed\n");

    printf(" ");
    printCheckbox(strlen(config.isoTag) > 0);
    printf(" ISO Tag: %s%s%s\n", (strlen(config.isoTag) == 0 ? COLOR_YELLOW : COLOR_CYAN), 
           (strlen(config.isoTag) == 0 ? "Not set" : config.isoTag), COLOR_RESET);

    printf(" ");
    printCheckbox(strlen(config.isoName) > 0);
    printf(" ISO Name: %s%s%s\n", (strlen(config.isoName) == 0 ? COLOR_YELLOW : COLOR_CYAN), 
           (strlen(config.isoName) == 0 ? "Not set" : config.isoName), COLOR_RESET);

    printf(" ");
    printCheckbox(strlen(config.outputDir) > 0);
    printf(" Output Directory: %s%s%s\n", (strlen(config.outputDir) == 0 ? COLOR_YELLOW : COLOR_CYAN), 
           (strlen(config.outputDir) == 0 ? "Not set" : config.outputDir), COLOR_RESET);

    printf(" ");
    printCheckbox(strlen(config.vmlinuzPath) > 0);
    printf(" vmlinuz Selected: %s%s%s\n", (strlen(config.vmlinuzPath) == 0 ? COLOR_YELLOW : COLOR_CYAN), 
           (strlen(config.vmlinuzPath) == 0 ? "Not selected" : config.vmlinuzPath), COLOR_RESET);

    printf(" ");
    printCheckbox(strlen(config.cloneDir) > 0);
    printf(" Clone Directory: %s%s%s\n", (strlen(config.cloneDir) == 0 ? COLOR_YELLOW : COLOR_CYAN), 
           (strlen(config.cloneDir) == 0 ? "Not set" : config.cloneDir), COLOR_RESET);

    printf(" ");
    printCheckbox(config.mkinitcpioGenerated);
    printf(" mkinitcpio Generated\n");

    printf(" ");
    printCheckbox(config.grubEdited);
    printf(" GRUB Config Edited\n");
}

char* getUserInput(const char* prompt) {
    printf("%s%s%s", COLOR_GREEN, prompt, COLOR_RESET);
    fflush(stdout);
    
    static char input[1024];
    int pos = 0;
    char ch;
    struct termios oldt, newt;

    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // Disable canonical mode and echo
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (1) {
        ch = getchar();
        if (ch == '\n') {  // Enter key pressed
            break;
        } else if (ch == 127 || ch == 8) {  // Backspace key pressed
            if (pos > 0) {
                pos--;
                printf("\b \b");  // Move cursor back, overwrite with space, move back again
            }
        } else if (ch >= 32 && ch <= 126) {  // Printable characters
            if (pos < 1023) {
                input[pos++] = ch;
                printf("%c", ch);
            }
        }
    }
    input[pos] = '\0';

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n");
    return input;
}

void installDependencies() {
    printf("%s\nInstalling required dependencies...\n%s", COLOR_CYAN, COLOR_RESET);

    char packages[4096] = "";
    for (int i = 0; i < DEPENDENCIES_COUNT; i++) {
        strcat(packages, DEPENDENCIES[i]);
        strcat(packages, " ");
    }

    char command[5120];
    snprintf(command, sizeof(command), "sudo pacman -Sy --needed --noconfirm %s", packages);
    execute_command(command, false);

    config.dependenciesInstalled = true;
    saveConfig();
    printf("%s\nDependencies installed successfully!\n%s", COLOR_GREEN, COLOR_RESET);
}

void selectVmlinuz() {
    DIR *dir;
    struct dirent *ent;
    char vmlinuzFiles[20][256];
    int file_count = 0;

    if ((dir = opendir("/boot")) != NULL) {
        while ((ent = readdir(dir)) != NULL && file_count < 20) {
            char* filename = ent->d_name;
            if (strstr(filename, "vmlinuz") == filename) {
                snprintf(vmlinuzFiles[file_count], sizeof(vmlinuzFiles[file_count]), "/boot/%s", filename);
                file_count++;
            }
        }
        closedir(dir);
    } else {
        fprintf(stderr, "%sCould not open /boot directory%s\n", COLOR_RED, COLOR_RESET);
        return;
    }

    if (file_count == 0) {
        fprintf(stderr, "%sNo vmlinuz files found in /boot!%s\n", COLOR_RED, COLOR_RESET);
        return;
    }

    printf("%sAvailable vmlinuz files:%s\n", COLOR_GREEN, COLOR_RESET);
    for (int i = 0; i < file_count; i++) {
        printf("%s%d) %s%s\n", COLOR_GREEN, i+1, vmlinuzFiles[i], COLOR_RESET);
    }

    char selection[256];
    snprintf(selection, sizeof(selection), "Select vmlinuz file (1-%d): ", file_count);
    char* input = getUserInput(selection);
    
    int choice = atoi(input);
    if (choice > 0 && choice <= file_count) {
        strcpy(config.vmlinuzPath, vmlinuzFiles[choice-1]);

        char destPath[512];
        snprintf(destPath, sizeof(destPath), "%s/boot/vmlinuz-x86_64", BUILD_DIR);
        
        char copyCmd[1024];
        snprintf(copyCmd, sizeof(copyCmd), "sudo cp %s %s", config.vmlinuzPath, destPath);
        execute_command(copyCmd, false);

        printf("%sSelected: %s%s\n", COLOR_CYAN, config.vmlinuzPath, COLOR_RESET);
        printf("%sCopied to: %s%s\n", COLOR_CYAN, destPath, COLOR_RESET);
        saveConfig();
    } else {
        fprintf(stderr, "%sInvalid selection!%s\n", COLOR_RED, COLOR_RESET);
    }
}

void generateMkinitcpio() {
    if (strlen(config.vmlinuzPath) == 0) {
        fprintf(stderr, "%sPlease select vmlinuz first!%s\n", COLOR_RED, COLOR_RESET);
        return;
    }

    if (strlen(BUILD_DIR) == 0) {
        fprintf(stderr, "%sBuild directory not set!%s\n", COLOR_RED, COLOR_RESET);
        return;
    }

    printf("%sGenerating initramfs...%s\n", COLOR_CYAN, COLOR_RESET);
    
    char command[1024];
    snprintf(command, sizeof(command), "cd %s && sudo mkinitcpio -c mkinitcpio.conf -g %s/boot/initramfs-x86_64.img", 
             BUILD_DIR, BUILD_DIR);
    execute_command(command, false);

    config.mkinitcpioGenerated = true;
    saveConfig();
    printf("%smkinitcpio generated successfully!%s\n", COLOR_GREEN, COLOR_RESET);
}

void editGrubCfg() {
    if (strlen(BUILD_DIR) == 0) {
        fprintf(stderr, "%sBuild directory not set!%s\n", COLOR_RED, COLOR_RESET);
        return;
    }

    char grubCfgPath[512];
    snprintf(grubCfgPath, sizeof(grubCfgPath), "%s/boot/grub/grub.cfg", BUILD_DIR);
    printf("%sEditing GRUB config: %s%s\n", COLOR_CYAN, grubCfgPath, COLOR_RESET);

    // Set nano to use cyan color scheme
    char nanoCommand[1024];
    snprintf(nanoCommand, sizeof(nanoCommand), "sudo env TERM=xterm-256color nano -Y cyanish %s", grubCfgPath);
    execute_command(nanoCommand, false);

    config.grubEdited = true;
    saveConfig();
    printf("%sGRUB config edited!%s\n", COLOR_GREEN, COLOR_RESET);
}

void setIsoTag() {
    char* input = getUserInput("Enter ISO tag (e.g., 2025): ");
    strcpy(config.isoTag, input);
    saveConfig();
}

void setIsoName() {
    char* input = getUserInput("Enter ISO name (e.g., claudemods.iso): ");
    strcpy(config.isoName, input);
    saveConfig();
}

void setOutputDir() {
    char defaultDir[256];
    snprintf(defaultDir, sizeof(defaultDir), "/home/%s/Downloads", USERNAME);
    
    printf("%sCurrent output directory: %s%s%s\n", COLOR_GREEN, 
           (strlen(config.outputDir) == 0 ? COLOR_YELLOW : COLOR_CYAN), 
           (strlen(config.outputDir) == 0 ? "Not set" : config.outputDir), COLOR_RESET);
    printf("%sDefault directory: %s%s%s\n", COLOR_GREEN, COLOR_CYAN, defaultDir, COLOR_RESET);
    
    char prompt[512];
    snprintf(prompt, sizeof(prompt), "Enter output directory (e.g., %s or $USER/Downloads): ", defaultDir);
    char* input = getUserInput(prompt);
    strcpy(config.outputDir, input);

    char* user_pos = strstr(config.outputDir, "$USER");
    if (user_pos != NULL) {
        memmove(user_pos, USERNAME, strlen(USERNAME));
        user_pos[strlen(USERNAME)] = '\0';
    }

    if (strlen(config.outputDir) == 0) {
        strcpy(config.outputDir, defaultDir);
    }

    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", config.outputDir);
    execute_command(mkdir_cmd, true);

    saveConfig();
}

void setCloneDir() {
    char defaultDir[256];
    snprintf(defaultDir, sizeof(defaultDir), "/home/%s", USERNAME);
    
    printf("%sCurrent clone directory: %s%s%s\n", COLOR_GREEN, 
           (strlen(config.cloneDir) == 0 ? COLOR_YELLOW : COLOR_CYAN), 
           (strlen(config.cloneDir) == 0 ? "Not set" : config.cloneDir), COLOR_RESET);
    printf("%sDefault directory: %s%s%s\n", COLOR_GREEN, COLOR_CYAN, defaultDir, COLOR_RESET);

    // Get the parent directory from user
    char prompt[512];
    snprintf(prompt, sizeof(prompt), "Enter parent directory for clone_system_temp folder (e.g., %s or $USER): ", defaultDir);
    char* input = getUserInput(prompt);
    char parentDir[256];
    strcpy(parentDir, input);

    char* user_pos = strstr(parentDir, "$USER");
    if (user_pos != NULL) {
        memmove(user_pos, USERNAME, strlen(USERNAME));
        user_pos[strlen(USERNAME)] = '\0';
    }

    if (strlen(parentDir) == 0) {
        strcpy(parentDir, defaultDir);
    }

    // Always use clone_system_temp as the folder name
    snprintf(config.cloneDir, sizeof(config.cloneDir), "%s/clone_system_temp", parentDir);

    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "sudo mkdir -p %s", config.cloneDir);
    execute_command(mkdir_cmd, true);

    saveConfig();
}

char* getConfigFilePath() {
    static char path[256];
    snprintf(path, sizeof(path), "/home/%s/.config/cmi/configuration.txt", USERNAME);
    return path;
}

void saveConfig() {
    char* configPath = getConfigFilePath();
    FILE* configFile = fopen(configPath, "w");
    if (configFile != NULL) {
        fprintf(configFile, "isoTag=%s\n", config.isoTag);
        fprintf(configFile, "isoName=%s\n", config.isoName);
        fprintf(configFile, "outputDir=%s\n", config.outputDir);
        fprintf(configFile, "vmlinuzPath=%s\n", config.vmlinuzPath);
        fprintf(configFile, "cloneDir=%s\n", config.cloneDir);
        fprintf(configFile, "mkinitcpioGenerated=%d\n", config.mkinitcpioGenerated);
        fprintf(configFile, "grubEdited=%d\n", config.grubEdited);
        fprintf(configFile, "dependenciesInstalled=%d\n", config.dependenciesInstalled);
        fclose(configFile);
    } else {
        fprintf(stderr, "%sFailed to save configuration to %s%s\n", COLOR_RED, configPath, COLOR_RESET);
    }
}

void loadConfig() {
    char* configPath = getConfigFilePath();
    FILE* configFile = fopen(configPath, "r");
    if (configFile != NULL) {
        char line[512];
        while (fgets(line, sizeof(line), configFile)) {
            char* delimiter = strchr(line, '=');
            if (delimiter != NULL) {
                *delimiter = '\0';
                char* key = line;
                char* value = delimiter + 1;
                
                // Remove newline from value
                char* newline = strchr(value, '\n');
                if (newline) *newline = '\0';
                
                if (strcmp(key, "isoTag") == 0) strcpy(config.isoTag, value);
                else if (strcmp(key, "isoName") == 0) strcpy(config.isoName, value);
                else if (strcmp(key, "outputDir") == 0) strcpy(config.outputDir, value);
                else if (strcmp(key, "vmlinuzPath") == 0) strcpy(config.vmlinuzPath, value);
                else if (strcmp(key, "cloneDir") == 0) strcpy(config.cloneDir, value);
                else if (strcmp(key, "mkinitcpioGenerated") == 0) config.mkinitcpioGenerated = (atoi(value) == 1);
                else if (strcmp(key, "grubEdited") == 0) config.grubEdited = (atoi(value) == 1);
                else if (strcmp(key, "dependenciesInstalled") == 0) config.dependenciesInstalled = (atoi(value) == 1);
            }
        }
        fclose(configFile);
    }
}

bool validateSizeInput(const char* input) {
    char* endptr;
    double size = strtod(input, &endptr);
    return size > 0.1 && *endptr == '\0';
}

bool copyFilesWithRsync(const char* source, const char* destination) {
    printf("%sCopying files...%s\n", COLOR_CYAN, COLOR_RESET);

    char command[4096];
    snprintf(command, sizeof(command),
        "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
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
        "--include=usr %s/ %s/",
        source, destination);

    execute_command(command, true);
    return true;
}

bool createSquashFS(const char* inputDir, const char* outputFile) {
    printf("%sCreating SquashFS image, this may take some time...%s\n", COLOR_CYAN, COLOR_RESET);

    // Use exact same mksquashfs arguments as in the bash script
    char command[2048];
    snprintf(command, sizeof(command), "sudo mksquashfs %s %s -noappend -comp xz -b 256K -Xbcj x86",
             inputDir, outputFile);

    execute_command(command, true);
    return true;
}

bool createChecksum(const char* filename) {
    char command[1024];
    snprintf(command, sizeof(command), "sha512sum %s > %s.sha512", filename, filename);
    execute_command(command, true);
    return true;
}

void printFinalMessage(const char* outputFile) {
    printf("\n");
    printf("%sSquashFS image created successfully: %s%s\n", COLOR_CYAN, outputFile, COLOR_RESET);
    printf("%sChecksum file: %s.sha512%s\n", COLOR_CYAN, outputFile, COLOR_RESET);
    printf("%sSize: ", COLOR_CYAN);
    char size_cmd[512];
    snprintf(size_cmd, sizeof(size_cmd), "sudo du -h %s | cut -f1", outputFile);
    execute_command(size_cmd, true);
    printf("%s", COLOR_RESET);
}

char* getOutputDirectory() {
    static char dir[512];
    snprintf(dir, sizeof(dir), "/home/%s/.config/cmi/build-image-arch-img/LiveOS", USERNAME);
    return dir;
}

char* expandPath(const char* path) {
    static char result[1024];
    strcpy(result, path);
    
    char* pos;
    if ((pos = strstr(result, "~")) != NULL) {
        const char* home = getenv("HOME");
        if (home) {
            char temp[1024];
            strncpy(temp, result, pos - result);
            temp[pos - result] = '\0';
            snprintf(result, sizeof(result), "%s%s%s", temp, home, pos + 1);
        }
    }
    if ((pos = strstr(result, "$USER")) != NULL) {
        char temp[1024];
        strncpy(temp, result, pos - result);
        temp[pos - result] = '\0';
        snprintf(result, sizeof(result), "%s%s%s", temp, USERNAME, pos + 5);
    }
    return result;
}

bool createISO() {
    // Check if setup is complete
    if (strlen(config.isoTag) == 0 || strlen(config.isoName) == 0 || 
        strlen(config.outputDir) == 0 || strlen(config.vmlinuzPath) == 0 ||
        !config.mkinitcpioGenerated || !config.grubEdited || !config.dependenciesInstalled) {
        fprintf(stderr, "%sCannot create ISO - setup is incomplete!%s\n", COLOR_RED, COLOR_RESET);
        return false;
    }

    printf("%s\nStarting ISO creation process...\n%s", COLOR_CYAN, COLOR_RESET);

    char* expandedOutputDir = expandPath(config.outputDir);

    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", expandedOutputDir);
    execute_command(mkdir_cmd, true);

    char xorrisoCmd[4096];
    snprintf(xorrisoCmd, sizeof(xorrisoCmd),
        "sudo xorriso -as mkisofs "
        "--modification-date=\"$(date +%%Y%%m%%d%%H%%M%%S00)\" "
        "--protective-msdos-label "
        "-volid \"%s\" "
        "-appid \"claudemods Linux Live/Rescue CD\" "
        "-publisher \"claudemods claudemods101@gmail.com >\" "
        "-preparer \"Prepared by user\" "
        "-r -graft-points -no-pad "
        "--sort-weight 0 / "
        "--sort-weight 1 /boot "
        "--grub2-mbr %s/boot/grub/i386-pc/boot_hybrid.img "
        "-partition_offset 16 "
        "-b boot/grub/i386-pc/eltorito.img "
        "-c boot.catalog "
        "-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info "
        "-eltorito-alt-boot "
        "-append_partition 2 0xef %s/boot/efi.img "
        "-e --interval:appended_partition_2:all:: "
        "-no-emul-boot "
        "-iso-level 3 "
        "-o \"%s/%s\" %s",
        config.isoTag, BUILD_DIR, BUILD_DIR, expandedOutputDir, config.isoName, BUILD_DIR);

    execute_command(xorrisoCmd, true);
    printf("%sISO created successfully at %s/%s%s\n", COLOR_CYAN, expandedOutputDir, config.isoName, COLOR_RESET);
    return true;
}

void autoSetup() {
    printBanner();
    printConfigStatus();

    // Install dependencies if not already installed
    if (!config.dependenciesInstalled) {
        printf("%s\nInstalling dependencies...%s\n", COLOR_CYAN, COLOR_RESET);
        installDependencies();
    }

    // Set ISO tag if not set
    if (strlen(config.isoTag) == 0) {
        printf("%s\nSetting ISO tag...%s\n", COLOR_CYAN, COLOR_RESET);
        // Get current date for default ISO tag
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char dateStr[20];
        strftime(dateStr, sizeof(dateStr), "%Y%m%d", t);
        snprintf(config.isoTag, sizeof(config.isoTag), "CMI_%s", dateStr);
        saveConfig();
    }

    // Set ISO name if not set
    if (strlen(config.isoName) == 0) {
        printf("%s\nSetting ISO name...%s\n", COLOR_CYAN, COLOR_RESET);
        // Get current date for default ISO name
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char dateStr[20];
        strftime(dateStr, sizeof(dateStr), "%Y%m%d", t);
        snprintf(config.isoName, sizeof(config.isoName), "claudemods_%s.iso", dateStr);
        saveConfig();
    }

    // Set output directory if not set
    if (strlen(config.outputDir) == 0) {
        printf("%s\nSetting output directory...%s\n", COLOR_CYAN, COLOR_RESET);
        snprintf(config.outputDir, sizeof(config.outputDir), "/home/%s/Downloads", USERNAME);
        char mkdir_cmd[512];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", config.outputDir);
        execute_command(mkdir_cmd, true);
        saveConfig();
    }

    // Set clone directory if not set
    if (strlen(config.cloneDir) == 0) {
        printf("%s\nSetting clone directory...%s\n", COLOR_CYAN, COLOR_RESET);
        snprintf(config.cloneDir, sizeof(config.cloneDir), "/home/%s/clone_system_temp", USERNAME);
        char mkdir_cmd[512];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "sudo mkdir -p %s", config.cloneDir);
        execute_command(mkdir_cmd, true);
        saveConfig();
    }

    // Select vmlinuz if not selected
    if (strlen(config.vmlinuzPath) == 0) {
        printf("%s\nSelecting vmlinuz...%s\n", COLOR_CYAN, COLOR_RESET);
        selectVmlinuz();
    }

    // Generate mkinitcpio if not generated
    if (!config.mkinitcpioGenerated) {
        printf("%s\nGenerating mkinitcpio...%s\n", COLOR_CYAN, COLOR_RESET);
        generateMkinitcpio();
    }

    // Edit GRUB config if not edited
    if (!config.grubEdited) {
        printf("%s\nEditing GRUB config...%s\n", COLOR_CYAN, COLOR_RESET);
        editGrubCfg();
    }

    // Create image
    printf("%s\nCreating image...%s\n", COLOR_CYAN, COLOR_RESET);
    char* outputDir = getOutputDirectory();
    char finalImgPath[1024];
    snprintf(finalImgPath, sizeof(finalImgPath), "%s/%s", outputDir, FINAL_IMG_NAME);

    // Use the user-specified clone directory
    char* cloneDir = expandPath(config.cloneDir);
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "sudo mkdir -p %s", cloneDir);
    execute_command(mkdir_cmd, true);

    // Directly rsync into the clone directory
    if (!copyFilesWithRsync(SOURCE_DIR, cloneDir)) {
        return;
    }

    // Create SquashFS from the clone directory
    createSquashFS(cloneDir, finalImgPath);

    createChecksum(finalImgPath);
    printFinalMessage(finalImgPath);

    // Create ISO
    printf("%s\nCreating ISO...%s\n", COLOR_CYAN, COLOR_RESET);
    createISO();

    printf("%s\nProcess completed successfully!%s\n", COLOR_GREEN, COLOR_RESET);
}

int main() {
    // Initialize config structure
    memset(&config, 0, sizeof(config));
    
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        strcpy(USERNAME, pw->pw_name);
    } else {
        fprintf(stderr, "%sFailed to get username!%s\n", COLOR_RED, COLOR_RESET);
        return 1;
    }

    snprintf(BUILD_DIR, sizeof(BUILD_DIR), "/home/%s/.config/cmi/build-image-arch-img", USERNAME);

    char configDir[256];
    snprintf(configDir, sizeof(configDir), "/home/%s/.config/cmi", USERNAME);
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", configDir);
    execute_command(mkdir_cmd, true);

    loadConfig();

    // Create time thread
    pthread_create(&time_thread, NULL, update_time_thread, NULL);

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Run the automatic process
    autoSetup();

    time_thread_running = false;
    pthread_join(time_thread, NULL);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return 0;
}