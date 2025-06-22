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
enum Distro { ARCH, UBUNTU, DEBIAN, UNKNOWN };

Distro detect_distro() {
    std::ifstream os_release("/etc/os-release");
    if (!os_release.is_open()) return UNKNOWN;

    std::string line;
    while (std::getline(os_release, line)) {
        if (line.find("ID=") == 0) {
            if (line.find("arch") != std::string::npos) return ARCH;
            if (line.find("ubuntu") != std::string::npos) return UBUNTU;
            if (line.find("debian") != std::string::npos) return DEBIAN;
        }
    }
    return UNKNOWN;
}

// Forward declarations
std::string read_clone_dir();
void save_clone_dir(const std::string &dir_path);
void print_banner(const std::string& distro_name = "Arch");

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

void print_blue(const std::string &text) { std::cout << BLUE << text << RESET << std::endl; }

void message_box(const std::string &title, const std::string &message) {
    std::cout << GREEN << title << RESET << std::endl;
    std::cout << GREEN << message << RESET << std::endl;
}

void error_box(const std::string &title, const std::string &message) {
    std::cout << RED << title << RESET << std::endl;
    std::cout << RED << message << RESET << std::endl;
}

void progress_dialog(const std::string &message) { std::cout << BLUE << message << RESET << std::endl; }

void run_command(const std::string &command) {
    std::cout << BLUE << "Running command: " << command << RESET << std::endl;
    int status = system(command.c_str());
    if (status != 0) {
        std::cout << RED << "Command failed with exit code: " << WEXITSTATUS(status) << RESET << std::endl;
    }
}

std::string prompt(const std::string &prompt_text) {
    std::cout << BLUE << prompt_text << RESET;
    std::string input;
    std::getline(std::cin, input);
    return input;
}

void run_sudo_command(const std::string &command, const std::string &password) {
    std::cout << BLUE << "Running command: " << command << RESET << std::endl;
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
        std::string full_command = "sudo -S " + command;
        execl("/bin/sh", "sh", "-c", full_command.c_str(), (char *)NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    } else {
        close(pipefd[0]);
        write(pipefd[1], password.c_str(), password.length());
        write(pipefd[1], "\n", 1);
        close(pipefd[1]);
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0) {
                std::cout << RED << "Command failed with exit code " << exit_code << "." << RESET << std::endl;
            }
        }
    }
}

bool dir_exists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string get_kernel_version() {
    std::string version = "unknown";
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

void print_banner(const std::string& distro_name) {
    std::cout << RED;
    std::cout <<
    "░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n"
    "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n"
    "██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n"
    "██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n"
    "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n"
    "░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░\n";
        std::cout << RESET;
        std::cout << RED << "Claudemods Multi Iso Creator Advanced C++ Script v2.0 22-06-2025" << RESET << std::endl;

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char datetime[50];
        strftime(datetime, sizeof(datetime), "%d/%m/%Y %H:%M:%S", t);
        std::cout << GREEN << "Current UK Time: " << datetime << RESET << std::endl;

        std::cout << GREEN << "Disk Usage:" << RESET << std::endl;
        std::string cmd = "df -h /";
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                std::cout << buffer.data();
            }
        }
}

int show_menu(const std::string &title, const std::vector<std::string> &items, int selected, const std::string& distro_name = "Arch") {
    system("clear");
    print_banner(distro_name);
    std::cout << COLOR_CYAN << "  " << title << RESET << std::endl;
    std::cout << COLOR_CYAN << "  " << std::string(title.length(), '-') << RESET << std::endl;
    for (size_t i = 0; i < items.size(); i++) {
        if (i == static_cast<size_t>(selected)) {
            std::cout << COLOR_GOLD << "➤ " << items[i] << RESET << "\n";
        } else {
            std::cout << COLOR_CYAN << "  " << items[i] << RESET << "\n";
        }
    }
    return get_key();
}

// ============ ARCH FUNCTIONS ============
void install_dependencies_arch() {
    progress_dialog("Installing dependencies...");
    const std::string packages =
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
    std::string command = "sudo pacman -S --needed --noconfirm " + packages;
    run_command(command);
    message_box("Success", "Dependencies installed successfully.");
}

