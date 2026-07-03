#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <dirent.h>
using namespace std;

#define COLOR_CYAN "\033[38;2;0;255;255m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET "\033[0m"

string exec(const char* cmd) {
    char buffer[128];
    string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw runtime_error("popen() failed!");
    while (fgets(buffer, sizeof buffer, pipe) != NULL) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

void execute_command(const string& cmd) {
    cout << COLOR_CYAN;
    fflush(stdout);
    string full_cmd = "sudo " + cmd;
    int status = system(full_cmd.c_str());
    cout << COLOR_RESET;
    if (status != 0) {
        cerr << COLOR_RED << "Error executing: " << full_cmd << COLOR_RESET << endl;
        exit(1);
    }
}

bool is_block_device(const string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) != 0) return false;
    return S_ISBLK(statbuf.st_mode);
}

bool directory_exists(const string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) != 0) return false;
    return S_ISDIR(statbuf.st_mode);
}

string get_uk_date_time() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%d-%m-%Y_%H:%M", ltm);
    string time_str(buffer);
    int hour = ltm->tm_hour;
    string ampm = (hour < 12) ? "am" : "pm";
    if (hour > 12) hour -= 12;
    if (hour == 0) hour = 12;
    char time_buffer[80];
    strftime(time_buffer, sizeof(time_buffer), "%d-%m-%Y_%I:%M", ltm);
    string result(time_buffer);
    result += ampm;
    return result;
}

void display_header() {
    cout << COLOR_RED;
    cout << R"(
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
)" << endl;
cout << COLOR_CYAN << "claudemods cmi advanced installer v1.02" << COLOR_RESET << endl;
cout << COLOR_CYAN << "Supports Btrfs and Ext4 filesystems (squashfs/erofs.img)" << COLOR_RESET << endl << endl;
}

void prepare_target_partitions(const string& drive, const string& fs_type) {
    execute_command("umount -f " + drive + "* 2>/dev/null || true");
    execute_command("wipefs -a " + drive);
    execute_command("parted -s " + drive + " mklabel gpt");
    execute_command("parted -s " + drive + " mkpart primary fat32 1MiB 551MiB");
    execute_command("parted -s " + drive + " mkpart primary " + fs_type + " 551MiB 100%");
    execute_command("parted -s " + drive + " set 1 esp on");
    execute_command("partprobe " + drive);
    sleep(2);

    string efi_part = drive + "1";
    string root_part = drive + "2";

    if (!is_block_device(efi_part) || !is_block_device(root_part)) {
        cerr << COLOR_RED << "Error: Failed to create partitions" << COLOR_RESET << endl;
        exit(1);
    }

    execute_command("mkfs.vfat -F32 " + efi_part);
    if (fs_type == "btrfs") {
        execute_command("mkfs.btrfs -f -L ROOT " + root_part);
    } else {
        execute_command("mkfs.ext4 -F -L ROOT " + root_part);
    }
}

void setup_btrfs_subvolumes(const string& root_part) {
    // Mount root partition temporarily to create subvolumes
    execute_command("mount " + root_part + " /mnt");

    // Create subvolumes with compression enabled
    execute_command("btrfs subvolume create /mnt/@");
    execute_command("btrfs subvolume create /mnt/@home");
    execute_command("btrfs subvolume create /mnt/@root");
    execute_command("btrfs subvolume create /mnt/@srv");
    execute_command("btrfs subvolume create /mnt/@cache");
    execute_command("btrfs subvolume create /mnt/@tmp");
    execute_command("btrfs subvolume create /mnt/@log");
    execute_command("mkdir -p /mnt/@/var/lib");
    execute_command("btrfs subvolume create /mnt/@/var/lib/portables");
    execute_command("btrfs subvolume create /mnt/@/var/lib/machines");

    // Unmount temporary mount
    execute_command("umount /mnt");

    // Mount all subvolumes with Zstd compression (level 22)
    execute_command("mount -o subvol=@,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt");
    execute_command("mkdir -p /mnt/{home,root,srv,tmp,var/{cache,log},var/lib/{portables,machines},boot/efi}");

    // Mount other subvolumes with same compression settings
    execute_command("mount -o subvol=@home,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/home");
    execute_command("mount -o subvol=@root,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/root");
    execute_command("mount -o subvol=@srv,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/srv");
    execute_command("mount -o subvol=@cache,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/var/cache");
    execute_command("mount -o subvol=@tmp,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/tmp");
    execute_command("mount -o subvol=@log,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/var/log");
    execute_command("mount -o subvol=@/var/lib/portables,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/var/lib/portables");
    execute_command("mount -o subvol=@/var/lib/machines,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/var/lib/machines");
}

