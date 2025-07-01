#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>

#define TURQUOISE "\033[38;2;0;255;255m"
#define RED "\033[1;31m"
#define YELLOW "\033[1;33m"
#define RESET "\033[0m"

void clear_screen() {
    std::system("printf '\\033[H\\033[J'");
}

int main() {
    // Set turquoise console color
    std::system("echo -e \"\\033[38;2;0;255;255m\"");

    // ASCII Art Header
    clear_screen();
    std::cout << RED << std::endl;
    std::cout <<
    R"(░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
)";
std::cout << std::endl;
std::cout << TURQUOISE << "claudemods Btrfs Rsync Installer v1.01 Build 06/05/2025" << RESET << std::endl;
std::cout << std::endl;

// Set strict error handling via bash
std::system("set -euo pipefail");

// User input
std::string drive;
std::string username;

std::cout << "Enter drive (e.g., /dev/nvme0n1): ";
std::getline(std::cin, drive);

std::cout << "Enter username: ";
std::getline(std::cin, username);

// Validate inputs
if (drive.empty() || username.empty()) {
    std::cerr << RED << "Error: Missing inputs" << RESET << std::endl;
    return 1;
}

if (std::system(("test -e " + drive).c_str()) != 0) {
    std::cerr << RED << "Error: Drive " << drive << " not found" << RESET << std::endl;
    return 1;
}

// Partitioning
std::cout << TURQUOISE << "\n[1/6] Partitioning disk..." << RESET << std::endl;
std::system(("sudo wipefs --all " + drive).c_str());
std::system(("sudo parted -s " + drive + " mklabel gpt").c_str());
std::system(("sudo parted -s " + drive + " mkpart primary fat32 1MiB 551MiB").c_str());
std::system(("sudo parted -s " + drive + " set 1 esp on").c_str());
std::system(("sudo parted -s " + drive + " mkpart primary btrfs 551MiB 100%").c_str());

// Formatting
std::cout << TURQUOISE << "\n[2/6] Formatting partitions..." << RESET << std::endl;
std::system(("sudo mkfs.vfat -F32 " + drive + "1").c_str());
std::system(("sudo mkfs.btrfs -f " + drive + "2").c_str());

// Btrfs setup
std::cout << TURQUOISE << "\n[3/6] Configuring Btrfs subvolumes..." << RESET << std::endl;
std::system(("sudo mount " + drive + "2 /mnt").c_str());
std::system("sudo btrfs subvolume create /mnt/@");
std::system("sudo btrfs subvolume create /mnt/@home");
std::system("sudo btrfs subvolume create /mnt/@var_cache");
std::system("sudo btrfs subvolume create /mnt/@var_log");
std::system("sudo umount /mnt");

// Mount hierarchy
std::cout << TURQUOISE << "\n[4/6] Mounting filesystems..." << RESET << std::endl;
std::system(("sudo mount -o subvol=@ " + drive + "2 /mnt").c_str());
std::system("sudo mkdir -p /mnt/{boot/efi,home,var/{cache,log}}");
std::system(("sudo mount -o subvol=@home " + drive + "2 /mnt/home").c_str());
std::system(("sudo mount -o subvol=@var_cache " + drive + "2 /mnt/var/cache").c_str());
std::system(("sudo mount -o subvol=@var_log " + drive + "2 /mnt/var/log").c_str());
std::system(("sudo mount " + drive + "1 /mnt/boot/efi").c_str());

// Rsync root filesystem
std::cout << TURQUOISE << "\n[5/6] RSYNC entire root filesystem..." << RESET << std::endl;
std::system(R"(sudo rsync -aHAxSr --numeric-ids --info=progress2 \
        --include dev --include proc --include tmp --include sys --include run \
        --exclude dev/* --exclude proc/* --exclude tmp/* --exclude sys/* --exclude run/* \
        --include mnt --exclude mnt/* \
         --exclude var/lib/docker --exclude etc/fstab --exclude etc/mtab \
        --exclude etc/udev/rules.d/70-persistent-cd.rules --exclude etc/udev/rules.d/70-persistent-net.rules \
        / /mnt/)");

