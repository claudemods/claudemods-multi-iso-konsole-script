#!/usr/bin/env ruby

require 'io/console'
require 'fileutils'

# Main menu
def main_menu
    ascii_art = "\e[34m" +
            "░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n" +
            "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n" +
            "██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n" +
            "██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n" +
            "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n" +
            "░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░\n" +
            "\n" +
            "claudemods iso configurator v1.02\e[0m"
    puts ascii_art
end

def print_blue(text)
    puts "\e[34m#{text}\e[0m"
end

# Function to run a command in the current terminal and wait for it to finish
def run_command(command)
    puts "\e[34mRunning command: #{command}\e[0m"  # Debugging output in blue
    exit_code = system(command)
    unless exit_code
        puts "\e[31mCommand Execution Error: Command '#{command}' failed with exit code #{$? >> 8}.\e[0m"
    end
end

# Function to prompt for user input
def prompt(prompt_text)
    print_blue prompt_text
    input = $stdin.gets.chomp
    return input
end

# Function to display a progress dialog (simulated)
def progress_dialog(message)
    puts "\e[33m#{message}\e[0m"
end

# Function to display a message box (simulated)
def message_box(title, message)
    puts "\e[32m#{title}\e[0m"
    puts "\e[32m#{message}\e[0m"
end

# Function to display an error message box (simulated)
def error_box(title, message)
    puts "\e[31m#{title}\e[0m"
    puts "\e[31m#{message}\e[0m"
end

# Function to find vmlinuz
def find_vmlinuz
    search_paths = ["/boot/vmlinuz-linux", "/boot/vmlinuz-linux-zen", "/boot/vmlinuz-linux-lts"]
    search_paths.each do |path|
        return path if File.exist?(path)
    end
    return ""
end

# Function to copy vmlinuz
def copy_vmlinuz
    progress_dialog("Copying Vmlinuz...")
    run_command("sudo cp /boot/vmlinuz-linux-zen /opt/claudemods-iso-konsole-script/Supported-Distros/Arch/build-image-arch/live/")
    message_box("Success", "Vmlinuz copied successfully.")
end

# Function to generate initramfs
def generate_initramfs
    progress_dialog("Generating Initramfs...")
    mkinitcpio_command = "sudo mkinitcpio -c live.conf -g Arch/build-image-arch/live/initramfs-linux.img"
    run_command(mkinitcpio_command)
    message_box("Success", "Initramfs generated successfully.")
end

# Function to edit grub.cfg
def edit_grub_cfg
    grub_cfg_path = "/opt/claudemods-iso-konsole-script/Supported-Distros/Arch/build-image-arch/boot/grub/grub.cfg"
    if File.exist?(grub_cfg_path)
        system("kate #{grub_cfg_path}")
    else
        error_box("Error", "grub.cfg not found at #{grub_cfg_path}.")
    end
end

# Function to edit syslinux.cfg
def edit_syslinux_cfg
    syslinux_cfg_path = "/opt/claudemods-iso-konsole-script/Supported-Distros/Arch/build-image-arch/isolinux/isolinux.cfg"
    if File.exist?(syslinux_cfg_path)
        system("kate #{syslinux_cfg_path}")
    else
        error_box("Error", "syslinux.cfg not found at #{syslinux_cfg_path}.")
    end
end

# Function to install dependencies for Debian
def install_dependencies
    progress_dialog("Installing dependencies...")

    # List of packages to install
    packages = [
        "cryptsetup",
        "dmeventd",
        "isolinux",
        "libaio1",
        "libc-ares2",
        "libdevmapper-event1.02.1",
        "liblvm2cmd2.03",
        "live-boot",
        "live-boot-doc",
        "live-boot-initramfs-tools",
        "live-config-systemd",
        "live-tools",
        "lvm2",
        "pxelinux",
        "syslinux",
        "syslinux-common",
        "thin-provisioning-tools",
        "squashfs-tools",
        "xorriso"
    ]

    # Install all packages
    package_list = packages.join(" ")
    run_command("sudo apt-get install -y #{package_list}")

    message_box("Success", "Dependencies installed successfully.")

    optional_dependencies_menu
end

# Function to copy vmlinuz for Debian
def copy_vmlinuz_debian
    progress_dialog("Copying vmlinuz...")
    run_command("sudo cp /boot/vmlinuz-6.1.0-27-amd64 /opt/claudemods-iso-konsole-script/Supported-Distros/Debian/build-image-debian/live/")
    message_box("Success", "Vmlinuz copied successfully.")
end