void copy_vmlinuz_arch() {
    std::string kernel_version = get_kernel_version();
    std::string command = "sudo cp /boot/vmlinuz-" + kernel_version + " /home/$USER/.config/cmi/build-image-arch/live/";
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

// ============ UBUNTU FUNCTIONS ============
void install_dependencies_ubuntu() {
    progress_dialog("Installing Ubuntu dependencies...");
    const std::string packages =
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

    std::string command = "sudo apt install " + packages;
    run_command(command);
    message_box("Success", "Ubuntu dependencies installed successfully.");
}

void copy_vmlinuz_ubuntu() {
    std::string kernel_version = get_kernel_version();
    std::string command = "sudo cp /boot/vmlinuz-" + kernel_version + " /home/$USER/.config/cmi/build-image-noble/live/";
    run_command(command);
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
    std::string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }
    std::string command = "sudo mksquashfs " + clone_dir + " /home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs "
    "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs";
    std::cout << "Creating Ubuntu SquashFS image from: " << clone_dir << std::endl;
    run_command(command);
}

void delete_clone_system_temp_ubuntu() {
    std::string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }
    std::string command = "sudo rm -rf " + clone_dir;
    std::cout << "Deleting temporary clone directory: " << clone_dir << std::endl;
    run_command(command);
    struct stat st;
    if (stat("filesystem.squashfs", &st) == 0) {
        command = "sudo rm -f /home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs";
        std::cout << "Deleting SquashFS image: filesystem.squashfs" << std::endl;
        run_command(command);
    } else {
        std::cout << "SquashFS image does not exist: filesystem.squashfs" << std::endl;
    }
}

// ============ DEBIAN FUNCTIONS ============
void install_dependencies_debian() {
    progress_dialog("Installing Debian dependencies...");
    const std::string packages =
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

    std::string command = "sudo apt install " + packages;
    run_command(command);
    message_box("Success", "Debian dependencies installed successfully.");
}

void copy_vmlinuz_debian() {
    std::string kernel_version = get_kernel_version();
    std::string command = "sudo cp /boot/vmlinuz-" + kernel_version + " /home/$USER/.config/cmi/build-image-debian/live/";
    run_command(command);
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
    std::string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }
    std::string command = "sudo mksquashfs " + clone_dir + " /home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs "
    "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs";
    std::cout << "Creating Debian SquashFS image from: " << clone_dir << std::endl;
    run_command(command);
}

void delete_clone_system_temp_debian() {
    std::string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }
    std::string command = "sudo rm -rf " + clone_dir;
    std::cout << "Deleting temporary clone directory: " << clone_dir << std::endl;
    run_command(command);
    struct stat st;
    if (stat("filesystem.squashfs", &st) == 0) {
        command = "sudo rm -f /home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs";
        std::cout << "Deleting SquashFS image: filesystem.squashfs" << std::endl;
        run_command(command);
    } else {
        std::cout << "SquashFS image does not exist: filesystem.squashfs" << std::endl;
    }
}

// ============ COMMON FUNCTIONS ============
void save_clone_dir(const std::string &dir_path) {
    std::string config_dir = "/home/" + std::string(getenv("USER")) + "/.config/cmi";
    if (!dir_exists(config_dir)) {
        std::string mkdir_cmd = "mkdir -p " + config_dir;
        run_command(mkdir_cmd);
    }
    std::string file_path = config_dir + "/clonedir.txt";
    std::ofstream f(file_path);
    if (!f) {
        perror("Failed to open clonedir.txt");
        return;
    }
    f << dir_path;
    f.close();
    message_box("Success", "Clone directory path saved successfully.");
}

std::string read_clone_dir() {
    std::string file_path = "/home/" + std::string(getenv("USER")) + "/.config/cmi/clonedir.txt";
    std::ifstream f(file_path);
    if (!f) {
        return "";
    }
    std::string dir_path;
    std::getline(f, dir_path);
    return dir_path;
}

