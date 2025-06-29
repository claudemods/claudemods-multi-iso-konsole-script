#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctime>
#include <libgen.h>
#include <dirent.h>
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <termios.h>
#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <array>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <pwd.h>
#include <sstream>
#include <filesystem>

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

using namespace std;

struct termios original_term;
enum Distro { ARCH, UBUNTU, DEBIAN, CACHYOS, NEON, UNKNOWN };

// Function declarations
string get_kernel_version();
string read_clone_dir();
bool dir_exists(const string &path);
void execute_command(const string& cmd);
Distro detect_distro();
string get_distro_name(Distro distro);
void edit_calamares_branding();
bool is_init_generated(Distro distro);
string get_iso_name();
void save_iso_name(const string &name);
void save_clone_dir(const string &dir_path);
void install_dependencies(Distro distro);
void generate_initrd(Distro distro);
void edit_isolinux_cfg(Distro distro);
void edit_grub_cfg(Distro distro);
void install_calamares(Distro distro);
void clone_system(const string &clone_dir);
void create_squashfs(Distro distro);
void create_squashfs_clone(Distro distro);
void delete_clone(Distro distro);
void create_iso(Distro distro, const string &output_dir);
void run_qemu();
void install_updater();
void show_status(Distro distro);
void create_commands();
void remove_commands();
bool commands_exist();

void execute_command(const string& cmd) {
    cout << COLOR_CYAN;
    fflush(stdout);
    string full_cmd = " " + cmd;
    int status = system(full_cmd.c_str());
    cout << COLOR_RESET;
    if (status != 0) {
        cerr << RED << "Error executing: " << full_cmd << RESET << endl;
        exit(1);
    }
}

Distro detect_distro() {
    ifstream os_release("/etc/os-release");
    if (!os_release.is_open()) return UNKNOWN;

    string line;
    while (getline(os_release, line)) {
        if (line.find("ID=") == 0) {
            if (line.find("arch") != string::npos) return ARCH;
            if (line.find("ubuntu") != string::npos) return UBUNTU;
            if (line.find("debian") != string::npos) return DEBIAN;
            if (line.find("cachyos") != string::npos) return CACHYOS;
            if (line.find("neon") != string::npos) return NEON;
        }
    }
    return UNKNOWN;
}

string get_distro_name(Distro distro) {
    switch(distro) {
        case ARCH: return "Arch";
        case UBUNTU: return "Ubuntu";
        case DEBIAN: return "Debian";
        case CACHYOS: return "CachyOS";
        case NEON: return "KDE Neon";
        default: return "Unknown";
    }
}

string get_kernel_version() {
    string version = "unknown";
    FILE* fp = popen("uname -r", "r");
    if (!fp) {
        perror("Failed to get kernel version");
        return version;
    }
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp)) {
        version = buffer;
        version.erase(version.find_last_not_of("\n") + 1);
    }
    pclose(fp);
    return version;
}

string read_clone_dir() {
    string file_path = "/home/" + string(getenv("USER")) + "/.config/cmi/clonedir.txt";
    ifstream f(file_path, ios::in | ios::binary);
    if (!f) return "";

    f.seekg(0, ios::end);
    size_t size = f.tellg();
    f.seekg(0, ios::beg);

    if (size == 0) return "";

    string dir_path(size, '\0');
    f.read(&dir_path[0], size);

    if (!dir_path.empty() && dir_path.back() == '\n') {
        dir_path.pop_back();
    }
    if (!dir_path.empty() && dir_path.back() != '/') {
        dir_path += '/';
    }

    return dir_path;
}

