#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <libgen.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>

// Constants
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

// Paths
#define VERSION_FILE "/home/$USER/.config/cmi/version.txt"
#define GUIDE_PATH "/home/$USER/.config/cmi/readme.txt"
#define CHANGELOG_PATH "/home/$USER/.config/cmi/changelog.txt"
#define CLONE_DIR_FILE "/home/$USER/.config/cmi/clonedir.txt"
#define ISO_NAME_FILE "/home/$USER/.config/cmi/isoname.txt"

typedef enum {
    ARCH,
    UBUNTU,
    DEBIAN,
    CACHYOS,
    NEON,
    UNKNOWN
} Distro;

char* expand_path(const char* path) {
    const char* user = getenv("USER");
    if (!user) user = "";

    char* result = malloc(MAX_PATH);
    const char* p = path;
    char* r = result;

    while (*p) {
        if (strncmp(p, "$USER", 5) == 0) {
            strcpy(r, user);
            r += strlen(user);
            p += 5;
        } else {
            *r++ = *p++;
        }
    }
    *r = '\0';
    return result;
}

char* get_cmi_version() {
    char* path = expand_path(VERSION_FILE);
    FILE* file = fopen(path, "r");
    free(path);

    if (!file) return strdup("unknown");

    char* version = malloc(256);
    if (!fgets(version, 256, file)) {
        strcpy(version, "unknown");
    } else {
        version[strcspn(version, "\n")] = '\0';
    }
    fclose(file);
    return version;
}

char* get_kernel_version() {
    FILE* fp = popen("uname -r", "r");
    if (!fp) return strdup("unknown");

    char* version = malloc(256);
    if (!fgets(version, 256, fp)) {
        strcpy(version, "unknown");
    } else {
        version[strcspn(version, "\n")] = '\0';
    }
    pclose(fp);
    return version;
}

char* get_vmlinuz_version() {
    FILE* fp = popen("ls /boot/vmlinuz*", "r");
    if (!fp) return strdup("unknown");

    char* version = malloc(256);
    if (!fgets(version, 256, fp)) {
        strcpy(version, "unknown");
    } else {
        char* last_slash = strrchr(version, '/');
        if (last_slash) {
            strcpy(version, last_slash + 1);
        }
        version[strcspn(version, "\n")] = '\0';
    }
    pclose(fp);
    return version;
}

char* get_distro_version() {
    FILE* file = fopen("/etc/os-release", "r");
    if (!file) return strdup("unknown");

    char* line = NULL;
    size_t len = 0;
    char* version = strdup("unknown");

    while (getline(&line, &len, file) != -1) {
        if (strstr(line, "VERSION_ID=") == line) {
            free(version);
            version = strdup(line + 11);
            char* quote = strchr(version, '"');
            if (quote) {
                memmove(version, version + 1, strlen(version));
                char* end_quote = strchr(version, '"');
                if (end_quote) *end_quote = '\0';
            } else {
                version[strcspn(version, "\n")] = '\0';
            }
            break;
        }
    }

    free(line);
    fclose(file);
    return version;
}

Distro detect_distro() {
    FILE* file = fopen("/etc/os-release", "r");
    if (!file) return UNKNOWN;

    char* line = NULL;
    size_t len = 0;
    Distro distro = UNKNOWN;

    while (getline(&line, &len, file) != -1) {
        if (strstr(line, "ID=") == line) {
            if (strstr(line, "arch")) distro = ARCH;
            else if (strstr(line, "ubuntu")) distro = UBUNTU;
            else if (strstr(line, "debian")) distro = DEBIAN;
            else if (strstr(line, "cachyos")) distro = CACHYOS;
            else if (strstr(line, "neon")) distro = NEON;
            break;
        }
    }

    free(line);
    fclose(file);
    return distro;
}

const char* get_distro_name(Distro distro) {
    switch(distro) {
        case ARCH: return "Arch Linux";
        case UBUNTU: return "Ubuntu";
        case DEBIAN: return "Debian";
        case CACHYOS: return "CachyOS";
        case NEON: return "KDE Neon";
        default: return "Unknown";
    }
}

int execute_command(const char* cmd) {
    int status = system(cmd);
    if (status != 0) {
        fprintf(stderr, "Command failed: %s\n", cmd);
        return 1;
    }
    return 0;
}