# Function to generate initrd for Debian
def generate_initrd
    progress_dialog("Generating Initramfs...")
    initramfs_command = "sudo mkinitramfs -o \"$(pwd)/Debian/build-image-debian/live/initrd.img-$(uname -r)\" \"$(uname -r)\""
    run_command(initramfs_command)
    message_box("Success", "Initramfs generated successfully.")
end

# Function to edit grub.cfg for Debian
def edit_grub_cfg_debian
    grub_cfg_path = "/opt/claudemods-iso-konsole-script/Supported-Distros/Debian/build-image-debian/boot/grub/grub.cfg"
    if File.exist?(grub_cfg_path)
        system("kate #{grub_cfg_path}")
    else
        error_box("Error", "grub.cfg not found at #{grub_cfg_path}.")
    end
end

# Function to edit isolinux.cfg for Debian
def edit_isolinux_cfg_debian
    isolinux_cfg_path = "/opt/claudemods-iso-konsole-script/Supported-Distros/Debian/build-image-debian/isolinux/isolinux.cfg"
    if File.exist?(isolinux_cfg_path)
        system("kate #{isolinux_cfg_path}")
    else
        error_box("Error", "isolinux.cfg not found at #{isolinux_cfg_path}.")
    end
end

# Function for Oracular 24.10
def oracular_24_10
    print_blue "You chose Oracular 24.10."
end

# Function to install dependencies for Oracular 24.10
def install_dependencies_oracular
    progress_dialog("Installing dependencies...")

    # List of packages to install
    packages = [
        "cryptsetup",
        "dmeventd",
        "isolinux",
        "libaio-dev",
        "libcares2",
        "libdevmapper-event1.02.1",
        "liblvm2cmd2.03",
        "live-boot",
        "live-boot-doc",
        "live-boot-initramfs-tools",
        "live-config-systemd",
        "live-tools",
        "lvm2",
        "pxelinux",
        "syslinux",
        "syslinux-common",
        "thin-provisioning-tools",
        "squashfs-tools",
        "xorriso"
    ]

    # Install all packages
    package_list = packages.join(" ")
    run_command("sudo apt-get install -y #{package_list}")

    message_box("Success", "Dependencies installed successfully.")

    optional_dependencies_menu
end

# Function to copy vmlinuz for Oracular 24.10
def copy_vmlinuz_oracular
    progress_dialog("Copying vmlinuz...")
    run_command("sudo cp /boot/vmlinuz-6.11.0-9-generic /opt/claudemods-iso-konsole-script/Supported-Distros/Ubuntu/build-image-oracular/live/")
    message_box("Success", "Vmlinuz copied successfully.")
end

# Function to generate initrd for Oracular 24.10
def generate_initrd_oracular
    progress_dialog("Generating Initramfs...")
    initramfs_command = "sudo mkinitramfs -o \"$(pwd)/Ubuntu/build-image-oracular/live/initrd.img-$(uname -r)\" \"$(uname -r)\""
    run_command(initramfs_command)
    message_box("Success", "Initramfs generated successfully.")
end

# Function to edit grub.cfg for Oracular 24.10
def edit_grub_cfg_oracular
    progress_dialog("Opening grub.cfg for editing...")
    run_command("kate /opt/claudemods-iso-konsole-script/Supported-Distros/Ubuntu/build-image-oracular/boot/grub/grub.cfg")
    message_box("Success", "grub.cfg opened for editing.")
end

# Function to edit isolinux.cfg for Oracular 24.10
def edit_isolinux_cfg_oracular
    progress_dialog("Opening isolinux.cfg for editing...")
    run_command("kate /opt/claudemods-iso-konsole-script/Supported-Distros/Ubuntu/build-image-oracular/isolinux/isolinux.cfg")
    message_box("Success", "isolinux.cfg opened for editing.")
end

# Function to install dependencies for Arch
def install_dependencies_arch
    progress_dialog("Installing dependencies...")

    # Open the file in Kate and wait for it to exit
    system("kate /opt/claudemods-iso-konsole-script/Supported-Distros/information/arch-dependencies.txt")

    # Install the specific package
    system("sudo pacman -U /opt/claudemods-iso-konsole-script/Supported-Distros/Arch/calamares-files/calamares-eggs-3.3.10-1-x86_64.pkg.tar.zst")

    # List of packages to install
    packages = [
        "arch-install-scripts",
        "bash-completion",
        "dosfstools",
        "erofs-utils",
        "findutils",
        "git",
        "grub",
        "jq",
        "libarchive",
        "libisoburn",
        "lsb-release",
        "lvm2",
        "mkinitcpio-archiso",
        "mkinitcpio-nfs-utils",
        "mtools",
        "nbd",
        "pacman-contrib",
        "parted",
        "procps-ng",
        "pv",
        "python",
        "rsync",
        "squashfs-tools",
        "sshfs",
        "syslinux",
        "xdg-utils",
        "bash-completion",
        "zsh-completions",
        "kernel-modules-hook",
        "virt-manager"
    ]

    # Install the list of packages
    system("sudo pacman -S --needed #{packages.join(' ')}")

    puts "All packages installed successfully!"

    optional_dependencies_menu
