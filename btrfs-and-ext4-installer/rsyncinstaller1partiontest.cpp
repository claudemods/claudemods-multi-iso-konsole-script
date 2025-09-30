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
cout << COLOR_CYAN << "claudemods cmi rsync installer v1.01" << COLOR_RESET << endl;
cout << COLOR_CYAN << "Supports Btrfs (with Zstd compression) and Ext4 filesystems" << COLOR_RESET << endl << endl;
}

void prepare_target_partitions(const string& root_part, const string& fs_type) {
    execute_command("umount -f " + root_part + " 2>/dev/null || true");
    execute_command("mkdir -p /mnt");

    if (fs_type == "btrfs") {
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
    } else {
        execute_command("mount " + root_part + " /mnt");
        execute_command("mkdir -p /mnt/{home,boot/efi,etc,usr,var,proc,sys,dev,tmp,run}");
    }
}

void setup_btrfs_subvolumes(const string& root_part) {
    // This function is now integrated into prepare_target_partitions
}

void setup_ext4_filesystem(const string& root_part) {
    // This function is now integrated into prepare_target_partitions
}

void copy_system() {
    string rsync_cmd = "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
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
    "--include=/dev "
    "--include=/proc "
    "--include=/tmp "
    "--include=/sys "
    "--include=/run "
    "--include=/usr "
    "--include=/etc "
    "/ /mnt";
    execute_command(rsync_cmd);
    
    execute_command("mkdir -p /mnt/{proc,sys,dev,run,tmp}");
    execute_command("mkdir -p /mnt/boot/efi"); // Ensure EFI directory exists
    execute_command("cp btrfsfstabcompressed.sh /mnt/opt");
    execute_command("chmod +x /mnt/opt/btrfsfstabcompressed.sh");
}

void install_grub_ext4(const string& root_part) {
    execute_command("mount --bind /dev /mnt/dev");
    execute_command("mount --bind /dev/pts /mnt/dev/pts");
    execute_command("mount --bind /proc /mnt/proc");
    execute_command("mount --bind /sys /mnt/sys");
    execute_command("mount --bind /run /mnt/run");
    
    // Create proper fstab and install GRUB
    execute_command("chroot /mnt /bin/bash -c \""
    "mkdir -p /boot/efi; "
    "mount -t efivarfs efivarfs /sys/firmware/efi/efivars 2>/dev/null || true; "
    "genfstab -U / > /etc/fstab; "  // THIS CREATES FSTAB THAT SURVIVES REBOOT
    "echo 'EFI directory contents:'; ls -la /boot/efi/; "
    "grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB --recheck; "
    "grub-mkconfig -o /boot/grub/grub.cfg; "
    "mkinitcpio -P; "
    "echo '=== FSTAB CONTENTS ==='; cat /etc/fstab; echo '=== END FSTAB ==='\"");
}

void install_grub_btrfs(const string& root_part) {
    execute_command("mount --bind /dev /mnt/dev");
    execute_command("mount --bind /dev/pts /mnt/dev/pts");
    execute_command("mount --bind /proc /mnt/proc");
    execute_command("mount --bind /sys /mnt/sys");
    execute_command("mount --bind /run /mnt/run");

    // Create proper fstab and install GRUB for Btrfs
    execute_command("chroot /mnt /bin/bash -c \""
    "mkdir -p /boot/efi; "
    "mount -t efivarfs efivarfs /sys/firmware/efi/efivars 2>/dev/null || true; "
    "./opt/btrfsfstabcompressed.sh; "  // THIS SHOULD CREATE /etc/fstab FOR BTRFS
    "echo 'EFI directory contents:'; ls -la /boot/efi/; "
    "grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB --recheck; "
    "grub-mkconfig -o /boot/grub/grub.cfg; "
    "mkinitcpio -P; "
    "echo '=== FSTAB CONTENTS ==='; cat /etc/fstab; echo '=== END FSTAB ==='\"");
}

int main() {
    display_header();

    string root_part;
    cout << COLOR_CYAN << "Enter target root partition (e.g., /dev/sda2): " << COLOR_RESET;
    getline(cin, root_part);
    if (!is_block_device(root_part)) {
        cerr << COLOR_RED << "Error: " << root_part << " is not a valid block device" << COLOR_RESET << endl;
        return 1;
    }

    string fs_type;
    cout << COLOR_CYAN << "Choose filesystem type (btrfs/ext4): " << COLOR_RESET;
    getline(cin, fs_type);

    if (fs_type != "btrfs" && fs_type != "ext4") {
        cerr << COLOR_RED << "Error: Invalid filesystem type. Choose either 'btrfs' or 'ext4'" << COLOR_RESET << endl;
        return 1;
    }

    cout << COLOR_YELLOW << "\nWARNING: This will erase ALL data on " << root_part << " and install a new system.\n";
    cout << "Are you sure you want to continue? (yes/no): " << COLOR_RESET;
    string confirmation;
    getline(cin, confirmation);

    if (confirmation != "yes") {
        cout << COLOR_CYAN << "Operation cancelled." << COLOR_RESET << endl;
        return 0;
    }

    cout << COLOR_CYAN << "\nPreparing target partition..." << COLOR_RESET << endl;
    prepare_target_partitions(root_part, fs_type);

    cout << COLOR_CYAN << "Copying system files (this may take a while)..." << COLOR_RESET << endl;
    copy_system();

    cout << COLOR_CYAN << "Installing bootloader..." << COLOR_RESET << endl;
    if (fs_type == "btrfs") {
        install_grub_btrfs(root_part);
    } else {
        install_grub_ext4(root_part);
    }

    cout << COLOR_CYAN << "Cleaning up..." << COLOR_RESET << endl;
    execute_command("umount -R /mnt");

    cout << COLOR_GREEN << "\nInstallation complete! You can now reboot into your new system." << COLOR_RESET << endl;
    cout << COLOR_YELLOW << "IMPORTANT: Make sure your BIOS/UEFI is set to boot from the correct device." << COLOR_RESET << endl;

    return 0;
}