bool dir_exists(const string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void save_clone_dir(const string &dir_path) {
    string config_dir = "/home/" + string(getenv("USER")) + "/.config/cmi";
    if (!dir_exists(config_dir)) {
        string mkdir_cmd = "mkdir -p " + config_dir;
        execute_command(mkdir_cmd);
    }

    string full_clone_path = dir_path;
    if (full_clone_path.back() != '/') {
        full_clone_path += '/';
    }

    string file_path = config_dir + "/clonedir.txt";
    ofstream f(file_path, ios::out | ios::trunc);
    if (!f) {
        perror("Failed to open clonedir.txt");
        return;
    }
    f << full_clone_path;
    f.close();

    string clone_folder = full_clone_path + "clone_system_temp";
    if (!dir_exists(clone_folder)) {
        string mkdir_cmd = "mkdir -p " + clone_folder;
        execute_command(mkdir_cmd);
    }
}

void save_iso_name(const string &name) {
    string config_dir = "/home/" + string(getenv("USER")) + "/.config/cmi";
    if (!dir_exists(config_dir)) {
        string mkdir_cmd = "mkdir -p " + config_dir;
        execute_command(mkdir_cmd);
    }

    string file_path = config_dir + "/isoname.txt";
    ofstream f(file_path, ios::out | ios::trunc);
    if (!f) {
        perror("Failed to open isoname.txt");
        return;
    }
    f << name;
    f.close();
}

bool is_init_generated(Distro distro) {
    string init_path;
    if (distro == UBUNTU || distro == DEBIAN) {
        init_path = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-" +
        (distro == UBUNTU ? "noble" : "debian") + "/live/initrd.img-" + get_kernel_version();
    } else {
        init_path = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-arch/live/initramfs-linux.img";
    }
    struct stat st;
    return stat(init_path.c_str(), &st) == 0;
}

string get_iso_name() {
    string file_path = "/home/" + string(getenv("USER")) + "/.config/cmi/isoname.txt";
    ifstream f(file_path);
    if (!f) return "";

    string name;
    getline(f, name);
    return name;
}

void install_dependencies(Distro distro) {
    switch(distro) {
        case ARCH:
        case CACHYOS:
            execute_command("sudo pacman -S --needed --noconfirm arch-install-scripts bash-completion dosfstools erofs-utils findutils grub jq libarchive libisoburn lsb-release lvm2 mkinitcpio-archiso mkinitcpio-nfs-utils mtools nbd pacman-contrib parted procps-ng pv python rsync squashfs-tools sshfs syslinux xdg-utils bash-completion zsh-completions kernel-modules-hook virt-manager");
            break;
        case UBUNTU:
        case NEON:
            execute_command("sudo apt install -y cryptsetup dmeventd isolinux libaio-dev libcares2 libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot live-boot-doc live-boot-initramfs-tools live-config-systemd live-tools lvm2 pxelinux syslinux syslinux-common thin-provisioning-tools squashfs-tools xorriso");
            break;
        case DEBIAN:
            execute_command("sudo apt install -y cryptsetup dmeventd isolinux libaio1 libc-ares2 libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot live-boot-doc live-boot-initramfs-tools live-config-systemd live-tools lvm2 pxelinux syslinux syslinux-common thin-provisioning-tools squashfs-tools xorriso");
            break;
        default:
            cerr << RED << "Unsupported distribution" << RESET << endl;
            exit(1);
    }
}

void generate_initrd(Distro distro) {
    switch(distro) {
        case ARCH:
        case CACHYOS:
            execute_command("cd /home/$USER/.config/cmi/build-image-arch >/dev/null 2>&1 && sudo mkinitcpio -c live.conf -g /home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img");
            execute_command("sudo cp /boot/vmlinuz-linux* /home/$USER/.config/cmi/build-image-arch/live/ 2>/dev/null");
            break;
        case UBUNTU:
        case NEON:
            execute_command("sudo mkinitramfs -o \"/home/$USER/.config/cmi/build-image-noble/live/initrd.img-$(uname -r)\" \"$(uname -r)\"");
            execute_command("sudo cp /boot/vmlinuz* /home/$USER/.config/cmi/build-image-noble/live/ 2>/dev/null");
            break;
        case DEBIAN:
            execute_command("sudo mkinitramfs -o \"/home/$USER/.config/cmi/build-image-debian/live/initrd.img-$(uname -r)\" \"$(uname -r)\"");
            execute_command("sudo cp /boot/vmlinuz* /home/$USER/.config/cmi/build-image-debian/live/ 2>/dev/null");
            break;
        default:
            cerr << RED << "Unsupported distribution" << RESET << endl;
            exit(1);
    }
}

void edit_isolinux_cfg(Distro distro) {
    switch(distro) {
        case ARCH:
        case CACHYOS:
            execute_command("nano /home/$USER/.config/cmi/build-image-arch/isolinux/isolinux.cfg");
            break;
        case UBUNTU:
        case NEON:
            execute_command("nano /home/$USER/.config/cmi/build-image-noble/isolinux/isolinux.cfg");
            break;
        case DEBIAN:
            execute_command("nano /home/$USER/.config/cmi/build-image-debian/isolinux/isolinux.cfg");
            break;
        default:
            cerr << RED << "Unsupported distribution" << RESET << endl;
            exit(1);
    }
}

void edit_grub_cfg(Distro distro) {
    switch(distro) {
        case ARCH:
        case CACHYOS:
            execute_command("nano /home/$USER/.config/cmi/build-image-arch/boot/grub/grub.cfg");
            break;
        case UBUNTU:
        case NEON:
            execute_command("nano /home/$USER/.config/cmi/build-image-noble/boot/grub/grub.cfg");
            break;
        case DEBIAN:
            execute_command("nano /home/$USER/.config/cmi/build-image-debian/boot/grub/grub.cfg");
            break;
        default:
            cerr << RED << "Unsupported distribution" << RESET << endl;
            exit(1);
    }
}

void install_calamares(Distro distro) {
    switch(distro) {
        case ARCH:
            execute_command("cd /home/$USER/.config/cmi/calamares-per-distro/arch >/dev/null 2>&1 && sudo pacman -U calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar ckbcomp-1.227-2-any.pkg.tar.zst");
            break;
        case CACHYOS:
            execute_command("sudo pacman -U --noconfirm calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar ckbcomp-1.227-2-any.pkg.tar.zst");
            break;
        case UBUNTU:
        case NEON:
            execute_command("sudo apt install -y calamares-settings-ubuntu-common calamares");
            break;
        case DEBIAN:
            execute_command("sudo apt install -y calamares-settings-debian calamares");
            break;
        default:
            cerr << RED << "Unsupported distribution" << RESET << endl;
            exit(1);
    }
}

void edit_calamares_branding() {
    execute_command("sudo nano /usr/share/calamares/branding/claudemods/branding.desc");
}

void clone_system(const string &clone_dir) {
    string full_clone_path = clone_dir;
    if (full_clone_path.back() != '/') {
        full_clone_path += '/';
    }
    full_clone_path += "clone_system_temp";

    if (!dir_exists(full_clone_path)) {
        string mkdir_cmd = "mkdir -p " + full_clone_path;
        execute_command(mkdir_cmd);
    }

    string command = "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
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
    "/ " + full_clone_path;

    execute_command(command);
}

void create_squashfs(Distro distro) {
    string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        cerr << RED << "Error: No clone directory specified. Set it with 'cmi set-clone-dir'" << RESET << endl;
        exit(1);
    }

    string full_clone_path = clone_dir;
    if (full_clone_path.back() != '/') {
        full_clone_path += '/';
    }
    full_clone_path += "clone_system_temp";

    string output_path;
    if (distro == UBUNTU || distro == NEON) {
        output_path = "/home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs";
    } else if (distro == DEBIAN) {
        output_path = "/home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs";
    } else {
        output_path = "/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs";
    }

    string command = "sudo mksquashfs " + full_clone_path + " " + output_path + " "
    "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs";

    execute_command(command);
}