void setup_ext4_filesystem(const string& root_part) {
    execute_command("mount " + root_part + " /mnt");
    execute_command("mkdir -p /mnt/{home,boot/efi,etc,usr,var,proc,sys,dev,tmp,run}");
}

string detect_img_fs_type(const string& img_path) {
    string cmd = "file -b " + img_path;
    string result = exec(cmd.c_str());
    
    if (!result.empty() && result[result.length()-1] == '\n') {
        result.erase(result.length()-1);
    }
    
    cout << COLOR_CYAN << "Detected image type: " << result << COLOR_RESET << endl;
    
    if (result.find("Squashfs") != string::npos) {
        return "squashfs";
    }
    else if (result.find("EROFS") != string::npos || result.find("erofs") != string::npos) {
        return "erofs";
    }
    else {
        return "unknown";
    }
}

void copy_system(const string& efi_part, const string& img_fs_type) {
    string img_path = "/run/arch/bootmnt/LiveOS/rootfs.img";
    
    if (img_fs_type == "squashfs") {
        cout << COLOR_CYAN << "Extracting Squashfs image..." << COLOR_RESET << endl;
        execute_command("unsquashfs -f -d /mnt " + img_path);
    } else if (img_fs_type == "erofs") {
        cout << COLOR_CYAN << "Extracting EROFS image..." << COLOR_RESET << endl;
        execute_command("mkdir -p /mnt");
        execute_command("fsck.erofs --extract=/mnt " + img_path);
    } else {
        cerr << COLOR_RED << "Error: Unknown or unsupported image filesystem type" << COLOR_RESET << endl;
        exit(1);
    }
    
    execute_command("mount " + efi_part + " /mnt/boot/efi");
    execute_command("mkdir -p /mnt/{proc,sys,dev,run,tmp}");
    execute_command("cp btrfsfstabcompressed.sh /mnt/opt");
    execute_command("chmod +x /mnt/opt/btrfsfstabcompressed.sh");
}

void install_grub_ext4(const string& drive) {
    execute_command("mount --bind /dev /mnt/dev");
    execute_command("mount --bind /dev/pts /mnt/dev/pts");
    execute_command("mount --bind /proc /mnt/proc");
    execute_command("mount --bind /sys /mnt/sys");
    execute_command("mount --bind /run /mnt/run");
    execute_command("mount -t efivarfs efivarfs /sys/firmware/efi/efivars/");
    
    // Configure GRUB with proper kernel parameters (from shell script)
    string kernel_args = "root=PARTUUID=$(lsblk -dno PARTUUID " + drive + "2)";
    
    execute_command("chroot /mnt /bin/bash -c \""
    "echo 'GRUB_DEFAULT=0' > /etc/default/grub; "
    "echo 'GRUB_TIMEOUT=3' >> /etc/default/grub; "
    "echo 'GRUB_TIMEOUT_STYLE=menu' >> /etc/default/grub; "
    "echo 'GRUB_CMDLINE_LINUX=\\\\" + kernel_args + "\\\"' >> /etc/default/grub; "
    "genfstab -U /; "
    "grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB --recheck; "
    "grub-mkconfig -o /boot/grub/grub.cfg; "
    "mkinitcpio -P\"");
}

void install_grub_btrfs(const string& drive) {
    execute_command("touch /mnt/etc/fstab");
    execute_command("mount --bind /dev /mnt/dev");
    execute_command("mount --bind /dev/pts /mnt/dev/pts");
    execute_command("mount --bind /proc /mnt/proc");
    execute_command("mount --bind /sys /mnt/sys");
    execute_command("mount --bind /run /mnt/run");

    // Configure GRUB with proper kernel parameters for BTRFS (from shell script)
    string kernel_args = "root=PARTUUID=$(lsblk -dno PARTUUID " + drive + "2) rootflags=subvol=@ rootfstype=btrfs";
    
    execute_command("chroot /mnt /bin/bash -c \""
    "echo 'GRUB_DEFAULT=0' > /etc/default/grub; "
    "echo 'GRUB_TIMEOUT=3' >> /etc/default/grub; "
    "echo 'GRUB_TIMEOUT_STYLE=menu' >> /etc/default/grub; "
    "echo 'GRUB_CMDLINE_LINUX=\\\"" + kernel_args + "\\\"' >> /etc/default/grub; "
    "grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB --recheck; "
    "grub-mkconfig -o /boot/grub/grub.cfg; "
    "./opt/btrfsfstabcompressed.sh; "
    "rm -rf /opt/btrfsfstabcompressed.sh; "
    "mkinitcpio -P\"");
}

