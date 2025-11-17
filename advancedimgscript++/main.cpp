#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>
#include <termios.h>
#include <algorithm>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

// Include your resource manager
#include "resources.h"

// Color definitions
const std::string COLOR_CYAN = "\033[38;2;0;255;255m";
const std::string COLOR_RED = "\033[31m";
const std::string COLOR_GREEN = "\033[32m";
const std::string COLOR_YELLOW = "\033[33m";
const std::string COLOR_ORANGE = "\033[38;5;208m";
const std::string COLOR_PURPLE = "\033[38;5;93m";
const std::string COLOR_RESET = "\033[0m";

class ClaudemodsInstaller {
private:
    std::string target_folder;
    std::string new_username;
    std::string root_password;
    std::string user_password;
    std::string timezone;
    std::string keyboard_layout;
    std::string current_distro_name; // Store the current distro name for ISO

    // Terminal control for arrow keys
    struct termios oldt, newt;

    // Get current working directory
    std::string getCurrentDir() {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            return std::string(cwd);
        }
        return ".";
    }

    // ADD ZIP EXTRACTION FUNCTION
    bool extractRequiredFiles() {
        std::cout << COLOR_CYAN << "Extracting required files..." << COLOR_RESET << std::endl;

        // Use your existing resource manager functions
        if (!ResourceManager::extractEmbeddedZip("")) {
            std::cerr << COLOR_RED << "Failed to extract embedded ZIP files!" << COLOR_RESET << std::endl;
            return false;
        }

        if (!ResourceManager::extractCalamaresResources("")) {
            std::cerr << COLOR_RED << "Failed to extract Calamares resources!" << COLOR_RESET << std::endl;
            return false;
        }

        std::cout << COLOR_GREEN << "All required files extracted successfully!" << COLOR_RESET << std::endl;
        return true;
    }

    void display_header() {
        std::cout << COLOR_RED;
        std::cout << "░█████╗░██╗░░░░░░█████╗░██║░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗" << std::endl;
        std::cout << "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝" << std::endl;
        std::cout << "██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░" << std::endl;
        std::cout << "██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗" << std::endl;
        std::cout << "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝" << std::endl;
        std::cout << "░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░" << std::endl;
        std::cout << COLOR_CYAN << "claudemods distribution iso creator Beta v1.0 16-11-2025" << COLOR_RESET << std::endl;
        std::cout << std::endl;
    }

    // Function to setup terminal for arrow key reading
    void setup_terminal() {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    }

    // Function to restore terminal
    void restore_terminal() {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }

    // Function to get arrow key input
    int get_arrow_key() {
        int ch = getchar();
        if (ch == 27) { // ESC
            getchar(); // Skip [
            ch = getchar(); // Actual key
            return ch;
        }
        return ch;
    }

    int show_menu(const std::vector<std::string>& options, const std::string& title, int selected = 0) {
        setup_terminal();

        while (true) {
            system("clear");
            display_header();

            std::cout << COLOR_CYAN;
            std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
            std::cout << "║ " << std::left << std::setw(60) << title << "║" << std::endl;
            std::cout << "╠══════════════════════════════════════════════════════════════╣" << std::endl;

            for (int i = 0; i < options.size(); i++) {
                std::cout << "║ ";
                if (i == selected) {
                    std::cout << COLOR_GREEN << "> " << COLOR_RESET << COLOR_CYAN;
                } else {
                    std::cout << "  ";
                }

                // Create the menu line with proper formatting
                std::string menu_line;
                if (title == "claudemods distribution iso creator") {
                    std::string setting_value;
                    switch(i) {
                        case 0: setting_value = target_folder.empty() ? "[Not Set]" : target_folder; break;
                        case 1: setting_value = new_username.empty() ? "[Not Set]" : new_username; break;
                        case 2: setting_value = root_password.empty() ? "[Not Set]" : "********"; break;
                        case 3: setting_value = user_password.empty() ? "[Not Set]" : "********"; break;
                        case 4: setting_value = timezone.empty() ? "[Not Set]" : timezone; break;
                        case 5: setting_value = keyboard_layout.empty() ? "[Not Set]" : keyboard_layout; break;
                        case 6: setting_value = current_distro_name.empty() ? "[Not Set]" : current_distro_name; break;
                        default: setting_value = ""; break;
                    }

                    // Build the complete line with menu item and setting
                    menu_line = options[i] + " " + setting_value;
                } else {
                    menu_line = options[i];
                }

                // Truncate if too long to fit in the box
                if (menu_line.length() > 56) { // 60 - 4 for borders and spacing
                    menu_line = menu_line.substr(0, 53) + "...";
                }

                std::cout << std::left << std::setw(56) << menu_line;
                std::cout << "║" << std::endl;
            }

            std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
            std::cout << COLOR_RESET;
            std::cout << COLOR_YELLOW << "Use ↑↓ arrows to navigate, Enter to select" << COLOR_RESET << std::endl;

            int key = get_arrow_key();
            if (key == 'A') {
                selected = (selected - 1 + options.size()) % options.size();
            } else if (key == 'B') {
                selected = (selected + 1) % options.size();
            } else if (key == 10) {
                restore_terminal();
                return selected;
            }
        }
    }

    // Function to get text input with prompt
    std::string get_input(const std::string& prompt) {
        std::cout << COLOR_CYAN << prompt << COLOR_RESET;
        std::string input;
        std::getline(std::cin, input);
        return input;
    }

    int execute_command(const std::string& cmd) {
        // Set color and immediately execute
        std::cout << COLOR_CYAN << std::flush;
        int status = system(cmd.c_str());
        std::cout << COLOR_RESET << std::flush;
        return status;
    }

    bool create_directory(const std::string& path) {
        return system(("sudo mkdir -p " + path).c_str()) == 0;
    }

    // FIXED: Function to display current settings on main menu
    void display_current_settings() {
        std::cout << COLOR_YELLOW << "\nCurrent Settings:" << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Installation Path: " << COLOR_RESET
        << (target_folder.empty() ? "[Not Set]" : target_folder) << std::endl;
        std::cout << COLOR_CYAN << "Username: " << COLOR_RESET
        << (new_username.empty() ? "[Not Set]" : new_username) << std::endl;
        std::cout << COLOR_CYAN << "Root Password: " << COLOR_RESET
        << (root_password.empty() ? "[Not Set]" : "********") << std::endl;
        std::cout << COLOR_CYAN << "User Password: " << COLOR_RESET
        << (user_password.empty() ? "[Not Set]" : "********") << std::endl;
        std::cout << COLOR_CYAN << "Timezone: " << COLOR_RESET
        << (timezone.empty() ? "[Not Set]" : timezone) << std::endl;
        std::cout << COLOR_CYAN << "Keyboard Layout: " << COLOR_RESET
        << (keyboard_layout.empty() ? "[Not Set]" : keyboard_layout) << std::endl;
        std::cout << COLOR_CYAN << "Current Distro: " << COLOR_RESET
        << (current_distro_name.empty() ? "[Not Set]" : current_distro_name) << std::endl;
        std::cout << std::endl;
    }

    // UPDATED: Function to create squashfs image after installation with additional steps
    void create_squashfs_image(const std::string& distro_name) {
        std::cout << COLOR_CYAN << "Creating squashfs image..." << COLOR_RESET << std::endl;

        std::string currentDir = getCurrentDir();
        std::string squashfs_cmd = "sudo mksquashfs " + target_folder + " " + currentDir + "/build-image-arch-img/LiveOS/rootfs.img -noappend -comp xz -b 256K -Xbcj x86";

        std::cout << COLOR_CYAN << "Executing: " << squashfs_cmd << COLOR_RESET << std::endl;

        if (execute_command(squashfs_cmd) == 0) {
            std::cout << COLOR_GREEN << "Squashfs image created successfully!" << COLOR_RESET << std::endl;

            // NEW: Copy vmlinuz from current system's /boot to build directory
            std::cout << COLOR_CYAN << "Copying kernel image..." << COLOR_RESET << std::endl;
            std::string copy_kernel_cmd = "sudo cp /boot/vmlinuz* " + currentDir + "/build-image-arch-img/boot/vmlinuz-x86_64";
            if (execute_command(copy_kernel_cmd) == 0) {
                std::cout << COLOR_GREEN << "Kernel image copied successfully!" << COLOR_RESET << std::endl;
            } else {
                std::cout << COLOR_RED << "Failed to copy kernel image!" << COLOR_RESET << std::endl;
            }

            // NEW: Generate initramfs
            std::cout << COLOR_CYAN << "Generating initramfs..." << COLOR_RESET << std::endl;
            std::string initramfs_cmd = "cd " + currentDir + "/build-image-arch-img && sudo mkinitcpio -c mkinitcpio.conf -g " + currentDir + "/build-image-arch-img/boot/initramfs-x86_64.img";
            if (execute_command(initramfs_cmd) == 0) {
                std::cout << COLOR_GREEN << "Initramfs generated successfully!" << COLOR_RESET << std::endl;

                // NEW: Create ISO with XORRISO after initramfs generation
                create_iso_image(distro_name);
            } else {
                std::cout << COLOR_RED << "Failed to generate initramfs!" << COLOR_RESET << std::endl;
            }

        } else {
            std::cout << COLOR_RED << "Failed to create squashfs image!" << COLOR_RESET << std::endl;
        }
    }

    // NEW: Create ISO image with XORRISO
    void create_iso_image(const std::string& distro_name) {
        std::cout << COLOR_CYAN << "Creating ISO image with XORRISO..." << COLOR_RESET << std::endl;

        std::string currentDir = getCurrentDir();
        std::string currentDIR = currentDir;

        // Build the XORRISO command with the distro name as ISO filename
        std::string xorriso_cmd = "sudo xorriso -as mkisofs "
        "--modification-date=\"$(date +%Y%m%d%H%M%S00)\" "
        "--protective-msdos-label "
        "-volid \"2025\" "
        "-appid \"claudemods Linux Live/Rescue CD\" "
        "-publisher \"claudemods claudemods101@gmail.com >\" "
        "-preparer \"Prepared by user\" "
        "-r -graft-points -no-pad "
        "--sort-weight 0 / "
        "--sort-weight 1 /boot "
        "--grub2-mbr " + currentDir + "/build-image-arch-img/boot/grub/i386-pc/boot_hybrid.img "
        "-partition_offset 16 "
        "-b boot/grub/i386-pc/eltorito.img "
        "-c boot.catalog "
        "-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info "
        "-eltorito-alt-boot "
        "-append_partition 2 0xef " + currentDir + "/build-image-arch-img/boot/efi.img "
        "-e --interval:appended_partition_2:all:: "
        "-no-emul-boot "
        "-iso-level 3 "
        "-o \"" + currentDir + "/build-image-arch-img/" + distro_name + ".iso\" " +
        currentDir + "/build-image-arch-img";

        std::cout << COLOR_CYAN << "Executing: " << xorriso_cmd << COLOR_RESET << std::endl;

        if (execute_command(xorriso_cmd) == 0) {
            std::cout << COLOR_GREEN << "ISO image created successfully: " << distro_name << ".iso" << COLOR_RESET << std::endl;
        } else {
            std::cout << COLOR_RED << "Failed to create ISO image!" << COLOR_RESET << std::endl;
        }
    }

    // NEW: Set installation path
    void set_installation_path() {
        std::string base_path = get_input("Enter base installation path (e.g., /mnt): ");
        target_folder = base_path + "/claudemods-distro";
        // REMOVED the green confirmation message
    }

    // NEW: Set username
    void set_username() {
        new_username = get_input("Enter username: ");
        // REMOVED the green confirmation message
    }

    // NEW: Set root password
    void set_root_password() {
        root_password = get_input("Enter root password: ");
        // REMOVED the green confirmation message
    }

    // NEW: Set user password
    void set_user_password() {
        user_password = get_input("Enter user password: ");
        // REMOVED the green confirmation message
    }

    // NEW: Set timezone
    void set_timezone() {
        std::vector<std::string> timezone_options = {
            "America/New_York (US English)",
            "Europe/London (UK English)",
            "Europe/Berlin (German)",
            "Europe/Paris (French)",
            "Europe/Madrid (Spanish)",
            "Europe/Rome (Italian)",
            "Asia/Tokyo (Japanese)",
            "Other (manual entry)"
        };

        int timezone_choice = show_menu(timezone_options, "Select Timezone");
        switch(timezone_choice) {
            case 0: timezone = "America/New_York"; break;
            case 1: timezone = "Europe/London"; break;
            case 2: timezone = "Europe/Berlin"; break;
            case 3: timezone = "Europe/Paris"; break;
            case 4: timezone = "Europe/Madrid"; break;
            case 5: timezone = "Europe/Rome"; break;
            case 6: timezone = "Asia/Tokyo"; break;
            case 7: timezone = get_input("Enter timezone (e.g., Europe/Berlin): "); break;
        }
        // REMOVED the green confirmation message
    }

    // NEW: Set keyboard layout
    void set_keyboard_layout() {
        std::vector<std::string> keyboard_options = {
            "us (US English)",
            "uk (UK English)",
            "de (German)",
            "fr (French)",
            "es (Spanish)",
            "it (Italian)",
            "jp (Japanese)",
            "Other (manual entry)"
        };

        int keyboard_choice = show_menu(keyboard_options, "Select Keyboard Layout");
        switch(keyboard_choice) {
            case 0: keyboard_layout = "us"; break;
            case 1: keyboard_layout = "uk"; break;
            case 2: keyboard_layout = "de"; break;
            case 3: keyboard_layout = "fr"; break;
            case 4: keyboard_layout = "es"; break;
            case 5: keyboard_layout = "it"; break;
            case 6: keyboard_layout = "jp"; break;
            case 7: keyboard_layout = get_input("Enter keyboard layout (e.g., br, ru, pt): "); break;
        }
        // REMOVED the green confirmation message
    }

    // NEW: Check if all required settings are configured
    bool check_settings_configured() {
        if (target_folder.empty()) {
            std::cout << COLOR_RED << "Error: Installation path not set!" << COLOR_RESET << std::endl;
            return false;
        }
        if (new_username.empty()) {
            std::cout << COLOR_RED << "Error: Username not set!" << COLOR_RESET << std::endl;
            return false;
        }
        if (root_password.empty()) {
            std::cout << COLOR_RED << "Error: Root password not set!" << COLOR_RESET << std::endl;
            return false;
        }
        if (user_password.empty()) {
            std::cout << COLOR_RED << "Error: User password not set!" << COLOR_RESET << std::endl;
            return false;
        }
        if (timezone.empty()) {
            std::cout << COLOR_RED << "Error: Timezone not set!" << COLOR_RESET << std::endl;
            return false;
        }
        if (keyboard_layout.empty()) {
            std::cout << COLOR_RED << "Error: Keyboard layout not set!" << COLOR_RESET << std::endl;
            return false;
        }
        return true;
    }

    void mount_system_dirs() {
        execute_command("sudo mkdir -p " + target_folder + "/dev");
        execute_command("sudo mkdir -p " + target_folder + "/dev/pts");
        execute_command("sudo mkdir -p " + target_folder + "/proc");
        execute_command("sudo mkdir -p " + target_folder + "/sys");
        execute_command("sudo mkdir -p " + target_folder + "/run");
        execute_command("sudo mkdir -p " + target_folder + "/etc");

        execute_command("sudo mount --bind /dev " + target_folder + "/dev");
        execute_command("sudo mount --bind /dev/pts " + target_folder + "/dev/pts");
        execute_command("sudo mount --bind /proc " + target_folder + "/proc");
        execute_command("sudo mount --bind /sys " + target_folder + "/sys");
        execute_command("sudo mount --bind /run " + target_folder + "/run");
    }

    void unmount_system_dirs() {
        execute_command("sudo umount " + target_folder + "/dev/pts");
        execute_command("sudo umount " + target_folder + "/dev");
        execute_command("sudo umount " + target_folder + "/proc");
        execute_command("sudo umount " + target_folder + "/sys");
        execute_command("sudo umount " + target_folder + "/run");
    }

    void create_user() {
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"useradd -m -G wheel -s /bin/bash " + new_username + "\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"echo 'root:" + root_password + "' | chpasswd\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"echo '" + new_username + ":" + user_password + "' | chpasswd\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"echo '%wheel ALL=(ALL:ALL) ALL' | tee -a /etc/sudoers\"");
    }

    void apply_timezone_keyboard_settings() {
        std::cout << COLOR_CYAN << "Setting timezone to: " << timezone << COLOR_RESET << std::endl;
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"ln -sf /usr/share/zoneinfo/" + timezone + " /etc/localtime\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"hwclock --systohc\"");

        std::cout << COLOR_CYAN << "Setting keyboard layout to: " << keyboard_layout << COLOR_RESET << std::endl;
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"echo 'KEYMAP=" + keyboard_layout + "' > /etc/vconsole.conf\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"echo 'LANG=en_US.UTF-8' > /etc/locale.conf\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"echo 'en_US.UTF-8 UTF-8' >> /etc/locale.gen\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"locale-gen\"");
    }

    // FIXED: Create target directory first and ensure pacstrap works
    void install_spitfire_ckge_minimal() {
        current_distro_name = "Spitfire-CKGE-Minimal";
        if (!check_settings_configured()) {
            std::cout << COLOR_RED << "Cannot proceed with installation. Please configure all settings first." << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_ORANGE << "Installing Spitfire CKGE Minimal..." << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Starting Spitfire CKGE Minimal installation in: " << target_folder << COLOR_RESET << std::endl;

        std::string currentDir = getCurrentDir();
        std::cout << COLOR_CYAN << "Working from directory: " << currentDir << COLOR_RESET << std::endl;

        // CREATE TARGET DIRECTORY FIRST
        std::cout << COLOR_CYAN << "Creating target directory: " << target_folder << COLOR_RESET << std::endl;
        execute_command("sudo mkdir -p " + target_folder);

        // VERIFY DIRECTORY WAS CREATED
        struct stat info;
        if (stat(target_folder.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "Failed to create target directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        if (!(info.st_mode & S_IFDIR)) {
            std::cerr << COLOR_RED << "Target path is not a directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_GREEN << "Target directory created successfully!" << COLOR_RESET << std::endl;

        // CREATE ESSENTIAL DIRECTORIES BEFORE COPYING FILES
        execute_command("sudo mkdir -p " + target_folder + "/etc/pacman.d");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/grub/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/plymouth/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/local/bin");
        execute_command("sudo mkdir -p " + target_folder + "/etc/systemd/system");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");

        // VERIFY AND COPY FILES WITH EXACT NAMES
        execute_command("sudo cp -r " + currentDir + "/vconsole.conf " + target_folder + "/etc/vconsole.conf");
        execute_command("sudo cp -r /etc/resolv.conf " + target_folder + "/etc/resolv.conf");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d " + target_folder + "/etc/pacman.d");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d /etc/pacman.d");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf " + target_folder + "/etc/pacman.conf");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf /etc/pacman.conf");

        execute_command("sudo pacman -Sy");

        // INSTALL BASE SYSTEM WITH PACSTRAP
        std::cout << COLOR_CYAN << "Installing base system with pacstrap..." << COLOR_RESET << std::endl;
        execute_command("sudo pacstrap " + target_folder + " claudemods-desktop");

        // VERIFY PACSTRAP SUCCESS
        std::string test_bin = target_folder + "/bin/bash";
        if (stat(test_bin.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "pacstrap failed! /bin/bash not found in target." << COLOR_RESET << std::endl;
            return;
        }

        execute_command("sudo mkdir -p " + target_folder + "/boot");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo touch " + target_folder + "/boot/grub/grub.cfg.new");

        mount_system_dirs();
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable sddm\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable NetworkManager\"");

        apply_timezone_keyboard_settings();

        // FIXED FILE PATHS - ENSURE CORRECT NAMES
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/grub " + target_folder + "/etc/default/grub");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/grub.cfg " + target_folder + "/boot/grub/grub.cfg");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos " + target_folder + "/usr/share/grub/themes/cachyos");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"grub-mkconfig -o /boot/grub/grub.cfg\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos-bootanimation " + target_folder + "/usr/share/plymouth/themes/cachyos-bootanimation");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/term.sh " + target_folder + "/usr/local/bin/term.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/local/bin/term.sh\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/term.service " + target_folder + "/etc/systemd/system/term.service");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable term.service >/dev/null 2>&1\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"plymouth-set-default-theme -R cachyos-bootanimation\"");

        create_user();

        // CREATE USER HOME DIRECTORY STRUCTURE
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.config/fish");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share/konsole");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share");

        execute_command("sudo chmod +x " + target_folder + "/home/" + new_username + "/.config/fish/config.fish");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/share/fish/config.fish\"");

        execute_command("cd " + target_folder);
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/claudemods-desktop/spitfire-minimal.zip");
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/arch-systemtool/Arch-Systemtool.zip");
        execute_command("sudo unzip -o " + currentDir + "/Arch-Systemtool.zip -d " + target_folder + "/opt/Arch-Systemtool");
        execute_command("sudo unzip -o " + currentDir + "/spitfire-minimal.zip -d " + target_folder + "/home/" + new_username + "/");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/kde_settings.conf " + target_folder + "/etc/sddm.conf.d/kde_settings.conf");
        execute_command("sudo cp " + currentDir + "/spitfire-ckge-minimal/tweaksspitfire.sh " + target_folder + "/opt/tweaksspitfire.sh");
        execute_command("sudo chmod +x " + target_folder + "/opt/tweaksspitfire.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"su - " + new_username + " -c 'cd /opt && ./tweaksspitfire.sh " + new_username + "'\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/konsolerc " + target_folder + "/home/" + new_username + "/.config/konsolerc");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/SpitFireLogin " + target_folder + "/usr/share/sddm/themes/SpitFireLogin");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/claudemods-cyan.colorscheme " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.colorscheme");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/claudemods-cyan.profile " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.profile");
        execute_command("sudo rm -rf " + currentDir + "/Arch-Systemtool.zip");
        execute_command("sudo rm -rf " + currentDir + "/spitfire-minimal.zip");
        execute_command("sudo rm -rf " + target_folder + "/opt/tweaksspitfire.sh");

        // Fix user-places.xbel
        std::string cmd = "sudo ls -1 " + target_folder + "/home | grep -v '^\\.' | head -1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            std::string home_folder;
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                home_folder = buffer;
                home_folder.erase(std::remove(home_folder.begin(), home_folder.end(), '\n'), home_folder.end());
            }
            pclose(pipe);

            if (!home_folder.empty()) {
                std::string user_places_file = target_folder + "/home/" + home_folder + "/.local/share/user-places.xbel";
                std::string sed_cmd = "sudo sed -i 's/spitfire/" + home_folder + "/g' " + user_places_file;
                execute_command(sed_cmd);
            }
        }

        unmount_system_dirs();
        std::cout << COLOR_GREEN << "Spitfire CKGE Minimal installation completed in: " << target_folder << COLOR_RESET << std::endl;

        // CREATE SQUASHFS IMAGE AFTER INSTALLATION
        create_squashfs_image("Spitfire-CKGE-Minimal");
    }

    // SPITFIRE CKGE MINIMAL DEV - FIXED
    void install_spitfire_ckge_minimal_dev() {
        current_distro_name = "Spitfire-CKGE-Minimal-Dev";
        if (!check_settings_configured()) {
            std::cout << COLOR_RED << "Cannot proceed with installation. Please configure all settings first." << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_ORANGE << "Installing Spitfire CKGE Minimal Dev..." << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Starting Spitfire CKGE Minimal Dev installation in: " << target_folder << COLOR_RESET << std::endl;

        std::string currentDir = getCurrentDir();
        std::cout << COLOR_CYAN << "Working from directory: " << currentDir << COLOR_RESET << std::endl;

        // CREATE TARGET DIRECTORY FIRST
        std::cout << COLOR_CYAN << "Creating target directory: " << target_folder << COLOR_RESET << std::endl;
        execute_command("sudo mkdir -p " + target_folder);

        // VERIFY DIRECTORY WAS CREATED
        struct stat info;
        if (stat(target_folder.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "Failed to create target directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        if (!(info.st_mode & S_IFDIR)) {
            std::cerr << COLOR_RED << "Target path is not a directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_GREEN << "Target directory created successfully!" << COLOR_RESET << std::endl;

        // CREATE ESSENTIAL DIRECTORIES BEFORE COPYING FILES
        execute_command("sudo mkdir -p " + target_folder + "/etc/pacman.d");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/grub/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/plymouth/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/local/bin");
        execute_command("sudo mkdir -p " + target_folder + "/etc/systemd/system");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");

        // VERIFY AND COPY FILES WITH EXACT NAMES
        execute_command("sudo cp -r " + currentDir + "/vconsole.conf " + target_folder + "/etc/vconsole.conf");
        execute_command("sudo cp -r /etc/resolv.conf " + target_folder + "/etc/resolv.conf");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d " + target_folder + "/etc/pacman.d");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d /etc/pacman.d");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf " + target_folder + "/etc/pacman.conf");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf /etc/pacman.conf");

        execute_command("sudo pacman -Sy");

        // INSTALL BASE SYSTEM WITH PACSTRAP
        std::cout << COLOR_CYAN << "Installing base system with pacstrap..." << COLOR_RESET << std::endl;
        execute_command("sudo pacstrap " + target_folder + " claudemods-desktop-dev");

        // VERIFY PACSTRAP SUCCESS
        std::string test_bin = target_folder + "/bin/bash";
        if (stat(test_bin.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "pacstrap failed! /bin/bash not found in target." << COLOR_RESET << std::endl;
            return;
        }

        execute_command("sudo mkdir -p " + target_folder + "/boot");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo touch " + target_folder + "/boot/grub/grub.cfg.new");

        mount_system_dirs();
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable sddm\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable NetworkManager\"");

        apply_timezone_keyboard_settings();

        // FIXED FILE PATHS - ENSURE CORRECT NAMES
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/grub " + target_folder + "/etc/default/grub");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/grub.cfg " + target_folder + "/boot/grub/grub.cfg");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos " + target_folder + "/usr/share/grub/themes/cachyos");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"grub-mkconfig -o /boot/grub/grub.cfg\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos-bootanimation " + target_folder + "/usr/share/plymouth/themes/cachyos-bootanimation");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/term.sh " + target_folder + "/usr/local/bin/term.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/local/bin/term.sh\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/term.service " + target_folder + "/etc/systemd/system/term.service");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable term.service >/dev/null 2>&1\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"plymouth-set-default-theme -R cachyos-bootanimation\"");

        create_user();

        // CREATE USER HOME DIRECTORY STRUCTURE
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.config/fish");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share/konsole");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share");

        execute_command("sudo chmod +x " + target_folder + "/home/" + new_username + "/.config/fish/config.fish");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/share/fish/config.fish\"");

        execute_command("cd " + target_folder);
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/claudemods-desktop/spitfire-minimal.zip");
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/arch-systemtool/Arch-Systemtool.zip");
        execute_command("sudo unzip -o " + currentDir + "/Arch-Systemtool.zip -d " + target_folder + "/opt/Arch-Systemtool");
        execute_command("sudo unzip -o " + currentDir + "/spitfire-minimal.zip -d " + target_folder + "/home/" + new_username + "/");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/kde_settings.conf " + target_folder + "/etc/sddm.conf.d/kde_settings.conf");
        execute_command("sudo cp " + currentDir + "/spitfire-ckge-minimal/tweaksspitfire.sh " + target_folder + "/opt/tweaksspitfire.sh");
        execute_command("sudo chmod +x " + target_folder + "/opt/tweaksspitfire.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"su - " + new_username + " -c 'cd /opt && ./tweaksspitfire.sh " + new_username + "'\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/konsolerc " + target_folder + "/home/" + new_username + "/.config/konsolerc");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/SpitFireLogin " + target_folder + "/usr/share/sddm/themes/SpitFireLogin");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/claudemods-cyan.colorscheme " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.colorscheme");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/claudemods-cyan.profile " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.profile");
        execute_command("sudo rm -rf " + currentDir + "/Arch-Systemtool.zip");
        execute_command("sudo rm -rf " + currentDir + "/spitfire-minimal.zip");
        execute_command("sudo rm -rf " + target_folder + "/opt/tweaksspitfire.sh");

        // Fix user-places.xbel
        std::string cmd = "sudo ls -1 " + target_folder + "/home | grep -v '^\\.' | head -1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            std::string home_folder;
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                home_folder = buffer;
                home_folder.erase(std::remove(home_folder.begin(), home_folder.end(), '\n'), home_folder.end());
            }
            pclose(pipe);

            if (!home_folder.empty()) {
                std::string user_places_file = target_folder + "/home/" + home_folder + "/.local/share/user-places.xbel";
                std::string sed_cmd = "sudo sed -i 's/spitfire/" + home_folder + "/g' " + user_places_file;
                execute_command(sed_cmd);
            }
        }

        unmount_system_dirs();
        std::cout << COLOR_GREEN << "Spitfire CKGE Minimal Dev installation completed in: " << target_folder << COLOR_RESET << std::endl;

        // CREATE SQUASHFS IMAGE AFTER INSTALLATION
        create_squashfs_image("Spitfire-CKGE-Minimal-Dev");
    }

    // SPITFIRE CKGE FULL - FIXED
    void install_spitfire_ckge_full() {
        current_distro_name = "Spitfire-CKGE-Full";
        if (!check_settings_configured()) {
            std::cout << COLOR_RED << "Cannot proceed with installation. Please configure all settings first." << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_ORANGE << "Installing Spitfire CKGE Full..." << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Starting Spitfire CKGE Full installation in: " << target_folder << COLOR_RESET << std::endl;

        std::string currentDir = getCurrentDir();
        std::cout << COLOR_CYAN << "Working from directory: " << currentDir << COLOR_RESET << std::endl;

        // CREATE TARGET DIRECTORY FIRST
        std::cout << COLOR_CYAN << "Creating target directory: " << target_folder << COLOR_RESET << std::endl;
        execute_command("sudo mkdir -p " + target_folder);

        // VERIFY DIRECTORY WAS CREATED
        struct stat info;
        if (stat(target_folder.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "Failed to create target directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        if (!(info.st_mode & S_IFDIR)) {
            std::cerr << COLOR_RED << "Target path is not a directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_GREEN << "Target directory created successfully!" << COLOR_RESET << std::endl;

        // CREATE ESSENTIAL DIRECTORIES BEFORE COPYING FILES
        execute_command("sudo mkdir -p " + target_folder + "/etc/pacman.d");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/grub/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/plymouth/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/local/bin");
        execute_command("sudo mkdir -p " + target_folder + "/etc/systemd/system");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");

        // VERIFY AND COPY FILES WITH EXACT NAMES
        execute_command("sudo cp -r " + currentDir + "/vconsole.conf " + target_folder + "/etc/vconsole.conf");
        execute_command("sudo cp -r /etc/resolv.conf " + target_folder + "/etc/resolv.conf");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d " + target_folder + "/etc/pacman.d");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d /etc/pacman.d");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf " + target_folder + "/etc/pacman.conf");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf /etc/pacman.conf");

        execute_command("sudo pacman -Sy");

        // INSTALL BASE SYSTEM WITH PACSTRAP
        std::cout << COLOR_CYAN << "Installing base system with pacstrap..." << COLOR_RESET << std::endl;
        execute_command("sudo pacstrap " + target_folder + " claudemods-desktop-full");

        // VERIFY PACSTRAP SUCCESS
        std::string test_bin = target_folder + "/bin/bash";
        if (stat(test_bin.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "pacstrap failed! /bin/bash not found in target." << COLOR_RESET << std::endl;
            return;
        }

        execute_command("sudo mkdir -p " + target_folder + "/boot");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo touch " + target_folder + "/boot/grub/grub.cfg.new");

        mount_system_dirs();
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable sddm\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable NetworkManager\"");

        apply_timezone_keyboard_settings();

        // FIXED FILE PATHS - ENSURE CORRECT NAMES
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/grub " + target_folder + "/etc/default/grub");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/grub.cfg " + target_folder + "/boot/grub/grub.cfg");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos " + target_folder + "/usr/share/grub/themes/cachyos");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"grub-mkconfig -o /boot/grub/grub.cfg\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos-bootanimation " + target_folder + "/usr/share/plymouth/themes/cachyos-bootanimation");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/termfull.sh " + target_folder + "/usr/local/bin/termfull.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/local/bin/termfull.sh\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/termfull.service " + target_folder + "/etc/systemd/system/termfull.service");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable termfull.service >/dev/null 2>&1\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"plymouth-set-default-theme -R cachyos-bootanimation\"");

        create_user();

        // CREATE USER HOME DIRECTORY STRUCTURE
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.config/fish");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share/konsole");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share");

        execute_command("sudo chmod +x " + target_folder + "/home/" + new_username + "/.config/fish/config.fish");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/share/fish/config.fish\"");

        execute_command("cd " + target_folder);
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/claudemods-desktop/spitfire-full.zip");
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/arch-systemtool/Arch-Systemtool.zip");
        execute_command("sudo unzip -o " + currentDir + "/Arch-Systemtool.zip -d " + target_folder + "/opt/Arch-Systemtool");
        execute_command("sudo unzip -o " + currentDir + "/spitfire-full.zip -d " + target_folder + "/home/" + new_username + "/");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/kde_settings.conf " + target_folder + "/etc/sddm.conf.d/kde_settings.conf");
        execute_command("sudo cp " + currentDir + "/spitfire-ckge-minimal/tweaksspitfire.sh " + target_folder + "/opt/tweaksspitfire.sh");
        execute_command("sudo chmod +x " + target_folder + "/opt/tweaksspitfire.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"su - " + new_username + " -c 'cd /opt && ./tweaksspitfire.sh " + new_username + "'\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/konsolerc " + target_folder + "/home/" + new_username + "/.config/konsolerc");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/SpitFireLogin " + target_folder + "/usr/share/sddm/themes/SpitFireLogin");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/claudemods-cyan.colorscheme " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.colorscheme");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/claudemods-cyan.profile " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.profile");
        execute_command("sudo rm -rf " + currentDir + "/Arch-Systemtool.zip");
        execute_command("sudo rm -rf " + currentDir + "/spitfire-full.zip");
        execute_command("sudo rm -rf " + target_folder + "/opt/tweaksspitfire.sh");

        // Fix user-places.xbel
        std::string cmd = "sudo ls -1 " + target_folder + "/home | grep -v '^\\.' | head -1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            std::string home_folder;
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                home_folder = buffer;
                home_folder.erase(std::remove(home_folder.begin(), home_folder.end(), '\n'), home_folder.end());
            }
            pclose(pipe);

            if (!home_folder.empty()) {
                std::string user_places_file = target_folder + "/home/" + home_folder + "/.local/share/user-places.xbel";
                std::string sed_cmd = "sudo sed -i 's/spitfire/" + home_folder + "/g' " + user_places_file;
                execute_command(sed_cmd);
            }
        }

        unmount_system_dirs();
        std::cout << COLOR_GREEN << "Spitfire CKGE Full installation completed in: " << target_folder << COLOR_RESET << std::endl;

        // CREATE SQUASHFS IMAGE AFTER INSTALLATION
        create_squashfs_image("Spitfire-CKGE-Full");
    }

    // SPITFIRE CKGE FULL DEV - FIXED
    void install_spitfire_ckge_full_dev() {
        current_distro_name = "Spitfire-CKGE-Full-Dev";
        if (!check_settings_configured()) {
            std::cout << COLOR_RED << "Cannot proceed with installation. Please configure all settings first." << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_ORANGE << "Installing Spitfire CKGE Full Dev..." << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Starting Spitfire CKGE Full Dev installation in: " << target_folder << COLOR_RESET << std::endl;

        std::string currentDir = getCurrentDir();
        std::cout << COLOR_CYAN << "Working from directory: " << currentDir << COLOR_RESET << std::endl;

        // CREATE TARGET DIRECTORY FIRST
        std::cout << COLOR_CYAN << "Creating target directory: " << target_folder << COLOR_RESET << std::endl;
        execute_command("sudo mkdir -p " + target_folder);

        // VERIFY DIRECTORY WAS CREATED
        struct stat info;
        if (stat(target_folder.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "Failed to create target directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        if (!(info.st_mode & S_IFDIR)) {
            std::cerr << COLOR_RED << "Target path is not a directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_GREEN << "Target directory created successfully!" << COLOR_RESET << std::endl;

        // CREATE ESSENTIAL DIRECTORIES BEFORE COPYING FILES
        execute_command("sudo mkdir -p " + target_folder + "/etc/pacman.d");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/grub/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/plymouth/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/local/bin");
        execute_command("sudo mkdir -p " + target_folder + "/etc/systemd/system");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");

        // VERIFY AND COPY FILES WITH EXACT NAMES
        execute_command("sudo cp -r " + currentDir + "/vconsole.conf " + target_folder + "/etc/vconsole.conf");
        execute_command("sudo cp -r /etc/resolv.conf " + target_folder + "/etc/resolv.conf");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d " + target_folder + "/etc/pacman.d");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d /etc/pacman.d");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf " + target_folder + "/etc/pacman.conf");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf /etc/pacman.conf");

        execute_command("sudo pacman -Sy");

        // INSTALL BASE SYSTEM WITH PACSTRAP
        std::cout << COLOR_CYAN << "Installing base system with pacstrap..." << COLOR_RESET << std::endl;
        execute_command("sudo pacstrap " + target_folder + " claudemods-desktop-fulldev");

        // VERIFY PACSTRAP SUCCESS
        std::string test_bin = target_folder + "/bin/bash";
        if (stat(test_bin.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "pacstrap failed! /bin/bash not found in target." << COLOR_RESET << std::endl;
            return;
        }

        execute_command("sudo mkdir -p " + target_folder + "/boot");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo touch " + target_folder + "/boot/grub/grub.cfg.new");

        mount_system_dirs();
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable sddm\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable NetworkManager\"");

        apply_timezone_keyboard_settings();

        // FIXED FILE PATHS - ENSURE CORRECT NAMES
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/grub " + target_folder + "/etc/default/grub");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/grub.cfg " + target_folder + "/boot/grub/grub.cfg");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos " + target_folder + "/usr/share/grub/themes/cachyos");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"grub-mkconfig -o /boot/grub/grub.cfg\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos-bootanimation " + target_folder + "/usr/share/plymouth/themes/cachyos-bootanimation");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/termfull.sh " + target_folder + "/usr/local/bin/termfull.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/local/bin/termfull.sh\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/termfull.service " + target_folder + "/etc/systemd/system/termfull.service");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable termfull.service >/dev/null 2>&1\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"plymouth-set-default-theme -R cachyos-bootanimation\"");

        create_user();

        // CREATE USER HOME DIRECTORY STRUCTURE
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.config/fish");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share/konsole");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share");

        execute_command("sudo chmod +x " + target_folder + "/home/" + new_username + "/.config/fish/config.fish");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/share/fish/config.fish\"");

        execute_command("cd " + target_folder);
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/claudemods-desktop/spitfire-full.zip");
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/arch-systemtool/Arch-Systemtool.zip");
        execute_command("sudo unzip -o " + currentDir + "/Arch-Systemtool.zip -d " + target_folder + "/opt/Arch-Systemtool");
        execute_command("sudo unzip -o " + currentDir + "/spitfire-full.zip -d " + target_folder + "/home/" + new_username + "/");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/kde_settings.conf " + target_folder + "/etc/sddm.conf.d/kde_settings.conf");
        execute_command("sudo cp " + currentDir + "/spitfire-ckge-minimal/tweaksspitfire.sh " + target_folder + "/opt/tweaksspitfire.sh");
        execute_command("sudo chmod +x " + target_folder + "/opt/tweaksspitfire.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"su - " + new_username + " -c 'cd /opt && ./tweaksspitfire.sh " + new_username + "'\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/konsolerc " + target_folder + "/home/" + new_username + "/.config/konsolerc");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/SpitFireLogin " + target_folder + "/usr/share/sddm/themes/SpitFireLogin");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/claudemods-cyan.colorscheme " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.colorscheme");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/claudemods-cyan.profile " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.profile");
        execute_command("sudo rm -rf " + currentDir + "/Arch-Systemtool.zip");
        execute_command("sudo rm -rf " + currentDir + "/spitfire-full.zip");
        execute_command("sudo rm -rf " + target_folder + "/opt/tweaksspitfire.sh");

        // Fix user-places.xbel
        std::string cmd = "sudo ls -1 " + target_folder + "/home | grep -v '^\\.' | head -1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            std::string home_folder;
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                home_folder = buffer;
                home_folder.erase(std::remove(home_folder.begin(), home_folder.end(), '\n'), home_folder.end());
            }
            pclose(pipe);

            if (!home_folder.empty()) {
                std::string user_places_file = target_folder + "/home/" + home_folder + "/.local/share/user-places.xbel";
                std::string sed_cmd = "sudo sed -i 's/spitfire/" + home_folder + "/g' " + user_places_file;
                execute_command(sed_cmd);
            }
        }

        unmount_system_dirs();
        std::cout << COLOR_GREEN << "Spitfire CKGE Full Dev installation completed in: " << target_folder << COLOR_RESET << std::endl;

        // CREATE SQUASHFS IMAGE AFTER INSTALLATION
        create_squashfs_image("Spitfire-CKGE-Full-Dev");
    }

    // APEX CKGE MINIMAL - FIXED
    void install_apex_ckge_minimal() {
        current_distro_name = "Apex-CKGE-Minimal";
        if (!check_settings_configured()) {
            std::cout << COLOR_RED << "Cannot proceed with installation. Please configure all settings first." << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_PURPLE << "Installing Apex CKGE Minimal..." << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Starting Apex CKGE Minimal installation in: " << target_folder << COLOR_RESET << std::endl;

        std::string currentDir = getCurrentDir();
        std::cout << COLOR_CYAN << "Working from directory: " << currentDir << COLOR_RESET << std::endl;

        // CREATE TARGET DIRECTORY FIRST
        std::cout << COLOR_CYAN << "Creating target directory: " << target_folder << COLOR_RESET << std::endl;
        execute_command("sudo mkdir -p " + target_folder);

        // VERIFY DIRECTORY WAS CREATED
        struct stat info;
        if (stat(target_folder.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "Failed to create target directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        if (!(info.st_mode & S_IFDIR)) {
            std::cerr << COLOR_RED << "Target path is not a directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_GREEN << "Target directory created successfully!" << COLOR_RESET << std::endl;

        // CREATE ESSENTIAL DIRECTORIES BEFORE COPYING FILES
        execute_command("sudo mkdir -p " + target_folder + "/etc/pacman.d");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/grub/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/plymouth/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/local/bin");
        execute_command("sudo mkdir -p " + target_folder + "/etc/systemd/system");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");

        // VERIFY AND COPY FILES WITH EXACT NAMES
        execute_command("sudo cp -r " + currentDir + "/vconsole.conf " + target_folder + "/etc/vconsole.conf");
        execute_command("sudo cp -r /etc/resolv.conf " + target_folder + "/etc/resolv.conf");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d " + target_folder + "/etc/pacman.d");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d /etc/pacman.d");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf " + target_folder + "/etc/pacman.conf");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf /etc/pacman.conf");

        execute_command("sudo pacman -Sy");

        // INSTALL BASE SYSTEM WITH PACSTRAP
        std::cout << COLOR_CYAN << "Installing base system with pacstrap..." << COLOR_RESET << std::endl;
        execute_command("sudo pacstrap " + target_folder + " claudemods-desktop");

        // VERIFY PACSTRAP SUCCESS
        std::string test_bin = target_folder + "/bin/bash";
        if (stat(test_bin.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "pacstrap failed! /bin/bash not found in target." << COLOR_RESET << std::endl;
            return;
        }

        execute_command("sudo mkdir -p " + target_folder + "/boot");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo touch " + target_folder + "/boot/grub/grub.cfg.new");

        mount_system_dirs();
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable sddm\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable NetworkManager\"");

        apply_timezone_keyboard_settings();

        // FIXED FILE PATHS - ENSURE CORRECT NAMES
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/grub " + target_folder + "/etc/default/grub");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/grub.cfg " + target_folder + "/boot/grub/grub.cfg");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/cachyos " + target_folder + "/usr/share/grub/themes/cachyos");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"grub-mkconfig -o /boot/grub/grub.cfg\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos-bootanimation " + target_folder + "/usr/share/plymouth/themes/cachyos-bootanimation");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/term.sh " + target_folder + "/usr/local/bin/term.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/local/bin/term.sh\"");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/term.service " + target_folder + "/etc/systemd/system/term.service");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable term.service >/dev/null 2>&1\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"plymouth-set-default-theme -R cachyos-bootanimation\"");

        create_user();

        // CREATE USER HOME DIRECTORY STRUCTURE
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.config/fish");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share/konsole");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share");

        execute_command("sudo chmod +x " + target_folder + "/home/" + new_username + "/.config/fish/config.fish");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/share/fish/config.fish\"");

        execute_command("cd " + target_folder);
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/claudemods-desktop/apex-minimal.zip");
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/arch-systemtool/Arch-Systemtool.zip");
        execute_command("sudo unzip -o " + currentDir + "/Arch-Systemtool.zip -d " + target_folder + "/opt/Arch-Systemtool");
        execute_command("sudo unzip -o " + currentDir + "/apex-minimal.zip -d " + target_folder + "/home/" + new_username + "/");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/kde_settings.conf " + target_folder + "/etc/sddm.conf.d/kde_settings.conf");
        execute_command("sudo cp " + currentDir + "/apex-ckge-minimal/tweaksapex.sh " + target_folder + "/opt/tweaksapex.sh");
        execute_command("sudo chmod +x " + target_folder + "/opt/tweaksapex.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"su - " + new_username + " -c 'cd /opt && ./tweaksapex.sh " + new_username + "'\"");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/konsolerc " + target_folder + "/home/" + new_username + "/.config/konsolerc");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/ApexLogin2 " + target_folder + "/usr/share/sddm/themes/ApexLogin2");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/claudemods-cyan.colorscheme " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.colorscheme");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/claudemods-cyan.profile " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.profile");
        execute_command("sudo rm -rf " + currentDir + "/Arch-Systemtool.zip");
        execute_command("sudo rm -rf " + currentDir + "/apex-minimal.zip");
        execute_command("sudo rm -rf " + target_folder + "/opt/tweaksapex.sh");

        // Fix user-places.xbel for APEX
        std::string cmd = "sudo ls -1 " + target_folder + "/home | grep -v '^\\.' | head -1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            std::string home_folder;
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                home_folder = buffer;
                home_folder.erase(std::remove(home_folder.begin(), home_folder.end(), '\n'), home_folder.end());
            }
            pclose(pipe);

            if (!home_folder.empty()) {
                std::string user_places_file = target_folder + "/home/" + home_folder + "/.local/share/user-places.xbel";
                std::string sed_cmd = "sudo sed -i 's/apex/" + home_folder + "/g' " + user_places_file;
                execute_command(sed_cmd);
            }
        }

        unmount_system_dirs();
        std::cout << COLOR_GREEN << "Apex CKGE Minimal installation completed in: " << target_folder << COLOR_RESET << std::endl;

        // CREATE SQUASHFS IMAGE AFTER INSTALLATION
        create_squashfs_image("Apex-CKGE-Minimal");
    }

    // APEX CKGE MINIMAL DEV - FIXED
    void install_apex_ckge_minimal_dev() {
        current_distro_name = "Apex-CKGE-Minimal-Dev";
        if (!check_settings_configured()) {
            std::cout << COLOR_RED << "Cannot proceed with installation. Please configure all settings first." << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_PURPLE << "Installing Apex CKGE Minimal Dev..." << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Starting Apex CKGE Minimal Dev installation in: " << target_folder << COLOR_RESET << std::endl;

        std::string currentDir = getCurrentDir();
        std::cout << COLOR_CYAN << "Working from directory: " << currentDir << COLOR_RESET << std::endl;

        // CREATE TARGET DIRECTORY FIRST
        std::cout << COLOR_CYAN << "Creating target directory: " << target_folder << COLOR_RESET << std::endl;
        execute_command("sudo mkdir -p " + target_folder);

        // VERIFY DIRECTORY WAS CREATED
        struct stat info;
        if (stat(target_folder.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "Failed to create target directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        if (!(info.st_mode & S_IFDIR)) {
            std::cerr << COLOR_RED << "Target path is not a directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_GREEN << "Target directory created successfully!" << COLOR_RESET << std::endl;

        // CREATE ESSENTIAL DIRECTORIES BEFORE COPYING FILES
        execute_command("sudo mkdir -p " + target_folder + "/etc/pacman.d");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/grub/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/plymouth/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/local/bin");
        execute_command("sudo mkdir -p " + target_folder + "/etc/systemd/system");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");

        // VERIFY AND COPY FILES WITH EXACT NAMES
        execute_command("sudo cp -r " + currentDir + "/vconsole.conf " + target_folder + "/etc/vconsole.conf");
        execute_command("sudo cp -r /etc/resolv.conf " + target_folder + "/etc/resolv.conf");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d " + target_folder + "/etc/pacman.d");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d /etc/pacman.d");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf " + target_folder + "/etc/pacman.conf");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf /etc/pacman.conf");

        execute_command("sudo pacman -Sy");

        // INSTALL BASE SYSTEM WITH PACSTRAP
        std::cout << COLOR_CYAN << "Installing base system with pacstrap..." << COLOR_RESET << std::endl;
        execute_command("sudo pacstrap " + target_folder + " claudemods-desktop-dev");

        // VERIFY PACSTRAP SUCCESS
        std::string test_bin = target_folder + "/bin/bash";
        if (stat(test_bin.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "pacstrap failed! /bin/bash not found in target." << COLOR_RESET << std::endl;
            return;
        }

        execute_command("sudo mkdir -p " + target_folder + "/boot");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo touch " + target_folder + "/boot/grub/grub.cfg.new");

        mount_system_dirs();
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable sddm\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable NetworkManager\"");

        apply_timezone_keyboard_settings();

        // FIXED FILE PATHS - ENSURE CORRECT NAMES
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/grub " + target_folder + "/etc/default/grub");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/grub.cfg " + target_folder + "/boot/grub/grub.cfg");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/cachyos " + target_folder + "/usr/share/grub/themes/cachyos");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"grub-mkconfig -o /boot/grub/grub.cfg\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos-bootanimation " + target_folder + "/usr/share/plymouth/themes/cachyos-bootanimation");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/term.sh " + target_folder + "/usr/local/bin/term.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/local/bin/term.sh\"");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/term.service " + target_folder + "/etc/systemd/system/term.service");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable term.service >/dev/null 2>&1\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"plymouth-set-default-theme -R cachyos-bootanimation\"");

        create_user();

        // CREATE USER HOME DIRECTORY STRUCTURE
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.config/fish");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share/konsole");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share");

        execute_command("sudo chmod +x " + target_folder + "/home/" + new_username + "/.config/fish/config.fish");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/share/fish/config.fish\"");

        execute_command("cd " + target_folder);
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/claudemods-desktop/apex-minimal.zip");
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/arch-systemtool/Arch-Systemtool.zip");
        execute_command("sudo unzip -o " + currentDir + "/Arch-Systemtool.zip -d " + target_folder + "/opt/Arch-Systemtool");
        execute_command("sudo unzip -o " + currentDir + "/apex-minimal.zip -d " + target_folder + "/home/" + new_username + "/");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/kde_settings.conf " + target_folder + "/etc/sddm.conf.d/kde_settings.conf");
        execute_command("sudo cp " + currentDir + "/apex-ckge-minimal/tweaksapex.sh " + target_folder + "/opt/tweaksapex.sh");
        execute_command("sudo chmod +x " + target_folder + "/opt/tweaksapex.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"su - " + new_username + " -c 'cd /opt && ./tweaksapex.sh " + new_username + "'\"");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/konsolerc " + target_folder + "/home/" + new_username + "/.config/konsolerc");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/ApexLogin2 " + target_folder + "/usr/share/sddm/themes/ApexLogin2");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/claudemods-cyan.colorscheme " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.colorscheme");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/claudemods-cyan.profile " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.profile");
        execute_command("sudo rm -rf " + currentDir + "/Arch-Systemtool.zip");
        execute_command("sudo rm -rf " + currentDir + "/apex-minimal.zip");
        execute_command("sudo rm -rf " + target_folder + "/opt/tweaksapex.sh");

        // Fix user-places.xbel for APEX
        std::string cmd = "sudo ls -1 " + target_folder + "/home | grep -v '^\\.' | head -1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            std::string home_folder;
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                home_folder = buffer;
                home_folder.erase(std::remove(home_folder.begin(), home_folder.end(), '\n'), home_folder.end());
            }
            pclose(pipe);

            if (!home_folder.empty()) {
                std::string user_places_file = target_folder + "/home/" + home_folder + "/.local/share/user-places.xbel";
                std::string sed_cmd = "sudo sed -i 's/apex/" + home_folder + "/g' " + user_places_file;
                execute_command(sed_cmd);
            }
        }

        unmount_system_dirs();
        std::cout << COLOR_GREEN << "Apex CKGE Minimal Dev installation completed in: " << target_folder << COLOR_RESET << std::endl;

        // CREATE SQUASHFS IMAGE AFTER INSTALLATION
        create_squashfs_image("Apex-CKGE-Minimal-Dev");
    }

    // APEX CKGE FULL - FIXED
    void install_apex_ckge_full() {
        current_distro_name = "Apex-CKGE-Full";
        if (!check_settings_configured()) {
            std::cout << COLOR_RED << "Cannot proceed with installation. Please configure all settings first." << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_PURPLE << "Installing Apex CKGE Full..." << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Starting Apex CKGE Full installation in: " << target_folder << COLOR_RESET << std::endl;

        std::string currentDir = getCurrentDir();
        std::cout << COLOR_CYAN << "Working from directory: " << currentDir << COLOR_RESET << std::endl;

        // CREATE TARGET DIRECTORY FIRST
        std::cout << COLOR_CYAN << "Creating target directory: " << target_folder << COLOR_RESET << std::endl;
        execute_command("sudo mkdir -p " + target_folder);

        // VERIFY DIRECTORY WAS CREATED
        struct stat info;
        if (stat(target_folder.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "Failed to create target directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        if (!(info.st_mode & S_IFDIR)) {
            std::cerr << COLOR_RED << "Target path is not a directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_GREEN << "Target directory created successfully!" << COLOR_RESET << std::endl;

        // CREATE ESSENTIAL DIRECTORIES BEFORE COPYING FILES
        execute_command("sudo mkdir -p " + target_folder + "/etc/pacman.d");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/grub/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/plymouth/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/local/bin");
        execute_command("sudo mkdir -p " + target_folder + "/etc/systemd/system");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");

        // VERIFY AND COPY FILES WITH EXACT NAMES
        execute_command("sudo cp -r " + currentDir + "/vconsole.conf " + target_folder + "/etc/vconsole.conf");
        execute_command("sudo cp -r /etc/resolv.conf " + target_folder + "/etc/resolv.conf");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d " + target_folder + "/etc/pacman.d");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d /etc/pacman.d");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf " + target_folder + "/etc/pacman.conf");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf /etc/pacman.conf");

        execute_command("sudo pacman -Sy");

        // INSTALL BASE SYSTEM WITH PACSTRAP
        std::cout << COLOR_CYAN << "Installing base system with pacstrap..." << COLOR_RESET << std::endl;
        execute_command("sudo pacstrap " + target_folder + " claudemods-desktop-full");

        // VERIFY PACSTRAP SUCCESS
        std::string test_bin = target_folder + "/bin/bash";
        if (stat(test_bin.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "pacstrap failed! /bin/bash not found in target." << COLOR_RESET << std::endl;
            return;
        }

        execute_command("sudo mkdir -p " + target_folder + "/boot");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo touch " + target_folder + "/boot/grub/grub.cfg.new");

        mount_system_dirs();
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable sddm\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable NetworkManager\"");

        apply_timezone_keyboard_settings();

        // FIXED FILE PATHS - ENSURE CORRECT NAMES
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/grub " + target_folder + "/etc/default/grub");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/grub.cfg " + target_folder + "/boot/grub/grub.cfg");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/cachyos " + target_folder + "/usr/share/grub/themes/cachyos");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"grub-mkconfig -o /boot/grub/grub.cfg\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos-bootanimation " + target_folder + "/usr/share/plymouth/themes/cachyos-bootanimation");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/termfull.sh " + target_folder + "/usr/local/bin/termfull.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/local/bin/termfull.sh\"");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/termfull.service " + target_folder + "/etc/systemd/system/termfull.service");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable termfull.service >/dev/null 2>&1\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"plymouth-set-default-theme -R cachyos-bootanimation\"");

        create_user();

        // CREATE USER HOME DIRECTORY STRUCTURE
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.config/fish");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share/konsole");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share");

        execute_command("sudo chmod +x " + target_folder + "/home/" + new_username + "/.config/fish/config.fish");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/share/fish/config.fish\"");

        execute_command("cd " + target_folder);
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/claudemods-desktop/apex-full.zip");
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/arch-systemtool/Arch-Systemtool.zip");
        execute_command("sudo unzip -o " + currentDir + "/Arch-Systemtool.zip -d " + target_folder + "/opt/Arch-Systemtool");
        execute_command("sudo unzip -o " + currentDir + "/apex-full.zip -d " + target_folder + "/home/" + new_username + "/");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/kde_settings.conf " + target_folder + "/etc/sddm.conf.d/kde_settings.conf");
        execute_command("sudo cp " + currentDir + "/apex-ckge-minimal/tweaksapex.sh " + target_folder + "/opt/tweaksapex.sh");
        execute_command("sudo chmod +x " + target_folder + "/opt/tweaksapex.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"su - " + new_username + " -c 'cd /opt && ./tweaksapex.sh " + new_username + "'\"");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/konsolerc " + target_folder + "/home/" + new_username + "/.config/konsolerc");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/ApexLogin2 " + target_folder + "/usr/share/sddm/themes/ApexLogin2");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/claudemods-cyan.colorscheme " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.colorscheme");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/claudemods-cyan.profile " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.profile");
        execute_command("sudo rm -rf " + currentDir + "/Arch-Systemtool.zip");
        execute_command("sudo rm -rf " + currentDir + "/apex-full.zip");
        execute_command("sudo rm -rf " + target_folder + "/opt/tweaksapex.sh");

        // Fix user-places.xbel for APEX
        std::string cmd = "sudo ls -1 " + target_folder + "/home | grep -v '^\\.' | head -1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            std::string home_folder;
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                home_folder = buffer;
                home_folder.erase(std::remove(home_folder.begin(), home_folder.end(), '\n'), home_folder.end());
            }
            pclose(pipe);

            if (!home_folder.empty()) {
                std::string user_places_file = target_folder + "/home/" + home_folder + "/.local/share/user-places.xbel";
                std::string sed_cmd = "sudo sed -i 's/apex/" + home_folder + "/g' " + user_places_file;
                execute_command(sed_cmd);
            }
        }

        unmount_system_dirs();
        std::cout << COLOR_GREEN << "Apex CKGE Full installation completed in: " << target_folder << COLOR_RESET << std::endl;

        // CREATE SQUASHFS IMAGE AFTER INSTALLATION
        create_squashfs_image("Apex-CKGE-Full");
    }

    // APEX CKGE FULL DEV - FIXED
    void install_apex_ckge_full_dev() {
        current_distro_name = "Apex-CKGE-Full-Dev";
        if (!check_settings_configured()) {
            std::cout << COLOR_RED << "Cannot proceed with installation. Please configure all settings first." << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_PURPLE << "Installing Apex CKGE Full Dev..." << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "Starting Apex CKGE Full Dev installation in: " << target_folder << COLOR_RESET << std::endl;

        std::string currentDir = getCurrentDir();
        std::cout << COLOR_CYAN << "Working from directory: " << currentDir << COLOR_RESET << std::endl;

        // CREATE TARGET DIRECTORY FIRST
        std::cout << COLOR_CYAN << "Creating target directory: " << target_folder << COLOR_RESET << std::endl;
        execute_command("sudo mkdir -p " + target_folder);

        // VERIFY DIRECTORY WAS CREATED
        struct stat info;
        if (stat(target_folder.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "Failed to create target directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        if (!(info.st_mode & S_IFDIR)) {
            std::cerr << COLOR_RED << "Target path is not a directory: " << target_folder << COLOR_RESET << std::endl;
            return;
        }

        std::cout << COLOR_GREEN << "Target directory created successfully!" << COLOR_RESET << std::endl;

        // CREATE ESSENTIAL DIRECTORIES BEFORE COPYING FILES
        execute_command("sudo mkdir -p " + target_folder + "/etc/pacman.d");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/grub/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/share/plymouth/themes");
        execute_command("sudo mkdir -p " + target_folder + "/usr/local/bin");
        execute_command("sudo mkdir -p " + target_folder + "/etc/systemd/system");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");

        // VERIFY AND COPY FILES WITH EXACT NAMES
        execute_command("sudo cp -r " + currentDir + "/vconsole.conf " + target_folder + "/etc/vconsole.conf");
        execute_command("sudo cp -r /etc/resolv.conf " + target_folder + "/etc/resolv.conf");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d " + target_folder + "/etc/pacman.d");
        execute_command("sudo unzip -o " + currentDir + "/pacman.d.zip -d /etc/pacman.d");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf " + target_folder + "/etc/pacman.conf");
        execute_command("sudo cp -r " + currentDir + "/pacman.conf /etc/pacman.conf");

        execute_command("sudo pacman -Sy");

        // INSTALL BASE SYSTEM WITH PACSTRAP
        std::cout << COLOR_CYAN << "Installing base system with pacstrap..." << COLOR_RESET << std::endl;
        execute_command("sudo pacstrap " + target_folder + " claudemods-desktop-fulldev");

        // VERIFY PACSTRAP SUCCESS
        std::string test_bin = target_folder + "/bin/bash";
        if (stat(test_bin.c_str(), &info) != 0) {
            std::cerr << COLOR_RED << "pacstrap failed! /bin/bash not found in target." << COLOR_RESET << std::endl;
            return;
        }

        execute_command("sudo mkdir -p " + target_folder + "/boot");
        execute_command("sudo mkdir -p " + target_folder + "/boot/grub");
        execute_command("sudo touch " + target_folder + "/boot/grub/grub.cfg.new");

        mount_system_dirs();
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable sddm\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable NetworkManager\"");

        apply_timezone_keyboard_settings();

        // FIXED FILE PATHS - ENSURE CORRECT NAMES
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/grub " + target_folder + "/etc/default/grub");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/grub.cfg " + target_folder + "/boot/grub/grub.cfg");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/cachyos " + target_folder + "/usr/share/grub/themes/cachyos");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"grub-mkconfig -o /boot/grub/grub.cfg\"");
        execute_command("sudo cp -r " + currentDir + "/spitfire-ckge-minimal/cachyos-bootanimation " + target_folder + "/usr/share/plymouth/themes/cachyos-bootanimation");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/termfull.sh " + target_folder + "/usr/local/bin/termfull.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/local/bin/termfull.sh\"");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/termfull.service " + target_folder + "/etc/systemd/system/termfull.service");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"systemctl enable termfull.service >/dev/null 2>&1\"");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"plymouth-set-default-theme -R cachyos-bootanimation\"");

        create_user();

        // CREATE USER HOME DIRECTORY STRUCTURE
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.config/fish");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share/konsole");
        execute_command("sudo mkdir -p " + target_folder + "/home/" + new_username + "/.local/share");

        execute_command("sudo chmod +x " + target_folder + "/home/" + new_username + "/.config/fish/config.fish");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"chmod +x /usr/share/fish/config.fish\"");

        execute_command("cd " + target_folder);
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/claudemods-desktop/apex-full.zip");
        execute_command("sudo wget --show-progress --no-check-certificate --continue --tries=10 --timeout=30 --waitretry=5 https://claudemodsreloaded.co.uk/arch-systemtool/Arch-Systemtool.zip");
        execute_command("sudo unzip -o " + currentDir + "/Arch-Systemtool.zip -d " + target_folder + "/opt/Arch-Systemtool");
        execute_command("sudo unzip -o " + currentDir + "/apex-full.zip -d " + target_folder + "/home/" + new_username + "/");
        execute_command("sudo mkdir -p " + target_folder + "/etc/sddm.conf.d");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/kde_settings.conf " + target_folder + "/etc/sddm.conf.d/kde_settings.conf");
        execute_command("sudo cp " + currentDir + "/apex-ckge-minimal/tweaksapex.sh " + target_folder + "/opt/tweaksapex.sh");
        execute_command("sudo chmod +x " + target_folder + "/opt/tweaksapex.sh");
        execute_command("sudo chroot " + target_folder + " /bin/bash -c \"su - " + new_username + " -c 'cd /opt && ./tweaksapex.sh " + new_username + "'\"");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/konsolerc " + target_folder + "/home/" + new_username + "/.config/konsolerc");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/ApexLogin2 " + target_folder + "/usr/share/sddm/themes/ApexLogin2");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/claudemods-cyan.colorscheme " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.colorscheme");
        execute_command("sudo cp -r " + currentDir + "/apex-ckge-minimal/claudemods-cyan.profile " + target_folder + "/home/" + new_username + "/.local/share/konsole/claudemods-cyan.profile");
        execute_command("sudo rm -rf " + currentDir + "/Arch-Systemtool.zip");
        execute_command("sudo rm -rf " + currentDir + "/apex-full.zip");
        execute_command("sudo rm -rf " + target_folder + "/opt/tweaksapex.sh");

        // Fix user-places.xbel for APEX
        std::string cmd = "sudo ls -1 " + target_folder + "/home | grep -v '^\\.' | head -1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            std::string home_folder;
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                home_folder = buffer;
                home_folder.erase(std::remove(home_folder.begin(), home_folder.end(), '\n'), home_folder.end());
            }
            pclose(pipe);

            if (!home_folder.empty()) {
                std::string user_places_file = target_folder + "/home/" + home_folder + "/.local/share/user-places.xbel";
                std::string sed_cmd = "sudo sed -i 's/apex/" + home_folder + "/g' " + user_places_file;
                execute_command(sed_cmd);
            }
        }

        unmount_system_dirs();
        std::cout << COLOR_GREEN << "Apex CKGE Full Dev installation completed in: " << target_folder << COLOR_RESET << std::endl;

        // CREATE SQUASHFS IMAGE AFTER INSTALLATION
        create_squashfs_image("Apex-CKGE-Full-Dev");
    }

    // NEW: Show distro selection menu
    void show_distro_selection() {
        std::vector<std::string> distro_options = {
            "Install Spitfire CKGE Minimal",
            "Install Spitfire CKGE Minimal Dev",
            "Install Spitfire CKGE Full",
            "Install Spitfire CKGE Full Dev",
            "Install Apex CKGE Minimal",
            "Install Apex CKGE Minimal Dev",
            "Install Apex CKGE Full",
            "Install Apex CKGE Full Dev",
            "Back to Main Menu"
        };

        while (true) {
            int choice = show_menu(distro_options, "Select Distribution to Install");

            switch(choice) {
                case 0:
                    install_spitfire_ckge_minimal();
                    break;
                case 1:
                    install_spitfire_ckge_minimal_dev();
                    break;
                case 2:
                    install_spitfire_ckge_full();
                    break;
                case 3:
                    install_spitfire_ckge_full_dev();
                    break;
                case 4:
                    install_apex_ckge_minimal();
                    break;
                case 5:
                    install_apex_ckge_minimal_dev();
                    break;
                case 6:
                    install_apex_ckge_full();
                    break;
                case 7:
                    install_apex_ckge_full_dev();
                    break;
                case 8:
                    return;
            }

            std::cout << COLOR_CYAN << "Press Enter to continue..." << COLOR_RESET;
            std::cin.get();
        }
    }

    void show_main_menu() {
        std::vector<std::string> main_options = {
            "Set Installation Path",
            "Set Username",
            "Set Root Password",
            "Set User Password",
            "Set Timezone",
            "Set Keyboard Layout",
            "Select Distro to Install",
            "Exit"
        };

        while (true) {
            system("clear");
            display_header();
            display_current_settings(); // This will now show settings on main menu

            int choice = show_menu(main_options, "claudemods distribution iso creator");

            switch(choice) {
                case 0:
                    set_installation_path();
                    break;
                case 1:
                    set_username();
                    break;
                case 2:
                    set_root_password();
                    break;
                case 3:
                    set_user_password();
                    break;
                case 4:
                    set_timezone();
                    break;
                case 5:
                    set_keyboard_layout();
                    break;
                case 6:
                    show_distro_selection();
                    break;
                case 7:
                    std::cout << COLOR_GREEN << "Exiting. Goodbye!" << COLOR_RESET << std::endl;
                    return;
            }

            std::cout << COLOR_CYAN << "Press Enter to continue..." << COLOR_RESET;
            std::cin.get();
        }
    }

public:
    void run() {
        // EXTRACT FILES FIRST
        if (!extractRequiredFiles()) {
            std::cerr << COLOR_RED << "Failed to extract required files. Cannot continue." << COLOR_RESET << std::endl;
            return;
        }

        display_header();
        show_main_menu();
    }
};

int main() {
    ClaudemodsInstaller installer;
    installer.run();
    return 0;
}