void create_squashfs_clone(Distro distro) {
    string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        cerr << RED << "Error: No clone directory specified. Set it with 'cmi set-clone-dir'" << RESET << endl;
        exit(1);
    }

    if (!dir_exists(clone_dir + "/clone_system_temp")) {
        clone_system(clone_dir);
    }
    create_squashfs(distro);
}

void delete_clone(Distro distro) {
    string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        cerr << RED << "Error: No clone directory specified" << RESET << endl;
        exit(1);
    }

    string full_clone_path = clone_dir;
    if (full_clone_path.back() != '/') {
        full_clone_path += '/';
    }
    full_clone_path += "clone_system_temp";

    string command = "sudo rm -rf " + full_clone_path;
    execute_command(command);

    string squashfs_path;
    if (distro == UBUNTU || distro == NEON) {
        squashfs_path = "/home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs";
    } else if (distro == DEBIAN) {
        squashfs_path = "/home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs";
    } else {
        squashfs_path = "/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs";
    }

    struct stat st;
    if (stat(squashfs_path.c_str(), &st) == 0) {
        command = "sudo rm -f " + squashfs_path;
        execute_command(command);
    }
}

void create_iso(Distro distro, const string &output_dir) {
    string iso_name = get_iso_name();
    if (iso_name.empty()) {
        cerr << RED << "Error: ISO name not set. Set it with 'cmi set-iso-name'" << RESET << endl;
        exit(1);
    }

    if (output_dir.empty()) {
        cerr << RED << "Error: Output directory cannot be empty" << RESET << endl;
        exit(1);
    }

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H%M", t);

    string iso_file_name = output_dir + "/" + iso_name + "_amd64_" + timestamp + ".iso";

    string build_image_dir;
    if (distro == UBUNTU || distro == NEON) {
        build_image_dir = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-noble";
    } else if (distro == DEBIAN) {
        build_image_dir = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-debian";
    } else {
        build_image_dir = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-arch";
    }

    if (!dir_exists(output_dir)) {
        execute_command("mkdir -p " + output_dir);
    }

    string xorriso_command = "sudo xorriso -as mkisofs -o " + iso_file_name +
    " -V 2025 -iso-level 3";

        if (distro == UBUNTU || distro == NEON) {
            xorriso_command += " -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin"
            " -c isolinux/boot.cat"
            " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
            " -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot -isohybrid-gpt-basdat";
        } else if (distro == DEBIAN) {
            xorriso_command += " -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin"
            " -c isolinux/boot.cat"
            " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
            " -eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat";
        } else {
            xorriso_command += " -isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin"
            " -c isolinux/boot.cat"
            " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
            " -eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat";
        }

        xorriso_command += " " + build_image_dir;
        execute_command(xorriso_command);
}