end

# Function to display optional dependencies menu
def optional_dependencies_menu
    loop do
        print_blue "Choose an optional dependency to install:"
        print_blue "1. virt-manager"
        print_blue "2. gnome-boxes"
        print_blue "3. Return to main menu"

        choice = $stdin.gets.chomp.to_i

        case choice
        when 1
            install_virt_manager
        when 2
            install_gnome_boxes
        when 3
            break
        else
            print_blue "Invalid choice for optional dependency."
        end
    end
end

# Function to install virt-manager
def install_virt_manager
    progress_dialog("Installing virt-manager...")

    case distribution
    when :arch
        run_command("sudo pacman -S virt-manager")
    when :debian, :ubuntu
        run_command("sudo apt-get install -y virt-manager")
    else
        error_box("Error", "Unsupported distribution for virt-manager installation.")
    end

    message_box("Success", "virt-manager installed successfully.")
end

# Function to install gnome-boxes
def install_gnome_boxes
    progress_dialog("Installing gnome-boxes...")

    case distribution
    when :arch
        run_command("sudo pacman -S gnome-boxes")
    when :debian, :ubuntu
        run_command("sudo apt-get install -y gnome-boxes")
    else
        error_box("Error", "Unsupported distribution for gnome-boxes installation.")
    end

    message_box("Success", "gnome-boxes installed successfully.")
end

# Function to determine the distribution
def distribution
    if File.exist?("/etc/arch-release")
        :arch
    elsif File.exist?("/etc/debian_version")
        :debian
    elsif File.exist?("/etc/lsb-release")
        :ubuntu
    else
        :unknown
    end
end

loop do
    main_menu

    print_blue "Choose your distribution:"
    print_blue "1. Arch"
    print_blue "2. Ubuntu"
    print_blue "3. Debian"

    choice = $stdin.gets.chomp.to_i

    if choice == 1
        print_blue "Choose your Arch Linux action:"
        print_blue "1. Install Dependencies"
        print_blue "2. Copy Vmlinuz"
        print_blue "3. Generate Initramfs"
        print_blue "4. Edit grub.cfg"
        print_blue "5. Edit syslinux.cfg"

        arch_choice = $stdin.gets.chomp.to_i

        case arch_choice
        when 1
            install_dependencies_arch
        when 2
            copy_vmlinuz
        when 3
            generate_initramfs
        when 4
            edit_grub_cfg
        when 5
            edit_syslinux_cfg
        else
            print_blue "Invalid choice for Arch Linux action."
        end

    elsif choice == 2
        print_blue "Choose your Ubuntu version:"
        print_blue "1. Oracular 24.10"

        ubuntu_choice = $stdin.gets.chomp.to_i

        case ubuntu_choice
        when 1
            oracular_24_10
            print_blue "Choose your Oracular 24.10 action:"
            print_blue "1. Install Dependencies"
            print_blue "2. Copy vmlinuz"
            print_blue "3. Generate Initrd"
            print_blue "4. Edit grub.cfg"
            print_blue "5. Edit isolinux.cfg"

            oracular_choice = $stdin.gets.chomp.to_i

            case oracular_choice
            when 1
                install_dependencies_oracular
            when 2
                copy_vmlinuz_oracular
            when 3
                generate_initrd_oracular
            when 4
                edit_grub_cfg_oracular
            when 5
                edit_isolinux_cfg_oracular
            else
                print_blue "Invalid choice for Oracular 24.10 action."
            end
        else
            print_blue "Invalid choice for Ubuntu version."
        end

    elsif choice == 3
        print_blue "Choose your Debian action:"
        print_blue "1. Install Dependencies"
        print_blue "2. Copy vmlinuz"
        print_blue "3. Generate Initrd"
        print_blue "4. Edit grub.cfg"
        print_blue "5. Edit isolinux.cfg"

        debian_choice = $stdin.gets.chomp.to_i

        case debian_choice
        when 1
            install_dependencies
        when 2
            copy_vmlinuz_debian
        when 3
            generate_initrd
        when 4
            edit_grub_cfg_debian
        when 5
            edit_isolinux_cfg_debian
        else
            print_blue "Invalid choice for Debian action."
        end
    else
        print_blue "Invalid choice for distribution."
    end

    print_blue "Press Enter to return to the main menu or type 'exit' to quit."
    response = $stdin.gets.chomp
    break if response.downcase == 'exit'
end