char* read_clone_dir() {
    char* path = expand_path(CLONE_DIR_FILE);
    FILE* file = fopen(path, "r");
    free(path);

    if (!file) return NULL;

    char* dir = malloc(MAX_PATH);
    if (!fgets(dir, MAX_PATH, file)) {
        free(dir);
        fclose(file);
        return NULL;
    }

    fclose(file);

    dir[strcspn(dir, "\n")] = '\0';
    size_t len = strlen(dir);
    if (len > 0 && dir[len-1] != '/') {
        dir[len] = '/';
        dir[len+1] = '\0';
    }

    return dir;
}

bool dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int save_clone_dir(const char* dir_path) {
    char* config_dir = expand_path("/home/$USER/.config/cmi");
    if (!dir_exists(config_dir)) {
        char cmd[MAX_CMD];
        snprintf(cmd, MAX_CMD, "mkdir -p %s", config_dir);
        if (execute_command(cmd)) {
            free(config_dir);
            return 1;
        }
    }

    char* full_clone_path = strdup(dir_path);
    size_t len = strlen(full_clone_path);
    if (len > 0 && full_clone_path[len-1] != '/') {
        full_clone_path = realloc(full_clone_path, len + 2);
        full_clone_path[len] = '/';
        full_clone_path[len+1] = '\0';
    }

    char* file_path = malloc(strlen(config_dir) + strlen("/clonedir.txt") + 1);
    strcpy(file_path, config_dir);
    strcat(file_path, "/clonedir.txt");

    FILE* f = fopen(file_path, "w");
    if (!f) {
        free(config_dir);
        free(full_clone_path);
        free(file_path);
        return 1;
    }

    fprintf(f, "%s", full_clone_path);
    fclose(f);

    char* clone_folder = malloc(strlen(full_clone_path) + strlen("clone_system_temp") + 1);
    strcpy(clone_folder, full_clone_path);
    strcat(clone_folder, "clone_system_temp");

    if (!dir_exists(clone_folder)) {
        char cmd[MAX_CMD];
        snprintf(cmd, MAX_CMD, "mkdir -p %s", clone_folder);
        if (execute_command(cmd)) {
            free(config_dir);
            free(full_clone_path);
            free(file_path);
            free(clone_folder);
            return 1;
        }
    }

    free(config_dir);
    free(full_clone_path);
    free(file_path);
    free(clone_folder);
    return 0;
}

int save_iso_name(const char* name) {
    char* config_dir = expand_path("/home/$USER/.config/cmi");
    if (!dir_exists(config_dir)) {
        char cmd[MAX_CMD];
        snprintf(cmd, MAX_CMD, "mkdir -p %s", config_dir);
        if (execute_command(cmd)) {
            free(config_dir);
            return 1;
        }
    }

    char* file_path = malloc(strlen(config_dir) + strlen("/isoname.txt") + 1);
    strcpy(file_path, config_dir);
    strcat(file_path, "/isoname.txt");

    FILE* f = fopen(file_path, "w");
    if (!f) {
        free(config_dir);
        free(file_path);
        return 1;
    }

    fprintf(f, "%s", name);
    fclose(f);

    free(config_dir);
    free(file_path);
    return 0;
}

char* get_iso_name() {
    char* path = expand_path(ISO_NAME_FILE);
    FILE* file = fopen(path, "r");
    free(path);

    if (!file) return NULL;

    char* name = malloc(256);
    if (!fgets(name, 256, file)) {
        free(name);
        fclose(file);
        return NULL;
    }

    fclose(file);
    name[strcspn(name, "\n")] = '\0';
    return name;
}

bool is_init_generated(Distro distro) {
    char* init_path;
    if (distro == UBUNTU || distro == DEBIAN) {
        const char* distro_name = (distro == UBUNTU) ? "noble" : "debian";
        char* kernel_version = get_kernel_version();
        init_path = malloc(strlen("/home/$USER/.config/cmi/build-image-") + strlen(distro_name) +
        strlen("/live/initrd.img-") + strlen(kernel_version) + 1);
        sprintf(init_path, "/home/$USER/.config/cmi/build-image-%s/live/initrd.img-%s",
                distro_name, kernel_version);
        free(kernel_version);
    } else {
        init_path = expand_path("/home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img");
    }

    struct stat st;
    bool exists = (stat(init_path, &st) == 0);
    free(init_path);
    return exists;
}