void post_install_menu() {
    bool menu_running = true;
    
    while (menu_running) {
        cout << "\n" << COLOR_CYAN;
        cout << "╔══════════════════════════════════════════════╗" << endl;
        cout << "║           INSTALLATION COMPLETE              ║" << endl;
        cout << "╠══════════════════════════════════════════════╣" << endl;
        cout << "║  1. Reboot now                              ║" << endl;
        cout << "║  2. Exit to shell                           ║" << endl;
        cout << "╚══════════════════════════════════════════════╝" << endl;
        cout << COLOR_RESET;
        
        cout << COLOR_CYAN << "Choose an option (1-2): " << COLOR_RESET;
        string choice;
        getline(cin, choice);
        
        if (choice == "1") {
            cout << COLOR_CYAN << "Unmounting and rebooting..." << COLOR_RESET << endl;
            execute_command("umount -R /mnt");
            execute_command("reboot");
            menu_running = false;
        }
        else if (choice == "2") {
            cout << COLOR_CYAN << "Exiting to shell. System is still mounted at /mnt" << COLOR_RESET << endl;
            menu_running = false;
        }
        else {
            cout << COLOR_RED << "Invalid option. Please choose 1 or 2." << COLOR_RESET << endl;
        }
    }
}

int main() {
    display_header();

    string drive;
    cout << COLOR_CYAN << "Enter target drive (e.g., /dev/sda): " << COLOR_RESET;
    getline(cin, drive);
    if (!is_block_device(drive)) {
        cerr << COLOR_RED << "Error: " << drive << " is not a valid block device" << COLOR_RESET << endl;
        return 1;
    }

    string fs_type;
    cout << COLOR_CYAN << "Choose filesystem type (btrfs/ext4): " << COLOR_RESET;
    getline(cin, fs_type);

    if (fs_type != "btrfs" && fs_type != "ext4") {
        cerr << COLOR_RED << "Error: Invalid filesystem type. Choose either 'btrfs' or 'ext4'" << COLOR_RESET << endl;
        return 1;
    }

    cout << COLOR_YELLOW << "\nWARNING: This will erase ALL data on " << drive << " and install a new system.\n";
    cout << "Are you sure you want to continue? (yes/no): " << COLOR_RESET;
    string confirmation;
    getline(cin, confirmation);

    if (confirmation != "yes") {
        cout << COLOR_CYAN << "Operation cancelled." << COLOR_RESET << endl;
        return 0;
    }

    cout << COLOR_CYAN << "\nPreparing target partitions..." << COLOR_RESET << endl;
    prepare_target_partitions(drive, fs_type);

    string root_part = drive + "2";

    cout << COLOR_CYAN << "Setting up " << fs_type << " filesystem..." << COLOR_RESET << endl;
    if (fs_type == "btrfs") {
        setup_btrfs_subvolumes(root_part);
    } else {
        setup_ext4_filesystem(root_part);
    }

    string img_path = "/run/arch/bootmnt/LiveOS/rootfs.img";
    
    cout << COLOR_CYAN << "Detecting filesystem type of " << img_path << "..." << COLOR_RESET << endl;
    string img_fs_type = detect_img_fs_type(img_path);

    cout << COLOR_CYAN << "Copying system files (this may take a while)..." << COLOR_RESET << endl;
    copy_system(drive + "1", img_fs_type);

    cout << COLOR_CYAN << "Installing bootloader..." << COLOR_RESET << endl;
    if (fs_type == "btrfs") {
        install_grub_btrfs(drive);
    } else {
        install_grub_ext4(drive);
    }

    cout << COLOR_GREEN << "\nInstallation complete! You can now reboot into your new system." << COLOR_RESET << endl;
    
    post_install_menu();

    return 0;
}