// Fstab generation with fallback
std::cout << "Generating fstab...\n";
std::system("sudo mkdir -p /mnt/etc");
if (std::system("sudo genfstab -U /mnt > /mnt/etc/fstab") != 0) {
    std::string efi_uuid = "lsblk -no UUID " + drive + "1";
    std::string root_uuid = "lsblk -no UUID " + drive + "2";

    char efi_uuid_cstr[128];
    char root_uuid_cstr[128];

    FILE* efi_pipe = popen(efi_uuid.c_str(), "r");
    FILE* root_pipe = popen(root_uuid.c_str(), "r");

    fgets(efi_uuid_cstr, sizeof(efi_uuid_cstr), efi_pipe);
    fgets(root_uuid_cstr, sizeof(root_uuid_cstr), root_pipe);

    pclose(efi_pipe);
    pclose(root_pipe);

    std::string efi_uuid_str(efi_uuid_cstr);
    std::string root_uuid_str(root_uuid_cstr);

    efi_uuid_str.erase(efi_uuid_str.find_last_not_of(" \n\r\t")+1);
    root_uuid_str.erase(root_uuid_str.find_last_not_of(" \n\r\t")+1);

    std::string fstab = "UUID=" + efi_uuid_str + "  /boot/efi  vfat  umask=0077 0 2\n" +
    "UUID=" + root_uuid_str + "  /          btrfs  subvol=@,compress=zstd 0 0\n" +
    "UUID=" + root_uuid_str + "  /home      btrfs  subvol=@home,compress=zstd 0 0\n" +
    "UUID=" + root_uuid_str + "  /var/cache btrfs  subvol=@var_cache,compress=zstd 0 0\n" +
    "UUID=" + root_uuid_str + "  /var/log   btrfs  subvol=@var_log,compress=zstd 0 0\n";

    FILE* f = fopen("/mnt/etc/fstab", "w");
    if (f) {
        fwrite(fstab.c_str(), 1, fstab.size(), f);
        fclose(f);
    }
}

// Chroot fixes
std::cout << TURQUOISE << "\n[6/6] Configuring bootloader..." << RESET << std::endl;
int chroot_err = std::system(R"(sudo arch-chroot /mnt /bin/bash -c "
        grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB --recheck || exit 1
        grub-mkconfig -o /boot/grub/grub.cfg || exit 1
        mkinitcpio -P || exit 1
    ")");

if (chroot_err != 0) {
    std::cerr << RED << "Chroot commands failed" << RESET << std::endl;
    return 1;
}

// Final config
std::system(("sudo chown -R " + username + ": /mnt/home/" + username).c_str());
std::system("sudo umount -R /mnt 2>/dev/null || true");

// Post-install menu
while (true) {
    clear_screen();
    std::cout << RED;
    std::cout <<
    R"(╔══════════════════════════════════════╗
║        Post-Install Menu             ║
╠══════════════════════════════════════╣
║ 1. Chroot into installed system      ║
║ 2. Reboot                            ║
║ 3. Exit                              ║
╚══════════════════════════════════════╝
)";
std::cout << TURQUOISE << std::endl;
std::cout << "Select option (1-3): " << RESET;
char choice;
std::cin >> choice;

switch (choice) {
    case '1': {
        std::system(("sudo mount " + drive + "2 /mnt -o subvol=@").c_str());
        std::system(("sudo mount " + drive + "1 /mnt/boot/efi").c_str());
        std::system(("sudo mount -o subvol=@home " + drive + "2 /mnt/home").c_str());
        std::system(("sudo mount -o subvol=@var_cache " + drive + "2 /mnt/var/cache").c_str());
        std::system(("sudo mount -o subvol=@var_log " + drive + "2 /mnt/var/log").c_str());
        std::system("sudo arch-chroot /mnt /bin/bash");
        std::system("sudo umount -R /mnt");
        break;
    }
    case '2':
        std::cout << YELLOW << "Rebooting in 3 seconds..." << RESET << std::endl;
        sleep(3);
        std::system("sudo reboot");
        break;
    case '3':
        return 0;
    default:
        std::cerr << RED << "Invalid option. Try again." << RESET << std::endl;
        sleep(2);
}
}

return 0;
}