int install_dependencies(Distro distro) {
    switch(distro) {
        case ARCH:
        case CACHYOS:
            return execute_command("sudo pacman -S --needed --noconfirm arch-install-scripts bash-completion "
            "dosfstools erofs-utils findutils grub jq libarchive libisoburn lsb-release "
            "lvm2 mkinitcpio-archiso mkinitcpio-nfs-utils mtools nbd pacman-contrib "
            "parted procps-ng pv python rsync squashfs-tools sshfs syslinux xdg-utils "
            "bash-completion zsh-completions kernel-modules-hook virt-manager");
        case UBUNTU:
        case NEON:
            return execute_command("sudo apt install -y cryptsetup dmeventd isolinux libaio-dev libcares2 "
            "libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot live-boot-doc "
            "live-boot-initramfs-tools live-config-systemd live-tools lvm2 pxelinux "
            "syslinux syslinux-common thin-provisioning-tools squashfs-tools xorriso");
        case DEBIAN:
            return execute_command("sudo apt install -y cryptsetup dmeventd isolinux libaio1 libc-ares2 "
            "libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot live-boot-doc "
            "live-boot-initramfs-tools live-config-systemd live-tools lvm2 pxelinux "
            "syslinux syslinux-common thin-provisioning-tools squashfs-tools xorriso");
        default:
            return 1;
    }
}

int generate_initrd(Distro distro) {
    switch(distro) {
        case ARCH:
        case CACHYOS: {
            char* build_dir = expand_path("/home/$USER/.config/cmi/build-image-arch");
            char* init_path = expand_path("/home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img");
            char cmd[MAX_CMD];
            snprintf(cmd, MAX_CMD, "cd %s >/dev/null 2>&1 && sudo mkinitcpio -c live.conf -g %s",
                     build_dir, init_path);
            int status = execute_command(cmd);
            free(build_dir);
            free(init_path);

            if (status) return status;

            char* live_path = expand_path("/home/$USER/.config/cmi/build-image-arch/live/");
            snprintf(cmd, MAX_CMD, "sudo cp /boot/vmlinuz-linux* %s 2>/dev/null", live_path);
            status = execute_command(cmd);
            free(live_path);
            return status;
        }
        case UBUNTU:
        case NEON: {
            char* init_path = expand_path("/home/$USER/.config/cmi/build-image-noble/live/initrd.img-$(uname -r)");
            char cmd[MAX_CMD];
            snprintf(cmd, MAX_CMD, "sudo mkinitramfs -o \"%s\" \"$(uname -r)\"", init_path);
            int status = execute_command(cmd);
            free(init_path);

            if (status) return status;

            char* live_path = expand_path("/home/$USER/.config/cmi/build-image-noble/live/");
            snprintf(cmd, MAX_CMD, "sudo cp /boot/vmlinuz* %s 2>/dev/null", live_path);
            status = execute_command(cmd);
            free(live_path);
            return status;
        }
        case DEBIAN: {
            char* init_path = expand_path("/home/$USER/.config/cmi/build-image-debian/live/initrd.img-$(uname -r)");
            char cmd[MAX_CMD];
            snprintf(cmd, MAX_CMD, "sudo mkinitramfs -o \"%s\" \"$(uname -r)\"", init_path);
            int status = execute_command(cmd);
            free(init_path);

            if (status) return status;

            char* live_path = expand_path("/home/$USER/.config/cmi/build-image-debian/live/");
            snprintf(cmd, MAX_CMD, "sudo cp /boot/vmlinuz* %s 2>/dev/null", live_path);
            status = execute_command(cmd);
            free(live_path);
            return status;
        }
        default:
            return 1;
    }
}

int edit_isolinux_cfg(Distro distro) {
    char* path;
    switch(distro) {
        case ARCH:
        case CACHYOS:
            path = expand_path("/home/$USER/.config/cmi/build-image-arch/isolinux/isolinux.cfg");
            break;
        case UBUNTU:
        case NEON:
            path = expand_path("/home/$USER/.config/cmi/build-image-noble/isolinux/isolinux.cfg");
            break;
        case DEBIAN:
            path = expand_path("/home/$USER/.config/cmi/build-image-debian/isolinux/isolinux.cfg");
            break;
        default:
            return 1;
    }

    char cmd[MAX_CMD];
    snprintf(cmd, MAX_CMD, "nano %s", path);
    int status = execute_command(cmd);
    free(path);
    return status;
}

int edit_grub_cfg(Distro distro) {
    char* path;
    switch(distro) {
        case ARCH:
        case CACHYOS:
            path = expand_path("/home/$USER/.config/cmi/build-image-arch/boot/grub/grub.cfg");
            break;
        case UBUNTU:
        case NEON:
            path = expand_path("/home/$USER/.config/cmi/build-image-noble/boot/grub/grub.cfg");
            break;
        case DEBIAN:
            path = expand_path("/home/$USER/.config/cmi/build-image-debian/boot/grub/grub.cfg");
            break;
        default:
            return 1;
    }

    char cmd[MAX_CMD];
    snprintf(cmd, MAX_CMD, "nano %s", path);
    int status = execute_command(cmd);
    free(path);
    return status;
}