void clone_system(const std::string &clone_dir) {
    if (dir_exists(clone_dir)) {
        std::cout << "Skipping removal of existing clone directory: " << clone_dir << std::endl;
    } else {
        mkdir(clone_dir.c_str(), 0755);
    }
    std::string command = "sudo rsync -aHAxSr --numeric-ids --info=progress2 "
    "--include=dev --include=usr --include=proc --include=tmp --include=sys "
    "--include=run --include=media "
    "--exclude=dev/* --exclude=proc/* --exclude=tmp/* --exclude=sys/* "
    "--exclude=run/* --exclude=media/* --exclude=" + clone_dir + " "
    "/ " + clone_dir;
    std::cout << "Cloning system directory to: " << clone_dir << std::endl;
    run_command(command);
}

void create_squashfs_image() {
    std::string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }
    std::string command = "sudo mksquashfs " + clone_dir + " /home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs "
    "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs";
    std::cout << "Creating SquashFS image from: " << clone_dir << std::endl;
    run_command(command);
}

void delete_clone_system_temp() {
    std::string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        return;
    }
    std::string command = "sudo rm -rf " + clone_dir;
    std::cout << "Deleting temporary clone directory: " << clone_dir << std::endl;
    run_command(command);
    struct stat st;
    if (stat("filesystem.squashfs", &st) == 0) {
        command = "sudo rm -f /home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs";
        std::cout << "Deleting SquashFS image: filesystem.squashfs" << std::endl;
        run_command(command);
    } else {
        std::cout << "SquashFS image does not exist: filesystem.squashfs" << std::endl;
    }
}

void set_clone_directory() {
    std::string dir_path = prompt("Enter full path for clone_system_temp directory: ");
    if (dir_path.empty()) {
        error_box("Error", "Directory path cannot be empty");
        return;
    }
    save_clone_dir(dir_path);
}

void install_one_time_updater() {
    progress_dialog("Installing one-time updater...");
    const std::vector<std::string> commands = {
        "cd /home/$USER",
        "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git  >/dev/null 2>&1",
        "mkdir -p /home/$USER/.config/cmi >/dev/null 2>&1",
        "cp /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/updatermain/advancedcscriptupdater /home/$USER/.config/cmi >/dev/null 2>&1",
        "cp /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/installer/patch.sh /home/$USER/.config/cmi >/dev/null 2>&1",
        "chmod +x /home/$USER/.config/cmi/patch.sh >/dev/null 2>&1",
        "chmod +x /home/$USER/.config/cmi/advancedcscriptupdater >/dev/null 2>&1",
        "rm -rf /home/$USER/claudemods-multi-iso-konsole-script >/dev/null 2>&1"
    };
    for (const auto &cmd : commands) {
        run_command(cmd);
    }
    message_box("Success", "One-time updater installed successfully in /home/$USER/.config/cmi");
}

void squashfs_menu(const std::string& distro_name = "Arch") {
    std::vector<std::string> items = {
        "Max compression (xz)",
        "Create SquashFS from clone directory",
        "Delete clone directory and SquashFS image",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;
    while (true) {
        key = show_menu("SquashFS Creator", items, selected, distro_name);
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
                        std::string clone_dir = read_clone_dir();
                        if (clone_dir.empty()) {
                            error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
                            break;
                        }
                        if (!dir_exists(clone_dir)) {
                            clone_system(clone_dir);
                        }
                        if (distro_name == "ubuntu") {
                            create_squashfs_image_ubuntu();
                        } else if (distro_name == "debian") {
                            create_squashfs_image_debian();
                        } else {
                            create_squashfs_image();
                        }
                    }
                    break;
                    case 1:
                        if (distro_name == "ubuntu") {
                            create_squashfs_image_ubuntu();
                        } else if (distro_name == "debian") {
                            create_squashfs_image_debian();
                        } else {
                            create_squashfs_image();
                        }
                        break;
                    case 2:
                        if (distro_name == "ubuntu") {
                            delete_clone_system_temp_ubuntu();
                        } else if (distro_name == "debian") {
                            delete_clone_system_temp_debian();
                        } else {
                            delete_clone_system_temp();
                        }
                        break;
                    case 3:
                        return;
                }
                std::cout << "\nPress Enter to continue...";
                while (getchar() != '\n');
                break;
        }
    }
}