void run_qemu() {
    execute_command("ruby /opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb");
}

void install_updater() {
    execute_command("./home/$USER/.config/cmi/patch.sh");
}

void show_status(Distro distro) {
    string clone_dir = read_clone_dir();
    cout << "Clone Directory: " << (clone_dir.empty() ? RED + string("Not set") : GREEN + clone_dir) << RESET << endl;

    cout << "Initramfs: " << (is_init_generated(distro) ? GREEN + string("Generated") : RED + string("Not generated")) << RESET << endl;

    string iso_name = get_iso_name();
    cout << "ISO Name: " << (iso_name.empty() ? RED + string("Not set") : GREEN + iso_name) << RESET << endl;
}

bool commands_exist() {
    string test_cmd = "ls /usr/local/bin/cmi-* >/dev/null 2>&1";
    return system(test_cmd.c_str()) == 0;
}

void create_commands() {
    if (commands_exist()) {
        cout << "Commands already exist in /usr/local/bin/" << endl;
        return;
    }

    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len == -1) {
        perror("readlink");
        exit(1);
    }
    exe_path[len] = '\0';

    // Create all the command files
    string commands[] = {
        "install-deps",
        "gen-init",
        "set-iso-name",
        "edit-isolinux",
        "edit-grub",
        "set-clone-dir",
        "install-calamares",
        "edit-branding",
        "install-updater",
        "create-squashfs",
        "create-squashfs-clone",
        "delete-clone",
        "create-iso",
        "run-qemu",
        "status"
    };

    for (const auto& cmd : commands) {
        string script = "#!/bin/sh\n";
        script += "exec " + string(exe_path) + " " + cmd + " \"$@\"\n";

        string file_path = "/usr/local/bin/cmi-" + cmd;
        ofstream f(file_path);
        if (!f) {
            cerr << RED << "Failed to create command file: " << file_path << RESET << endl;
            continue;
        }
        f << script;
        f.close();

        execute_command("sudo chmod 755 " + file_path);
    }

    cout << GREEN << "Commands created in /usr/local/bin/" << RESET << endl;
}