int install_calamares(Distro distro) {
    switch(distro) {
        case ARCH: {
            char* calamares_dir = expand_path("/home/$USER/.config/cmi/calamares-per-distro/arch");
            char cmd[MAX_CMD];
            snprintf(cmd, MAX_CMD, "cd %s >/dev/null 2>&1 && sudo pacman -U calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst "
            "calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar "
            "ckbcomp-1.227-2-any.pkg.tar.zst", calamares_dir);
            int status = execute_command(cmd);
            free(calamares_dir);
            return status;
        }
        case CACHYOS:
            return execute_command("sudo pacman -U --noconfirm calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst "
            "calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar "
            "ckbcomp-1.227-2-any.pkg.tar.zst");
        case UBUNTU:
        case NEON:
            return execute_command("sudo apt install -y calamares-settings-ubuntu-common calamares");
        case DEBIAN:
            return execute_command("sudo apt install -y calamares-settings-debian calamares");
        default:
            return 1;
    }
}

int edit_calamares_branding() {
    return execute_command("sudo nano /usr/share/calamares/branding/claudemods/branding.desc");
}

int clone_system(const char* clone_dir) {
    char* full_clone_path = malloc(strlen(clone_dir) + strlen("/clone_system_temp") + 1);
    strcpy(full_clone_path, clone_dir);
    if (full_clone_path[strlen(full_clone_path)-1] != '/') {
        strcat(full_clone_path, "/");
    }
    strcat(full_clone_path, "clone_system_temp");

    if (!dir_exists(full_clone_path)) {
        char cmd[MAX_CMD];
        snprintf(cmd, MAX_CMD, "mkdir -p %s", full_clone_path);
        if (execute_command(cmd)) {
            free(full_clone_path);
            return 1;
        }
    }

    char cmd[MAX_CMD];
    snprintf(cmd, MAX_CMD, "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
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
    "/ %s", full_clone_path);

    int status = execute_command(cmd);
    free(full_clone_path);
    return status;
}

int create_squashfs(Distro distro) {
    char* clone_dir = read_clone_dir();
    if (!clone_dir) {
        fprintf(stderr, "Error: Clone directory not set\n");
        return 1;
    }

    char* full_clone_path = malloc(strlen(clone_dir) + strlen("clone_system_temp") + 1);
    strcpy(full_clone_path, clone_dir);
    strcat(full_clone_path, "clone_system_temp");
    free(clone_dir);

    char* output_path;
    switch(distro) {
        case UBUNTU:
        case NEON:
            output_path = expand_path("/home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs");
            break;
        case DEBIAN:
            output_path = expand_path("/home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs");
            break;
        default:
            output_path = expand_path("/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs");
    }

    char cmd[MAX_CMD];
    snprintf(cmd, MAX_CMD, "sudo mksquashfs %s %s -comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs", full_clone_path, output_path);

    int status = execute_command(cmd);
    free(full_clone_path);
    free(output_path);
    return status;
}

int create_squashfs_clone(Distro distro) {
    char* clone_dir = read_clone_dir();
    if (!clone_dir) {
        fprintf(stderr, "Error: Clone directory not set\n");
        return 1;
    }

    char* full_clone_path = malloc(strlen(clone_dir) + strlen("clone_system_temp") + 1);
    strcpy(full_clone_path, clone_dir);
    strcat(full_clone_path, "clone_system_temp");

    if (!dir_exists(full_clone_path)) {
        if (clone_system(clone_dir)) {
            free(clone_dir);
            free(full_clone_path);
            return 1;
        }
    }

    free(clone_dir);
    int status = create_squashfs(distro);
    free(full_clone_path);
    return status;
}

