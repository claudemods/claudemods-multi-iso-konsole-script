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
enum Distro { ARCH, CACHYOS, UNKNOWN };
atomic<bool> time_thread_running(true);
mutex time_mutex;
string current_time_str;
bool should_reset = false;

string get_kernel_version();
string read_clone_dir();
bool dir_exists(const string &path);
bool execute_command(const string& cmd);
Distro detect_distro();
string get_distro_name(Distro distro);
void edit_calamares_branding();
bool is_init_generated(Distro distro);
string get_clone_dir_status();
string get_init_status(Distro distro);
string get_iso_name();
string get_iso_name_status();
void message_box(const string &title, const string &message);
void error_box(const string &title, const string &message);
void progress_dialog(const string &message);
void enable_raw_mode();
void disable_raw_mode();
string get_highlight_color(Distro distro);
string get_input(const string &prompt_text, bool echo = true);
string prompt(const string &prompt_text);
string password_prompt(const string &prompt_text);
void update_time_thread();
void print_banner(Distro distro);
int get_key();
void print_blue(const string &text);
void install_dependencies_arch();
void generate_initrd_arch();
void edit_grub_cfg_arch();
void edit_isolinux_cfg_arch();
void install_calamares_arch();
void install_dependencies_cachyos();
void install_calamares_cachyos();
void clone_system(const string &clone_dir);
void create_squashfs_image(Distro distro);
void delete_clone_system_temp(Distro distro);
void set_clone_directory();
void install_one_time_updater();
void create_iso(Distro distro);
void run_iso_in_qemu();
void create_command_files();
void remove_command_files();
void save_iso_name(const string &name);
void set_iso_name();

string get_input(const string &prompt_text, bool echo) {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    if (echo) {
        newt.c_lflag |= (ICANON | ECHO);
    } else {
        newt.c_lflag &= ~(ECHO);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    cout << GREEN << prompt_text << RESET;
    string input;
    getline(cin, input);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return input;
}

bool is_init_generated(Distro) {
    string init_path = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-arch/live/initramfs-linux.img";
    struct stat st;
    return stat(init_path.c_str(), &st) == 0;
}

string get_clone_dir_status() {
    string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        return RED + string("✗ Clone directory not set") + RESET;
    } else {
        return GREEN + string("✓ Clone directory: ") + clone_dir + RESET;
    }
}

string get_init_status(Distro distro) {
    if (is_init_generated(distro)) {
        return GREEN + string("✓ Initramfs generated") + RESET;
    } else {
        return RED + string("✗ Initramfs not generated") + RESET;
    }
}

string get_iso_name() {
    string file_path = "/home/" + string(getenv("USER")) + "/.config/cmi/isoname.txt";
    ifstream f(file_path);
    if (!f) return "";

    string name;
    getline(f, name);
    return name;
}

string get_iso_name_status() {
    string iso_name = get_iso_name();
    if (iso_name.empty()) {
        return RED + string("✗ ISO name not set") + RESET;
    } else {
        return GREEN + string("✓ ISO name: ") + iso_name + RESET;
    }
}