void create_iso(const std::string& distro_name = "Arch") {
    std::string iso_name = prompt("What do you want to name your .iso? ");
    if (iso_name.empty()) {
        error_box("Input Error", "ISO name cannot be empty.");
        return;
    }
    std::string output_dir = prompt("Enter the output directory path (or press Enter for current directory): ");
    if (output_dir.empty()) {
        output_dir = ".";
    }
    char application_dir_path[MAX_PATH];
    if (getcwd(application_dir_path, sizeof(application_dir_path)) == NULL) {
        perror("getcwd");
        return;
    }

    std::string build_image_dir;
    if (distro_name == "ubuntu") {
        build_image_dir = std::string(application_dir_path) + "/build-image-noble";
    } else if (distro_name == "debian") {
        build_image_dir = std::string(application_dir_path) + "/build-image-debian";
    } else {
        build_image_dir = std::string(application_dir_path) + "/build-image-arch";
    }

    if (build_image_dir.length() >= MAX_PATH) {
        error_box("Error", "Path too long for build directory");
        return;
    }
    struct stat st;
    if (stat(output_dir.c_str(), &st) == -1) {
        if (mkdir(output_dir.c_str(), 0755) == -1) {
            perror("mkdir");
            return;
        }
    }
    progress_dialog("Creating ISO...");
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H%M", t);
    std::string iso_file_name = output_dir + "/" + iso_name + "_amd64_" + timestamp + ".iso";
    if (iso_file_name.length() >= MAX_PATH) {
        error_box("Error", "Path too long for ISO filename");
        return;
    }

    std::string xorriso_command;
    if (distro_name == "ubuntu" || distro_name == "debian") {
        xorriso_command = "sudo xorriso -as mkisofs -o \"" + iso_file_name + "\" -V 2025 -iso-level 3 "
        "-isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin "
        "-c isolinux/boot.cat -b isolinux/isolinux.bin "
        "-no-emul-boot -boot-load-size 4 -boot-info-table "
        "-eltorito-alt-boot -e boot/grub/efi.img "
        "-no-emul-boot -isohybrid-gpt-basdat \"" + build_image_dir + "\"";
    } else {
        xorriso_command = "sudo xorriso -as mkisofs -o \"" + iso_file_name + "\" -V 2025 -iso-level 3 "
        "-isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin -c isolinux/boot.cat "
        "-b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table "
        "-eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat \"" + build_image_dir + "\"";
    }

    if (xorriso_command.length() >= MAX_CMD) {
        error_box("Error", "Command too long for buffer");
        return;
    }
    std::string sudo_password = prompt("Enter your sudo password: ");
    if (sudo_password.empty()) {
        error_box("Input Error", "Sudo password cannot be empty.");
        return;
    }
    run_sudo_command(xorriso_command, sudo_password);
    message_box("Success", "ISO creation completed.");
    std::string choice = prompt("Press 'm' to go back to main menu or Enter to exit: ");
    if (!choice.empty() && (choice[0] == 'm' || choice[0] == 'M')) {
        run_command("ruby /opt/claudemods-iso-konsole-script/demo.rb");
    }
}

void run_iso_in_qemu() {
    const std::string qemu_script = "/opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb";
    std::string command = "ruby " + qemu_script;
    run_command(command);
}