int delete_clone(Distro distro) {
    char* clone_dir = read_clone_dir();
    if (!clone_dir) {
        fprintf(stderr, "Error: Clone directory not set\n");
        return 1;
    }

    char* full_clone_path = malloc(strlen(clone_dir) + strlen("clone_system_temp") + 1);
    strcpy(full_clone_path, clone_dir);
    strcat(full_clone_path, "clone_system_temp");
    free(clone_dir);

    char cmd[MAX_CMD];
    snprintf(cmd, MAX_CMD, "sudo rm -rf %s", full_clone_path);
    int status = execute_command(cmd);
    free(full_clone_path);

    if (status) return status;

    char* squashfs_path;
    switch(distro) {
        case UBUNTU:
        case NEON:
            squashfs_path = expand_path("/home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs");
            break;
        case DEBIAN:
            squashfs_path = expand_path("/home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs");
            break;
        default:
            squashfs_path = expand_path("/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs");
    }

    struct stat st;
    if (stat(squashfs_path, &st) == 0) {
        snprintf(cmd, MAX_CMD, "sudo rm -f %s", squashfs_path);
        status = execute_command(cmd);
    }

    free(squashfs_path);
    return status;
}

int create_iso(Distro distro, const char* output_dir) {
    char* iso_name = get_iso_name();
    if (!iso_name) {
        fprintf(stderr, "Error: ISO name not set\n");
        return 1;
    }

    if (!output_dir || strlen(output_dir) == 0) {
        fprintf(stderr, "Error: Output directory not specified\n");
        free(iso_name);
        return 1;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H%M", t);

    char* iso_file_name = malloc(strlen(output_dir) + strlen(iso_name) + strlen("_amd64_.iso") + strlen(timestamp) + 1);
    sprintf(iso_file_name, "%s/%s_amd64_%s.iso", output_dir, iso_name, timestamp);
    free(iso_name);

    char* build_image_dir;
    switch(distro) {
        case UBUNTU:
        case NEON:
            build_image_dir = expand_path("/home/$USER/.config/cmi/build-image-noble");
            break;
        case DEBIAN:
            build_image_dir = expand_path("/home/$USER/.config/cmi/build-image-debian");
            break;
        default:
            build_image_dir = expand_path("/home/$USER/.config/cmi/build-image-arch");
    }

    if (!dir_exists(output_dir)) {
        char cmd[MAX_CMD];
        snprintf(cmd, MAX_CMD, "mkdir -p %s", output_dir);
        if (execute_command(cmd)) {
            free(iso_file_name);
            free(build_image_dir);
            return 1;
        }
    }

    char* xorriso_command = malloc(MAX_CMD);
    snprintf(xorriso_command, MAX_CMD, "sudo xorriso -as mkisofs -o %s -V 2025 -iso-level 3", iso_file_name);
    free(iso_file_name);

    switch(distro) {
        case UBUNTU:
        case NEON:
            strcat(xorriso_command, " -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin"
            " -c isolinux/boot.cat"
            " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
            " -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot -isohybrid-gpt-basdat");
            break;
        case DEBIAN:
            strcat(xorriso_command, " -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin"
            " -c isolinux/boot.cat"
            " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
            " -eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat");
            break;
        default:
            strcat(xorriso_command, " -isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin"
            " -c isolinux/boot.cat"
            " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
            " -eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat");
    }

    char* full_command = realloc(xorriso_command, strlen(xorriso_command) + strlen(build_image_dir) + 2);
    strcat(full_command, " ");
    strcat(full_command, build_image_dir);
    free(build_image_dir);

    int status = execute_command(full_command);
    free(full_command);
    return status;
}

int run_qemu() {
    return execute_command("ruby /opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb");
}

int install_updater() {
    char* path = expand_path("./home/$USER/.config/cmi/patch.sh");
    char cmd[MAX_CMD];
    snprintf(cmd, MAX_CMD, "%s", path);
    int status = execute_command(cmd);
    free(path);
    return status;
}

int show_guide() {
    char* path = expand_path(GUIDE_PATH);
    char cmd[MAX_CMD];
    snprintf(cmd, MAX_CMD, "cat %s", path);
    int status = execute_command(cmd);
    free(path);
    return status;
}

int show_changelog() {
    char* path = expand_path(CHANGELOG_PATH);
    char cmd[MAX_CMD];
    snprintf(cmd, MAX_CMD, "cat %s", path);
    int status = execute_command(cmd);
    free(path);
    return status;
}

void show_status(Distro distro) {
    const char* distro_name = get_distro_name(distro);
    char* distro_version = get_distro_version();
    char* cmi_version = get_cmi_version();
    char* kernel_version = get_kernel_version();
    char* vmlinuz_version = get_vmlinuz_version();
    char* clone_dir = read_clone_dir();
    char* iso_name = get_iso_name();
    bool init_generated = is_init_generated(distro);

    printf("Current Distribution: %s%s %s%s\n", COLOR_CYAN, distro_name, distro_version, RESET);
    printf("CMI Version: %s%s%s\n", COLOR_CYAN, cmi_version, RESET);
    printf("Kernel Version: %s%s%s\n", COLOR_CYAN, kernel_version, RESET);
    printf("vmlinuz Version: %s%s%s\n", COLOR_CYAN, vmlinuz_version, RESET);

    printf("Clone Directory: %s%s%s\n",
           clone_dir ? GREEN : RED,
           clone_dir ? clone_dir : "Not set",
           RESET);

    printf("Initramfs: %s%s%s\n",
           init_generated ? GREEN : RED,
           init_generated ? "Generated" : "Not generated",
           RESET);

    printf("ISO Name: %s%s%s\n",
           iso_name ? GREEN : RED,
           iso_name ? iso_name : "Not set",
           RESET);

    free(distro_version);
    free(cmi_version);
    free(kernel_version);
    free(vmlinuz_version);
    free(clone_dir);
    free(iso_name);
}

void print_usage() {
    printf("Usage: cmi <command> [options]\n\n");
    printf("Available commands:\n");
    printf("%s  install-deps            - Install required dependencies\n", COLOR_CYAN);
    printf("  gen-init                - Generate initramfs\n");
    printf("  set-iso-name <name>     - Set ISO output name\n");
    printf("  edit-isolinux           - Edit isolinux configuration\n");
    printf("  edit-grub               - Edit GRUB configuration\n");
    printf("  set-clone-dir <path>    - Set directory for system cloning\n");
    printf("  install-calamares       - Install Calamares installer\n");
    printf("  edit-branding           - Edit Calamares branding\n");
    printf("  install-updater         - Install updater\n");
    printf("  create-squashfs         - Create squashfs from current system\n");
    printf("  create-squashfs-clone   - Clone system and create squashfs\n");
    printf("  delete-clone            - Delete cloned system\n");
    printf("  create-iso <out_dir>    - Create ISO image\n");
    printf("  run-qemu                - Run QEMU with the ISO\n");
    printf("  status                  - Show current status\n");
    printf("  guide                   - Show user guide\n");
    printf("  changelog               - Show changelog\n");
    printf("  mainmenu                - Launch main menu (cmitui)%s\n", RESET);
}

int main(int argc, char* argv[]) {
    Distro distro = detect_distro();

    if (argc < 2) {
        show_status(distro);
        printf("\n");
        print_usage();
        return 0;
    }

    const char* command = argv[1];
    int status = 0;

    if (strcmp(command, "install-deps") == 0) {
        status = install_dependencies(distro);
    } else if (strcmp(command, "gen-init") == 0) {
        status = generate_initrd(distro);
    } else if (strcmp(command, "set-iso-name") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: ISO name not specified\n");
            return 1;
        }
        status = save_iso_name(argv[2]);
    } else if (strcmp(command, "edit-isolinux") == 0) {
        status = edit_isolinux_cfg(distro);
    } else if (strcmp(command, "edit-grub") == 0) {
        status = edit_grub_cfg(distro);
    } else if (strcmp(command, "set-clone-dir") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Clone directory not specified\n");
            return 1;
        }
        status = save_clone_dir(argv[2]);
    } else if (strcmp(command, "install-calamares") == 0) {
        status = install_calamares(distro);
    } else if (strcmp(command, "edit-branding") == 0) {
        status = edit_calamares_branding();
    } else if (strcmp(command, "install-updater") == 0) {
        status = install_updater();
    } else if (strcmp(command, "create-squashfs") == 0) {
        status = create_squashfs(distro);
    } else if (strcmp(command, "create-squashfs-clone") == 0) {
        status = create_squashfs_clone(distro);
    } else if (strcmp(command, "delete-clone") == 0) {
        status = delete_clone(distro);
    } else if (strcmp(command, "create-iso") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Output directory not specified\n");
            return 1;
        }
        status = create_iso(distro, argv[2]);
    } else if (strcmp(command, "run-qemu") == 0) {
        status = run_qemu();
    } else if (strcmp(command, "status") == 0) {
        show_status(distro);
    } else if (strcmp(command, "guide") == 0) {
        status = show_guide();
    } else if (strcmp(command, "changelog") == 0) {
        status = show_changelog();
    } else if (strcmp(command, "mainmenu") == 0) {
        status = execute_command("cmitui");
    } else {
        show_status(distro);
        printf("\n");
        print_usage();
        return 1;
    }

    if (status != 0) {
        fprintf(stderr, "Operation failed\n");
        return status;
    }

    return 0;
}