void remove_commands() {
    string commands[] = {
        "install-deps",
        "gen-init",
        "set-iso-name",
        "edit-isolinux",
        "edit-grub",
        "set-clone-dir",
        "install-calamares",
        "edit-branding",
        "install-updater",
        "create-squashfs",
        "create-squashfs-clone",
        "delete-clone",
        "create-iso",
        "run-qemu",
        "status"
    };

    for (const auto& cmd : commands) {
        string file_path = "/usr/local/bin/cmi-" + cmd;
        execute_command("sudo rm -f " + file_path);
    }

    cout << GREEN << "Commands removed from /usr/local/bin/" << RESET << endl;
}

void print_usage() {
    cout << "Usage: cmi <command> [options]" << endl;
    cout << "Commands:" << endl;
    cout << "  install-deps          - Install dependencies for current distro" << endl;
    cout << "  gen-init              - Generate initramfs for current distro" << endl;
    cout << "  set-iso-name <name>   - Set ISO name (without extension)" << endl;
    cout << "  edit-isolinux         - Edit isolinux.cfg" << endl;
    cout << "  edit-grub             - Edit grub.cfg" << endl;
    cout << "  set-clone-dir <path>  - Set clone directory path" << endl;
    cout << "  install-calamares     - Install Calamares installer" << endl;
    cout << "  edit-branding         - Edit Calamares branding" << endl;
    cout << "  install-updater       - Install one-time updater" << endl;
    cout << "  create-squashfs       - Create SquashFS image" << endl;
    cout << "  create-squashfs-clone - Clone system and create SquashFS" << endl;
    cout << "  delete-clone          - Delete clone directory and SquashFS" << endl;
    cout << "  create-iso <out_dir>  - Create ISO in specified directory" << endl;
    cout << "  run-qemu              - Run ISO in QEMU" << endl;
    cout << "  status                - Show current configuration status" << endl;
    cout << "  create-commands       - Create command shortcuts in /usr/local/bin" << endl;
    cout << "  remove-commands       - Remove command shortcuts" << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    Distro distro = detect_distro();
    string command = argv[1];

    if (command == "install-deps") {
        install_dependencies(distro);
    }
    else if (command == "gen-init") {
        generate_initrd(distro);
    }
    else if (command == "set-iso-name") {
        if (argc < 3) {
            cerr << RED << "Error: ISO name required" << RESET << endl;
            return 1;
        }
        save_iso_name(argv[2]);
    }
    else if (command == "edit-isolinux") {
        edit_isolinux_cfg(distro);
    }
    else if (command == "edit-grub") {
        edit_grub_cfg(distro);
    }
    else if (command == "set-clone-dir") {
        if (argc < 3) {
            cerr << RED << "Error: Directory path required" << RESET << endl;
            return 1;
        }
        save_clone_dir(argv[2]);
    }
    else if (command == "install-calamares") {
        install_calamares(distro);
    }
    else if (command == "edit-branding") {
        edit_calamares_branding();
    }
    else if (command == "install-updater") {
        install_updater();
    }
    else if (command == "create-squashfs") {
        create_squashfs(distro);
    }
    else if (command == "create-squashfs-clone") {
        create_squashfs_clone(distro);
    }
    else if (command == "delete-clone") {
        delete_clone(distro);
    }
    else if (command == "create-iso") {
        if (argc < 3) {
            cerr << RED << "Error: Output directory required" << RESET << endl;
            return 1;
        }
        create_iso(distro, argv[2]);
    }
    else if (command == "run-qemu") {
        run_qemu();
    }
    else if (command == "status") {
        show_status(distro);
    }
    else if (command == "create-commands") {
        create_commands();
    }
    else if (command == "remove-commands") {
        remove_commands();
    }
    else {
        cerr << RED << "Error: Unknown command" << RESET << endl;
        print_usage();
        return 1;
    }

    return 0;
}