void iso_creator_menu(const std::string& distro_name = "Arch") {
    std::vector<std::string> items = {
        "Create ISO",
        "Run ISO in QEMU",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;
    while (true) {
        key = show_menu("ISO Creator Menu", items, selected, distro_name);
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
    std::string sudo_password = prompt("Enter sudo password to create command files: ");
    if (sudo_password.empty()) {
        error_box("Error", "Sudo password cannot be empty");
        return;
    }
    // gen-init
    std::string command = "sudo bash -c 'cat > /usr/bin/gen-init << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: gen-init\"\n"
    "  echo \"Generate initcpio configuration\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + std::string(exe_path) + " 5\n"
    "EOF\n"
    "chmod 755 /usr/bin/gen-init'";
    run_sudo_command(command, sudo_password);
    // edit-isocfg
    command = "sudo bash -c 'cat > /usr/bin/edit-isocfg << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-isocfg\"\n"
    "  echo \"Edit isolinux.cfg file\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + std::string(exe_path) + " 6\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-isocfg'";
    run_sudo_command(command, sudo_password);
    // edit-grubcfg
    command = "sudo bash -c 'cat > /usr/bin/edit-grubcfg << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-grubcfg\"\n"
    "  echo \"Edit grub.cfg file\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + std::string(exe_path) + " 7\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-grubcfg'";
    run_sudo_command(command, sudo_password);
    // setup-script
    command = "sudo bash -c 'cat > /usr/bin/setup-script << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: setup-script\"\n"
    "  echo \"Open setup script menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + std::string(exe_path) + " 8\n"
    "EOF\n"
    "chmod 755 /usr/bin/setup-script'";
    run_sudo_command(command, sudo_password);
    // make-iso
    command = "sudo bash -c 'cat > /usr/bin/make-iso << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: make-iso\"\n"
    "  echo \"Launches the ISO creation menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + std::string(exe_path) + " 3\n"
    "EOF\n"
    "chmod 755 /usr/bin/make-iso'";
    run_sudo_command(command, sudo_password);
    // make-squashfs
    command = "sudo bash -c 'cat > /usr/bin/make-squashfs << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: make-squashfs\"\n"
    "  echo \"Launches the SquashFS creation menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + std::string(exe_path) + " 4\n"
    "EOF\n"
    "chmod 755 /usr/bin/make-squashfs'";
    run_sudo_command(command, sudo_password);
    std::cout << GREEN << "Activated! You can now use all commands in your terminal." << RESET << std::endl;
}

void remove_command_files() {
    run_command("sudo rm -f /usr/bin/gen-init");
    run_command("sudo rm -f /usr/bin/edit-isocfg");
    run_command("sudo rm -f /usr/bin/edit-grubcfg");
    run_command("sudo rm -f /usr/bin/setup-script");
    run_command("sudo rm -f /usr/bin/make-iso");
    run_command("sudo rm -f /usr/bin/make-squashfs");
    std::cout << GREEN << "Commands deactivated and removed from system." << RESET << std::endl;
}

void command_installer_menu(const std::string& distro_name = "Arch") {
    std::vector<std::string> items = {
        "Activate terminal commands",
        "Deactivate terminal commands",
        "Back to Main Menu"
    };
    int selected = 0;
    int key;
    while (true) {
        key = show_menu("Command Installer Menu", items, selected, distro_name);
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
                std::cout << "\nPress Enter to continue...";
                while (getchar() != '\n');
                break;
        }
    }
}

void setup_script_menu() {
    Distro distro = detect_distro();
    std::string distro_name;

    switch(distro) {
        case ARCH:
            distro_name = "Arch";
            {
                std::vector<std::string> items = {
                    "Generate initcpio configuration (arch)",
                    "Edit isolinux.cfg (arch)",
                    "Edit grub.cfg (arch)",
                    "Set clone directory path",
                    "Install One Time Updater",
                    "Back to Main Menu"
                };
                int selected = 0;
                int key;
                while (true) {
                    key = show_menu("Setup Script Menu", items, selected, distro_name);
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
                            std::cout << "\nPress Enter to continue...";
                            while (getchar() != '\n');
                            break;
                    }
                }
            }
            break;

                                case UBUNTU:
                                    distro_name = "Ubuntu";
                                    {
                                        std::vector<std::string> items = {
                                            "Generate initcpio configuration (ubuntu)",
                                            "Edit isolinux.cfg (ubuntu)",
                                            "Edit grub.cfg (ubuntu)",
                                            "Set clone directory path",
                                            "Install One Time Updater",
                                            "Back to Main Menu"
                                        };
                                        int selected = 0;
                                        int key;
                                        while (true) {
                                            key = show_menu("Setup Script Menu", items, selected, distro_name);
                                            switch (key) {
                                                case 'A':
                                                    if (selected > 0) selected--;
                                                    break;
                                                case 'B':
                                                    if (selected < 5) selected++;
                                                    break;
                                                case '\n':
                                                    switch (selected) {
                                                        case 0: generate_initrd_ubuntu(); break;
                                                        case 1: edit_isolinux_cfg_ubuntu(); break;
                                                        case 2: edit_grub_cfg_ubuntu(); break;
                                                        case 3: set_clone_directory(); break;
                                                        case 4: install_one_time_updater(); break;
                                                        case 5: return;
                                                    }
                                                    std::cout << "\nPress Enter to continue...";
                                                    while (getchar() != '\n');
                                                    break;
                                            }
                                        }
                                    }
                                    break;

                                                        case DEBIAN:
                                                            distro_name = "Debian";
                                                            {
                                                                std::vector<std::string> items = {
                                                                    "Generate initcpio configuration (debian)",
                                                                    "Edit isolinux.cfg (debian)",
                                                                    "Edit grub.cfg (debian)",
                                                                    "Set clone directory path",
                                                                    "Install One Time Updater",
                                                                    "Back to Main Menu"
                                                                };
                                                                int selected = 0;
                                                                int key;
                                                                while (true) {
                                                                    key = show_menu("Setup Script Menu", items, selected, distro_name);
                                                                    switch (key) {
                                                                        case 'A':
                                                                            if (selected > 0) selected--;
                                                                            break;
                                                                        case 'B':
                                                                            if (selected < 5) selected++;
                                                                            break;
                                                                        case '\n':
                                                                            switch (selected) {
                                                                                case 0: generate_initrd_debian(); break;
                                                                                case 1: edit_isolinux_cfg_debian(); break;
                                                                                case 2: edit_grub_cfg_debian(); break;
                                                                                case 3: set_clone_directory(); break;
                                                                                case 4: install_one_time_updater(); break;
                                                                                case 5: return;
                                                                            }
                                                                            std::cout << "\nPress Enter to continue...";
                                                                            while (getchar() != '\n');
                                                                            break;
                                                                    }
                                                                }
                                                            }
                                                            break;

                                                                                case UNKNOWN:
                                                                                    error_box("Error", "Unsupported Linux distribution");
                                                                                    break;
    }
}