bool dir_exists(const string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void message_box(const string &title, const string &message) {
    cout << GREEN << title << RESET << endl;
    cout << GREEN << message << RESET << endl;
}

void error_box(const string &title, const string &message) {
    cout << RED << title << RESET << endl;
    cout << RED << message << RESET << endl;
}

void progress_dialog(const string &message) {
    cout << GREEN << message << RESET << endl;
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

bool execute_command(const string& cmd) {
    cout << COLOR_CYAN;
    fflush(stdout);
    string full_cmd = " " + cmd;
    int status = system(full_cmd.c_str());
    cout << COLOR_RESET;
    if (status != 0) {
        cerr << RED << "Command failed (continuing anyway): " << full_cmd << RESET << endl;
        return false;
    }
    return true;
}

Distro detect_distro() {
    ifstream os_release("/etc/os-release");
    if (!os_release.is_open()) return UNKNOWN;

    string line;
    while (getline(os_release, line)) {
        if (line.find("ID=") == 0) {
            if (line.find("arch") != string::npos) return ARCH;
            if (line.find("cachyos") != string::npos) return CACHYOS;
        }
    }
    return UNKNOWN;
}

string get_highlight_color(Distro distro) {
    switch(distro) {
        case ARCH: return "\033[34m";
        case CACHYOS: return "\033[34m";
        default: return "\033[36m";
    }
}

string get_distro_name(Distro distro) {
    switch(distro) {
        case ARCH: return "Arch";
        case CACHYOS: return "CachyOS";
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

    for (int i = 5; i > 0; i--) {
        sleep(1);
    }
    should_reset = true;
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
    should_reset = true;
}

void set_iso_name() {
    string name = prompt("Enter ISO name (without extension): ");
    if (name.empty()) {
        error_box("Error", "ISO name cannot be empty");
        return;
    }
    save_iso_name(name);
}

void print_banner(Distro distro) {
    cout << RED;
    cout <<
    "░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n"
    "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n"
    "██║░░╚═╝██║░░░░░██║░░██║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n"
    "██║░░██╗██║░░░░░██║░░██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n"
    "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n"
    "░╚════╝░╚══════╝╚═╝░░╚═╝░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░\n";
        cout << RESET;
        cout << RED << "Claudemods Multi Iso Creator Advanced C++ Script v2.0 DevBranch 23-07-2025" << RESET << endl;

        cout << GREEN << "Current Distribution: " << get_distro_name(distro) << RESET << endl;
        cout << GREEN << "Current Kernel: " << get_kernel_version() << RESET << endl;

        {
            lock_guard<mutex> lock(time_mutex);
            cout << GREEN << "Current UK Time: " << current_time_str << RESET << endl;
        }

        cout << get_clone_dir_status() << endl;
        cout << get_init_status(distro) << endl;
        cout << get_iso_name_status() << endl;

        cout << GREEN << "Disk Usage:" << RESET << endl;
        string cmd = "df -h /";
        unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (pipe) {
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
                cout << GREEN << buffer << RESET;
            }
        }
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

void print_blue(const string &text) {
    cout << BLUE << text << RESET << endl;
}

string prompt(const string &prompt_text) {
    return get_input(prompt_text, true);
}

string password_prompt(const string &prompt_text) {
    return get_input(prompt_text, false);
}

void update_time_thread() {
    while (time_thread_running) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char datetime[50];
        strftime(datetime, sizeof(datetime), "%d/%m/%Y %H:%M:%S", t);

        {
            lock_guard<mutex> lock(time_mutex);
            current_time_str = datetime;
        }

        sleep(1);
    }
}

void install_dependencies_arch() {
    progress_dialog("Installing dependencies...");
    const string packages =
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
    execute_command("sudo pacman -S --needed --noconfirm " + packages);
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
    const string packages =
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
    execute_command("sudo pacman -S --needed --noconfirm " + packages);
    message_box("Success", "Dependencies installed successfully.");
}

void install_calamares_cachyos() {
    progress_dialog("Installing Calamares for CachyOS...");
    execute_command("sudo pacman -U --noconfirm calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar ckbcomp-1.227-2-any.pkg.tar.zst");
    message_box("Success", "Calamares installed successfully for CachyOS.");
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

    string parent_dir = clone_dir;
    if (parent_dir.back() == '/') {
        parent_dir.pop_back();
    }
    size_t last_slash = parent_dir.find_last_of('/');
    if (last_slash != string::npos) {
        parent_dir = parent_dir.substr(last_slash + 1);
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

    cout << GREEN << "Cloning system into directory: " << full_clone_path << RESET << endl;
    execute_command(command);
}

void create_squashfs_image(Distro) {
    string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        error_box("Error", "No clone directory specified.");
        return;
    }

    string full_clone_path = clone_dir;
    if (full_clone_path.back() != '/') {
        full_clone_path += '/';
    }
    full_clone_path += "clone_system_temp";

    string output_path = "/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs";

    string command = "sudo mksquashfs " + full_clone_path + " " + output_path + " "
    "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
    "-always-use-fragments -wildcards -xattrs";

    cout << GREEN << "Creating SquashFS image from: " << full_clone_path << RESET << endl;
    execute_command(command);

    string del_cmd = "sudo rm -rf " + full_clone_path;
    execute_command(del_cmd);
}

void delete_clone_system_temp(Distro) {
    string clone_dir = read_clone_dir();
    if (clone_dir.empty()) {
        error_box("Error", "No clone directory specified.");
        return;
    }

    string full_clone_path = clone_dir;
    if (full_clone_path.back() != '/') {
        full_clone_path += '/';
    }
    full_clone_path += "clone_system_temp";

    string command = "sudo rm -rf " + full_clone_path;
    cout << GREEN << "Deleting clone directory: " << full_clone_path << RESET << endl;
    execute_command(command);

    string squashfs_path = "/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs";

    struct stat st;
    if (stat(squashfs_path.c_str(), &st) == 0) {
        command = "sudo rm -f " + squashfs_path;
        cout << GREEN << "Deleting SquashFS image: " << squashfs_path << RESET << endl;
        execute_command(command);
    } else {
        cout << GREEN << "SquashFS image does not exist: " << squashfs_path << RESET << endl;
    }
}

void set_clone_directory() {
    string dir_path = prompt("Enter full path for clone directory e.g /home/$USER/Pictures ");
    if (dir_path.empty()) {
        error_box("Error", "Directory path cannot be empty");
        return;
    }

    if (dir_path.back() != '/') {
        dir_path += '/';
    }

    save_clone_dir(dir_path);
}

void install_one_time_updater() {
    progress_dialog("Installing one-time updater...");
    execute_command("./home/$USER/.config/cmi/patch.sh");
    message_box("Success", "One-time updater installed successfully in /home/$USER/.config/cmi");
}

void create_iso(Distro) {
    string iso_name = get_iso_name();
    if (iso_name.empty()) {
        error_box("Error", "ISO name not set.");
        return;
    }

    string output_dir = prompt("Enter the output directory path: ");
    if (output_dir.empty()) {
        error_box("Input Error", "Output directory cannot be empty.");
        return;
    }

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H%M", t);

    std::ostringstream oss;
    oss << output_dir << std::filesystem::path::preferred_separator
    << iso_name << "_amd64_" << timestamp << ".iso";
    string iso_file_name = oss.str();

    string build_image_dir = "/home/" + string(getenv("USER")) + "/.config/cmi/build-image-arch";

    if (!dir_exists(output_dir)) {
        execute_command("mkdir -p " + output_dir);
    }

    oss.str("");
    oss << "sudo xorriso -as mkisofs -o " << iso_file_name
    << " -V 2025 -iso-level 3"
    << " -isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin"
    << " -c isolinux/boot.cat"
    << " -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table"
    << " -eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat"
    << " " << build_image_dir;

    string xorriso_command = oss.str();
    execute_command(xorriso_command);

    message_box("Success", "ISO creation completed.");
}

void run_iso_in_qemu() {
    execute_command("ruby /opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb");
}

void create_command_files() {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len == -1) {
        perror("readlink");
        return;
    }
    exe_path[len] = '\0';
    string command = "bash -c 'cat > /usr/bin/gen-init << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: gen-init\"\n"
    "  echo \"Generate initcpio configuration\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 5\n"
    "EOF\n"
    "chmod 755 /usr/bin/gen-init'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/edit-isocfg << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-isocfg\"\n"
    "  echo \"Edit isolinux.cfg file\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 6\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-isocfg'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/edit-grubcfg << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-grubcfg\"\n"
    "  echo \"Edit grub.cfg file\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 7\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-grubcfg'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/make-iso << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: make-iso\"\n"
    "  echo \"Launches the ISO creation menu\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 3\n"
    "EOF\n"
    "chmod 755 /usr/bin/make-iso'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/gen-calamares << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: gen-calamares\"\n"
    "  echo \"Install Calamares installer\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 9\n"
    "EOF\n"
    "chmod 755 /usr/bin/gen-calamares'";
    execute_command("sudo " + command);

    command = "bash -c 'cat > /usr/bin/edit-branding << \"EOF\"\n"
    "#!/bin/sh\n"
    "if [ \"$1\" = \"--help\" ]; then\n"
    "  echo \"Usage: edit-branding\"\n"
    "  echo \"Edit Calamares branding\"\n"
    "  exit 0\n"
    "fi\n"
    "exec " + string(exe_path) + " 10\n"
    "EOF\n"
    "chmod 755 /usr/bin/edit-branding'";
    execute_command("sudo " + command);

    cout << GREEN << "Activated! You can now use all commands in your terminal." << RESET << endl;
}

void remove_command_files() {
    execute_command("sudo rm -f /usr/bin/gen-init");
    execute_command("sudo rm -f /usr/bin/edit-isocfg");
    execute_command("sudo rm -f /usr/bin/edit-grubcfg");
    execute_command("sudo rm -f /usr/bin/make-iso");
    execute_command("sudo rm -f /usr/bin/gen-calamares");
    execute_command("sudo rm -f /usr/bin/edit-branding");
    cout << GREEN << "Commands deactivated and removed from system." << RESET << endl;
}

void automatic_workflow(Distro distro) {
    // 1. Install dependencies
    if (distro == ARCH) {
        install_dependencies_arch();
    } else if (distro == CACHYOS) {
        install_dependencies_cachyos();
    }

    // 2. Set clone directory
    set_clone_directory();

    // 3. Set ISO name
    set_iso_name();

    // 4. Generate initramfs
    generate_initrd_arch();

    // 5. Clone system
    string clone_dir = read_clone_dir();
    if (!clone_dir.empty()) {
        clone_system(clone_dir);
    }

    // 6. Create SquashFS image
    create_squashfs_image(distro);

    // 7. Install Calamares
    if (distro == ARCH) {
        install_calamares_arch();
    } else if (distro == CACHYOS) {
        install_calamares_cachyos();
    }

    // 8. Create ISO
    create_iso(distro);

    // 9. Create command files
    create_command_files();

    message_box("Complete", "All operations completed successfully!");
}

int main(int, char**) {
    tcgetattr(STDIN_FILENO, &original_term);
    thread time_thread(update_time_thread);

    Distro distro = detect_distro();
    if (distro == UNKNOWN) {
        cerr << RED << "Error: Unsupported distribution" << RESET << endl;
        time_thread_running = false;
        time_thread.join();
        return 1;
    }

    // Run automatic workflow
    automatic_workflow(distro);

    time_thread_running = false;
    time_thread.join();
    return 0;
}
