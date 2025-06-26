#!/usr/bin/env ruby

require 'open3'

# Function to run a command and check for errors
def run_command(command)
    puts "Running command: #{command}"
    stdout, stderr, status = Open3.capture3(command)
    if status.success?
        puts stdout
    else
        puts "Error running command: #{command}"
        puts stderr
        exit 1
    end
end

# Install the specific package
run_command("sudo pacman -U /opt/claudemods-iso-konsole-script/calamares-files/arch/calamares-eggs-3.3.9-1-x86_64.pkg.tar.zst")

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
    "pnpm",
    "bash-completion",
    "zsh-completions",
    "virt-manager"
]

# Install the list of packages
run_command("sudo pacman -S --needed #{packages.join(' ')}")

puts "All packages installed successfully!"