int main(int argc, char *argv[]) {
    tcgetattr(STDIN_FILENO, &original_term);
    enable_raw_mode();

    Distro distro = detect_distro();
    std::string distro_name;
    switch(distro) {
        case ARCH: distro_name = "Arch"; break;
        case UBUNTU: distro_name = "Ubuntu"; break;
        case DEBIAN: distro_name = "Debian"; break;
        default: distro_name = "Linux";
    }

    if (argc > 1) {
        if (strcmp(argv[1], "3") == 0) {
            iso_creator_menu(distro_name);
            disable_raw_mode();
            return 0;
        } else if (strcmp(argv[1], "4") == 0) {
            squashfs_menu(distro_name);
            disable_raw_mode();
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
            return 0;
        } else if (strcmp(argv[1], "8") == 0) {
            setup_script_menu();
            disable_raw_mode();
            return 0;
        }
    }

    std::vector<std::string> items = {
        "SquashFS Creator",
        "ISO Creator",
        "Setup Script",
        "Command Installer",
        "Exit"
    };
    int selected = 0;
    int key;
    while (true) {
        key = show_menu("Main Menu", items, selected, distro_name);
        switch (key) {
            case 'A':
                if (selected > 0) selected--;
                break;
            case 'B':
                if (selected < 4) selected++;
                break;
            case '\n':
                switch (selected) {
                    case 0: squashfs_menu(distro_name); break;
                    case 1: iso_creator_menu(distro_name); break;
                    case 2: setup_script_menu(); break;
                    case 3: command_installer_menu(distro_name); break;
                    case 4:
                        disable_raw_mode();
                        return 0;
                }
                break;
        }
    }
}
