#!/usr/bin/env python3

import os
import sys
import time
import stat
import fcntl
import termios
import subprocess
import shutil
import platform
import getpass
from pathlib import Path
from typing import List, Tuple, Optional

MAX_PATH = 4096
MAX_CMD = 16384

# Terminal colors
BLUE = "\033[34m"
GREEN = "\033[32m"
RED = "\033[31m"
RESET = "\033[0m"
COLOR_CYAN = "\033[38;2;0;255;255m"
COLOR_GOLD = "\033[38;2;36;255;255m"
PASSWORD_MAX = 100

# Terminal control
original_term = None

# Distribution detection
class Distro:
    ARCH = 0
    UBUNTU = 1
    DEBIAN = 2
    CACHYOS = 3
    UNKNOWN = 4

def detect_distro() -> int:
    try:
        with open("/etc/os-release", "r") as os_release:
            for line in os_release:
                if line.startswith("ID="):
                    if "arch" in line:
                        return Distro.ARCH
                    elif "ubuntu" in line:
                        return Distro.UBUNTU
                    elif "debian" in line:
                        return Distro.DEBIAN
                    elif "cachyos" in line:
                        return Distro.CACHYOS
    except FileNotFoundError:
        pass
    return Distro.UNKNOWN

def get_highlight_color(distro: int) -> str:
    if distro == Distro.ARCH:
        return "\033[38;2;36;255;255m"  # 24FFFF (cyan/blue)
    elif distro == Distro.UBUNTU:
        return "\033[38;2;255;165;0m"  # orange
    elif distro == Distro.DEBIAN:
        return "\033[31m"  # red
    elif distro == Distro.CACHYOS:
        return "\033[38;2;36;255;255m"  # same as Arch
    else:
        return "\033[36m"  # cyan as default

def get_distro_name(distro: int) -> str:
    if distro == Distro.ARCH:
        return "Arch"
    elif distro == Distro.UBUNTU:
        return "Ubuntu"
    elif distro == Distro.DEBIAN:
        return "Debian"
    elif distro == Distro.CACHYOS:
        return "CachyOS"
    else:
        return "Unknown"

def read_clone_dir() -> str:
    file_path = os.path.expanduser("~/.config/cmi/clonedir.txt")
    try:
        with open(file_path, "r") as f:
            return f.readline().strip()
    except FileNotFoundError:
        return ""

def save_clone_dir(dir_path: str) -> None:
    config_dir = os.path.expanduser("~/.config/cmi")
    os.makedirs(config_dir, exist_ok=True)
    file_path = os.path.join(config_dir, "clonedir.txt")
    try:
        with open(file_path, "w") as f:
            f.write(dir_path)
        message_box("Success", "Clone directory path saved successfully.")
    except IOError as e:
        print(f"{RED}Error saving clone directory: {e}{RESET}")

def print_banner(distro_name: str = "Arch") -> None:
    print(RED)
    print(
        "░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n"
        "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n"
        "██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n"
        "██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n"
        "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n"
        "░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░"
    )
    print(RESET)
    print(f"{RED}Claudemods Multi Iso Creator Advanced Python Script v2.0 22-06-2025{RESET}")

    now = time.localtime()
    print(f"{GREEN}Current UK Time: {time.strftime('%d/%m/%Y %H:%M:%S', now)}{RESET}")

    print(f"{GREEN}Disk Usage:{RESET}")
    try:
        result = subprocess.run(["df", "-h", "/"], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        print(f"{GREEN}{result.stdout}{RESET}")
    except subprocess.CalledProcessError as e:
        print(f"{RED}Error getting disk usage: {e.stderr}{RESET}")

def enable_raw_mode() -> None:
    global original_term
    original_term = termios.tcgetattr(sys.stdin)
    new_term = termios.tcgetattr(sys.stdin)
    new_term[3] = new_term[3] & ~(termios.ICANON | termios.ECHO)
    termios.tcsetattr(sys.stdin, termios.TCSANOW, new_term)

def disable_raw_mode() -> None:
    if original_term:
        termios.tcsetattr(sys.stdin, termios.TCSANOW, original_term)

def get_key() -> int:
    c = sys.stdin.read(1)
    if c == '\033':  # Escape sequence
        sys.stdin.read(1)  # Skip '['
        return ord(sys.stdin.read(1))  # Return actual key code
    return ord(c)

def print_blue(text: str) -> None:
    print(f"{BLUE}{text}{RESET}")

def message_box(title: str, message: str) -> None:
    print(f"{GREEN}{title}{RESET}")
    print(f"{GREEN}{message}{RESET}")

def error_box(title: str, message: str) -> None:
    print(f"{RED}{title}{RESET}")
    print(f"{RED}{message}{RESET}")

def progress_dialog(message: str) -> None:
    print(f"{BLUE}{message}{RESET}")

def run_command(command: str) -> None:
    print(f"{BLUE}Running command: {command}{RESET}")
    try:
        result = subprocess.run(command, shell=True, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if result.stdout:
            print(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"{RED}Command failed with exit code: {e.returncode}{RESET}")
        if e.stderr:
            print(f"{RED}Error: {e.stderr}{RESET}")

def prompt(prompt_text: str) -> str:
    print(f"{BLUE}{prompt_text}{RESET}", end='')
    return input().strip()

def run_sudo_command(command: str, password: str) -> None:
    print(f"{BLUE}Running command: {command}{RESET}")
    full_command = f"sudo -S {command}"
    try:
        proc = subprocess.Popen(
            full_command,
            shell=True,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True
        )
        stdout, stderr = proc.communicate(input=f"{password}\n")
        if proc.returncode != 0:
            print(f"{RED}Command failed with exit code {proc.returncode}.{RESET}")
            if stderr:
                print(f"{RED}Error: {stderr}{RESET}")
        elif stdout:
            print(stdout)
    except Exception as e:
        print(f"{RED}Error executing command: {e}{RESET}")

def dir_exists(path: str) -> bool:
    return os.path.isdir(path)

def get_kernel_version() -> str:
    try:
        result = subprocess.run(["uname", "-r"], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        return "unknown"

def show_menu(title: str, items: List[str], selected: int, distro: int = Distro.ARCH) -> int:
    os.system("clear")
    print_banner(get_distro_name(distro))

    highlight_color = get_highlight_color(distro)

    print(f"{COLOR_CYAN}  {title}{RESET}")
    print(f"{COLOR_CYAN}  {'-' * len(title)}{RESET}")

    for i, item in enumerate(items):
        if i == selected:
            print(f"{highlight_color}➤ {item}{RESET}")
        else:
            print(f"{COLOR_CYAN}  {item}{RESET}")

    # Set terminal to raw mode for single key input
    oldt = termios.tcgetattr(sys.stdin)
    newt = termios.tcgetattr(sys.stdin)
    newt[3] = newt[3] & ~(termios.ICANON | termios.ECHO)
    termios.tcsetattr(sys.stdin, termios.TCSANOW, newt)

    c = sys.stdin.read(1)
    key = 0

    if c == '\033':  # Escape sequence
        sys.stdin.read(1)  # Skip '['
        key = ord(sys.stdin.read(1))  # Get arrow key code
    elif c == '\n':
        key = ord('\n')
    else:
        key = ord(c)

    # Restore terminal settings
    termios.tcsetattr(sys.stdin, termios.TCSANOW, oldt)

    return key

# ============ ARCH FUNCTIONS ============
def install_dependencies_arch() -> None:
    progress_dialog("Installing dependencies...")
    packages = (
        "cryptsetup dmeventd isolinux libaio-dev libcares2 "
        "libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot "
        "live-boot-doc live-boot-initramfs-tools live-config-systemd "
        "live-tools lvm2 pxelinux syslinux syslinux-common "
        "thin-provisioning-tools squashfs-tools xorriso"
    )
    command = f"sudo pacman -S --needed --noconfirm {packages}"
    run_command(command)
    message_box("Success", "Dependencies installed successfully.")

def copy_vmlinuz_arch() -> None:
    kernel_version = get_kernel_version()
    command = f"sudo cp /boot/vmlinuz-{kernel_version} /home/$USER/.config/cmi/build-image-arch/live/"
    run_command(command)
    message_box("Success", "Vmlinuz copied successfully.")

def generate_initrd_arch() -> None:
    progress_dialog("Generating Initramfs (Arch)...")
    run_command("cd /home/$USER/.config/cmi")
    run_command("cd /home/$USER/.config/cmi/build-image-arch && sudo mkinitcpio -c live.conf -g /home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img")
    message_box("Success", "Initramfs generated successfully.")

def edit_grub_cfg_arch() -> None:
    progress_dialog("Opening grub.cfg (arch)...")
    run_command("nano /home/$USER/.config/cmi/build-image-arch/boot/grub/grub.cfg")
    message_box("Success", "grub.cfg opened for editing.")

def edit_isolinux_cfg_arch() -> None:
    progress_dialog("Opening isolinux.cfg (arch)...")
    run_command("nano /home/$USER/.config/cmi/build-image-arch/isolinux/isolinux.cfg")
    message_box("Success", "isolinux.cfg opened for editing.")

# ============ UBUNTU FUNCTIONS ============
def install_dependencies_ubuntu() -> None:
    progress_dialog("Installing Ubuntu dependencies...")
    packages = (
        "cryptsetup dmeventd isolinux libaio-dev libcares2 "
        "libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot "
        "live-boot-doc live-boot-initramfs-tools live-config-systemd "
        "live-tools lvm2 pxelinux syslinux syslinux-common "
        "thin-provisioning-tools squashfs-tools xorriso "
        "ubuntu-defaults-builder"
    )
    command = f"sudo apt install {packages}"
    run_command(command)
    message_box("Success", "Ubuntu dependencies installed successfully.")

def copy_vmlinuz_ubuntu() -> None:
    kernel_version = get_kernel_version()
    command = f"sudo cp /boot/vmlinuz-{kernel_version} /home/$USER/.config/cmi/build-image-noble/live/"
    run_command(command)
    message_box("Success", "Vmlinuz copied successfully for Ubuntu.")

def generate_initrd_ubuntu() -> None:
    progress_dialog("Generating Initramfs for Ubuntu...")
    run_command("cd /home/$USER/.config/cmi")
    run_command('sudo mkinitramfs -o "/home/$USER/.config/cmi/build-image-noble/live/initrd.img-$(uname -r)" "$(uname -r)"')
    message_box("Success", "Ubuntu initramfs generated successfully.")

def edit_grub_cfg_ubuntu() -> None:
    progress_dialog("Opening Ubuntu grub.cfg...")
    run_command("nano /home/$USER/.config/cmi/build-image-noble/boot/grub/grub.cfg")
    message_box("Success", "Ubuntu grub.cfg opened for editing.")

def edit_isolinux_cfg_ubuntu() -> None:
    progress_dialog("Opening Ubuntu isolinux.cfg...")
    run_command("nano /home/$USER/.config/cmi/build-image-noble/isolinux/isolinux.cfg")
    message_box("Success", "Ubuntu isolinux.cfg opened for editing.")

def create_squashfs_image_ubuntu() -> None:
    clone_dir = read_clone_dir()
    if not clone_dir:
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.")
        return
    command = (
        f"sudo mksquashfs {clone_dir} /home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs "
        "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
        "-always-use-fragments -wildcards -xattrs"
    )
    print(f"Creating Ubuntu SquashFS image from: {clone_dir}")
    run_command(command)

def delete_clone_system_temp_ubuntu() -> None:
    clone_dir = read_clone_dir()
    if not clone_dir:
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.")
        return
    command = f"sudo rm -rf {clone_dir}"
    print(f"Deleting temporary clone directory: {clone_dir}")
    run_command(command)
    if os.path.exists("filesystem.squashfs"):
        command = "sudo rm -f /home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs"
        print("Deleting SquashFS image: filesystem.squashfs")
        run_command(command)
    else:
        print("SquashFS image does not exist: filesystem.squashfs")

# ============ DEBIAN FUNCTIONS ============
def install_dependencies_debian() -> None:
    progress_dialog("Installing Debian dependencies...")
    packages = (
        "cryptsetup dmeventd isolinux libaio-dev libcares2 "
        "libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot "
        "live-boot-doc live-boot-initramfs-tools live-config-systemd "
        "live-tools lvm2 pxelinux syslinux syslinux-common "
        "thin-provisioning-tools squashfs-tools xorriso "
        "debian-goodies"
    )
    command = f"sudo apt install {packages}"
    run_command(command)
    message_box("Success", "Debian dependencies installed successfully.")

def copy_vmlinuz_debian() -> None:
    kernel_version = get_kernel_version()
    command = f"sudo cp /boot/vmlinuz-{kernel_version} /home/$USER/.config/cmi/build-image-debian/live/"
    run_command(command)
    message_box("Success", "Vmlinuz copied successfully for Debian.")

def generate_initrd_debian() -> None:
    progress_dialog("Generating Initramfs for Debian...")
    run_command("cd /home/$USER/.config/cmi")
    run_command('sudo mkinitramfs -o "/home/$USER/.config/cmi/build-image-debian/live/initrd.img-$(uname -r)" "$(uname -r)"')
    message_box("Success", "Debian initramfs generated successfully.")

def edit_grub_cfg_debian() -> None:
    progress_dialog("Opening Debian grub.cfg...")
    run_command("nano /home/$USER/.config/cmi/build-image-debian/boot/grub/grub.cfg")
    message_box("Success", "Debian grub.cfg opened for editing.")

def edit_isolinux_cfg_debian() -> None:
    progress_dialog("Opening Debian isolinux.cfg...")
    run_command("nano /home/$USER/.config/cmi/build-image-debian/isolinux/isolinux.cfg")
    message_box("Success", "Debian isolinux.cfg opened for editing.")

def create_squashfs_image_debian() -> None:
    clone_dir = read_clone_dir()
    if not clone_dir:
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.")
        return
    command = (
        f"sudo mksquashfs {clone_dir} /home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs "
        "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
        "-always-use-fragments -wildcards -xattrs"
    )
    print(f"Creating Debian SquashFS image from: {clone_dir}")
    run_command(command)

def delete_clone_system_temp_debian() -> None:
    clone_dir = read_clone_dir()
    if not clone_dir:
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.")
        return
    command = f"sudo rm -rf {clone_dir}"
    print(f"Deleting temporary clone directory: {clone_dir}")
    run_command(command)
    if os.path.exists("filesystem.squashfs"):
        command = "sudo rm -f /home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs"
        print("Deleting SquashFS image: filesystem.squashfs")
        run_command(command)
    else:
        print("SquashFS image does not exist: filesystem.squashfs")

# ============ COMMON FUNCTIONS ============
def clone_system(clone_dir: str) -> None:
    if dir_exists(clone_dir):
        print(f"Skipping removal of existing clone directory: {clone_dir}")
    else:
        os.makedirs(clone_dir, mode=0o755, exist_ok=True)

    command = (
        f"sudo rsync -aHAxSr --numeric-ids --info=progress2 "
        "--include=dev --include=usr --include=proc --include=tmp --include=sys "
        "--include=run --include=media "
        "--exclude=dev/* --exclude=proc/* --exclude=tmp/* --exclude=sys/* "
        f"--exclude=run/* --exclude=media/* --exclude={clone_dir} "
        f"/ {clone_dir}"
    )
    print(f"Cloning system directory to: {clone_dir}")
    run_command(command)

def create_squashfs_image() -> None:
    clone_dir = read_clone_dir()
    if not clone_dir:
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.")
        return
    command = (
        f"sudo mksquashfs {clone_dir} /home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs "
        "-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery "
        "-always-use-fragments -wildcards -xattrs"
    )
    print(f"Creating SquashFS image from: {clone_dir}")
    run_command(command)

def delete_clone_system_temp() -> None:
    clone_dir = read_clone_dir()
    if not clone_dir:
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.")
        return
    command = f"sudo rm -rf {clone_dir}"
    print(f"Deleting temporary clone directory: {clone_dir}")
    run_command(command)
    if os.path.exists("filesystem.squashfs"):
        command = "sudo rm -f /home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs"
        print("Deleting SquashFS image: filesystem.squashfs")
        run_command(command)
    else:
        print("SquashFS image does not exist: filesystem.squashfs")

def set_clone_directory() -> None:
    dir_path = prompt("Enter full path for clone_system_temp directory: ")
    if not dir_path:
        error_box("Error", "Directory path cannot be empty")
        return
    save_clone_dir(dir_path)

def install_one_time_updater() -> None:
    progress_dialog("Installing one-time updater...")
    commands = [
        "cd /home/$USER",
        "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git  >/dev/null 2>&1",
        "mkdir -p /home/$USER/.config/cmi >/dev/null 2>&1",
        "cp /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/updatermain/advancedcscriptupdater /home/$USER/.config/cmi >/dev/null 2>&1",
        "cp /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/installer/patch.sh /home/$USER/.config/cmi >/dev/null 2>&1",
        "chmod +x /home/$USER/.config/cmi/patch.sh >/dev/null 2>&1",
        "chmod +x /home/$USER/.config/cmi/advancedcscriptupdater >/dev/null 2>&1",
        "rm -rf /home/$USER/claudemods-multi-iso-konsole-script >/dev/null 2>&1"
    ]
    for cmd in commands:
        run_command(cmd)
    message_box("Success", "One-time updater installed successfully in /home/$USER/.config/cmi")

def squashfs_menu(distro: int) -> None:
    items = [
        "Max compression (xz)",
        "Create SquashFS from clone directory",
        "Delete clone directory and SquashFS image",
        "Back to Main Menu"
    ]
    selected = 0
    while True:
        key = show_menu("SquashFS Creator", items, selected, distro)
        if key == 65:  # Up arrow
            if selected > 0:
                selected -= 1
        elif key == 66:  # Down arrow
            if selected < len(items) - 1:
                selected += 1
        elif key == 10:  # Enter key
            if selected == 0:
                clone_dir = read_clone_dir()
                if not clone_dir:
                    error_box("Error", "No clone directory specified. Please set it in Setup Script menu.")
                else:
                    if not dir_exists(clone_dir):
                        clone_system(clone_dir)
                    if distro == Distro.UBUNTU:
                        create_squashfs_image_ubuntu()
                    elif distro == Distro.DEBIAN:
                        create_squashfs_image_debian()
                    else:
                        create_squashfs_image()
            elif selected == 1:
                if distro == Distro.UBUNTU:
                    create_squashfs_image_ubuntu()
                elif distro == Distro.DEBIAN:
                    create_squashfs_image_debian()
                else:
                    create_squashfs_image()
            elif selected == 2:
                if distro == Distro.UBUNTU:
                    delete_clone_system_temp_ubuntu()
                elif distro == Distro.DEBIAN:
                    delete_clone_system_temp_debian()
                else:
                    delete_clone_system_temp()
            elif selected == 3:
                return
            print("\nPress Enter to continue...")
            sys.stdin.read(1)  # Wait for Enter

def create_iso(distro: int) -> None:
    iso_name = prompt("What do you want to name your .iso? ")
    if not iso_name:
        error_box("Input Error", "ISO name cannot be empty.")
        return

    output_dir = prompt("Enter the output directory path (or press Enter for current directory): ")
    if not output_dir:
        output_dir = "."

    application_dir_path = os.getcwd()

    if distro == Distro.UBUNTU:
        build_image_dir = os.path.join(application_dir_path, "build-image-noble")
    elif distro == Distro.DEBIAN:
        build_image_dir = os.path.join(application_dir_path, "build-image-debian")
    else:
        build_image_dir = os.path.join(application_dir_path, "build-image-arch")

    if len(build_image_dir) >= MAX_PATH:
        error_box("Error", "Path too long for build directory")
        return

    if not os.path.exists(output_dir):
        try:
            os.makedirs(output_dir, mode=0o755)
        except OSError as e:
            print(f"{RED}Error creating directory: {e}{RESET}")
            return

    progress_dialog("Creating ISO...")
    now = time.localtime()
    timestamp = time.strftime("%Y-%m-%d_%H%M", now)
    iso_file_name = os.path.join(output_dir, f"{iso_name}_amd64_{timestamp}.iso")

    if len(iso_file_name) >= MAX_PATH:
        error_box("Error", "Path too long for ISO filename")
        return

    if distro == Distro.UBUNTU or distro == Distro.DEBIAN:
        xorriso_command = (
            f'sudo xorriso -as mkisofs -o "{iso_file_name}" -V 2025 -iso-level 3 '
            '-isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin '
            '-c isolinux/boot.cat -b isolinux/isolinux.bin '
            '-no-emul-boot -boot-load-size 4 -boot-info-table '
            '-eltorito-alt-boot -e boot/grub/efi.img '
            f'-no-emul-boot -isohybrid-gpt-basdat "{build_image_dir}"'
        )
    else:
        xorriso_command = (
            f'sudo xorriso -as mkisofs -o "{iso_file_name}" -V 2025 -iso-level 3 '
            '-isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin -c isolinux/boot.cat '
            '-b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table '
            '-eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat '
            f'"{build_image_dir}"'
        )

    if len(xorriso_command) >= MAX_CMD:
        error_box("Error", "Command too long for buffer")
        return

    sudo_password = prompt("Enter your sudo password: ")
    if not sudo_password:
        error_box("Input Error", "Sudo password cannot be empty.")
        return

    run_sudo_command(xorriso_command, sudo_password)
    message_box("Success", "ISO creation completed.")

    choice = prompt("Press 'm' to go back to main menu or Enter to exit: ")
    if choice.lower() == 'm':
        run_command("ruby /opt/claudemods-iso-konsole-script/demo.rb")

def run_iso_in_qemu() -> None:
    qemu_script = "/opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb"
    command = f"ruby {qemu_script}"
    run_command(command)

def iso_creator_menu(distro: int) -> None:
    items = [
        "Create ISO",
        "Run ISO in QEMU",
        "Back to Main Menu"
    ]
    selected = 0
    while True:
        key = show_menu("ISO Creator Menu", items, selected, distro)
        if key == 65:  # Up arrow
            if selected > 0:
                selected -= 1
        elif key == 66:  # Down arrow
            if selected < len(items) - 1:
                selected += 1
        elif key == 10:  # Enter key
            if selected == 0:
                create_iso(distro)
            elif selected == 1:
                run_iso_in_qemu()
            elif selected == 2:
                return

def create_command_files() -> None:
    try:
        exe_path = os.path.realpath(sys.argv[0])
    except:
        exe_path = sys.argv[0]

    sudo_password = prompt("Enter sudo password to create command files: ")
    if not sudo_password:
        error_box("Error", "Sudo password cannot be empty")
        return

    # gen-init
    gen_init_content = f"""#!/bin/sh
if [ "$1" = "--help" ]; then
  echo "Usage: gen-init"
  echo "Generate initcpio configuration"
  exit 0
fi
exec {exe_path} 5
"""
    run_sudo_command(f'echo \'{gen_init_content}\' | sudo tee /usr/bin/gen-init > /dev/null && sudo chmod 755 /usr/bin/gen-init', sudo_password)

    # edit-isocfg
    edit_isocfg_content = f"""#!/bin/sh
if [ "$1" = "--help" ]; then
  echo "Usage: edit-isocfg"
  echo "Edit isolinux.cfg file"
  exit 0
fi
exec {exe_path} 6
"""
    run_sudo_command(f'echo \'{edit_isocfg_content}\' | sudo tee /usr/bin/edit-isocfg > /dev/null && sudo chmod 755 /usr/bin/edit-isocfg', sudo_password)

    # edit-grubcfg
    edit_grubcfg_content = f"""#!/bin/sh
if [ "$1" = "--help" ]; then
  echo "Usage: edit-grubcfg"
  echo "Edit grub.cfg file"
  exit 0
fi
exec {exe_path} 7
"""
    run_sudo_command(f'echo \'{edit_grubcfg_content}\' | sudo tee /usr/bin/edit-grubcfg > /dev/null && sudo chmod 755 /usr/bin/edit-grubcfg', sudo_password)

    # setup-script
    setup_script_content = f"""#!/bin/sh
if [ "$1" = "--help" ]; then
  echo "Usage: setup-script"
  echo "Open setup script menu"
  exit 0
fi
exec {exe_path} 8
"""
    run_sudo_command(f'echo \'{setup_script_content}\' | sudo tee /usr/bin/setup-script > /dev/null && sudo chmod 755 /usr/bin/setup-script', sudo_password)

    # make-iso
    make_iso_content = f"""#!/bin/sh
if [ "$1" = "--help" ]; then
  echo "Usage: make-iso"
  echo "Launches the ISO creation menu"
  exit 0
fi
exec {exe_path} 3
"""
    run_sudo_command(f'echo \'{make_iso_content}\' | sudo tee /usr/bin/make-iso > /dev/null && sudo chmod 755 /usr/bin/make-iso', sudo_password)

    # make-squashfs
    make_squashfs_content = f"""#!/bin/sh
if [ "$1" = "--help" ]; then
  echo "Usage: make-squashfs"
  echo "Launches the SquashFS creation menu"
  exit 0
fi
exec {exe_path} 4
"""
    run_sudo_command(f'echo \'{make_squashfs_content}\' | sudo tee /usr/bin/make-squashfs > /dev/null && sudo chmod 755 /usr/bin/make-squashfs', sudo_password)

    print(f"{GREEN}Activated! You can now use all commands in your terminal.{RESET}")

def remove_command_files() -> None:
    commands = [
        "sudo rm -f /usr/bin/gen-init",
        "sudo rm -f /usr/bin/edit-isocfg",
        "sudo rm -f /usr/bin/edit-grubcfg",
        "sudo rm -f /usr/bin/setup-script",
        "sudo rm -f /usr/bin/make-iso",
        "sudo rm -f /usr/bin/make-squashfs"
    ]
    for cmd in commands:
        run_command(cmd)
    print(f"{GREEN}Commands deactivated and removed from system.{RESET}")

def command_installer_menu(distro: int) -> None:
    items = [
        "Activate terminal commands",
        "Deactivate terminal commands",
        "Back to Main Menu"
    ]
    selected = 0
    while True:
        key = show_menu("Command Installer Menu", items, selected, distro)
        if key == 65:  # Up arrow
            if selected > 0:
                selected -= 1
        elif key == 66:  # Down arrow
            if selected < len(items) - 1:
                selected += 1
        elif key == 10:  # Enter key
            if selected == 0:
                create_command_files()
            elif selected == 1:
                remove_command_files()
            elif selected == 2:
                return
            print("\nPress Enter to continue...")
            sys.stdin.read(1)  # Wait for Enter

def setup_script_menu(distro: int) -> None:
    distro_name = get_distro_name(distro)
    items = []

    if distro == Distro.ARCH:
        items = [
            "Generate initcpio configuration (arch)",
            "Edit isolinux.cfg (arch)",
            "Edit grub.cfg (arch)",
            "Set clone directory path",
            "Install One Time Updater",
            "Back to Main Menu"
        ]
    elif distro == Distro.CACHYOS:
        items = [
            "Generate initcpio configuration (cachyos)",
            "Edit isolinux.cfg (cachyos)",
            "Edit grub.cfg (cachyos)",
            "Set clone directory path",
            "Install One Time Updater",
            "Back to Main Menu"
        ]
    elif distro == Distro.UBUNTU:
        items = [
            "Generate initramfs (ubuntu)",
            "Edit isolinux.cfg (ubuntu)",
            "Edit grub.cfg (ubuntu)",
            "Set clone directory path",
            "Install One Time Updater",
            "Back to Main Menu"
        ]
    elif distro == Distro.DEBIAN:
        items = [
            "Generate initramfs (debian)",
            "Edit isolinux.cfg (debian)",
            "Edit grub.cfg (debian)",
            "Set clone directory path",
            "Install One Time Updater",
            "Back to Main Menu"
        ]
    else:
        error_box("Error", "Unsupported distribution")
        return

    selected = 0
    oldt = termios.tcgetattr(sys.stdin)
    newt = termios.tcgetattr(sys.stdin)
    newt[3] = newt[3] & ~(termios.ICANON | termios.ECHO)
    termios.tcsetattr(sys.stdin, termios.TCSANOW, newt)

    while True:
        os.system("clear")
        print_banner(distro_name)

        # Display menu
        print(f"{COLOR_CYAN}  Setup Script Menu{RESET}")
        print(f"{COLOR_CYAN}  -----------------{RESET}")

        for i, item in enumerate(items):
            if i == selected:
                print(f"{get_highlight_color(distro)}➤ {item}{RESET}")
            else:
                print(f"  {item}")

        # Handle input
        c = sys.stdin.read(1)
        if c == '\033':  # Escape sequence
            sys.stdin.read(1)  # Skip '['
            c = sys.stdin.read(1)

            if c == 'A' and selected > 0:  # Up arrow
                selected -= 1
            elif c == 'B' and selected < len(items) - 1:  # Down arrow
                selected += 1
        elif c == '\n':  # Enter key
            if selected == 0:
                if distro in (Distro.ARCH, Distro.CACHYOS):
                    generate_initrd_arch()
                elif distro == Distro.UBUNTU:
                    generate_initrd_ubuntu()
                elif distro == Distro.DEBIAN:
                    generate_initrd_debian()
            elif selected == 1:
                if distro in (Distro.ARCH, Distro.CACHYOS):
                    edit_isolinux_cfg_arch()
                elif distro == Distro.UBUNTU:
                    edit_isolinux_cfg_ubuntu()
                elif distro == Distro.DEBIAN:
                    edit_isolinux_cfg_debian()
            elif selected == 2:
                if distro in (Distro.ARCH, Distro.CACHYOS):
                    edit_grub_cfg_arch()
                elif distro == Distro.UBUNTU:
                    edit_grub_cfg_ubuntu()
                elif distro == Distro.DEBIAN:
                    edit_grub_cfg_debian()
            elif selected == 3:
                set_clone_directory()
            elif selected == 4:
                install_one_time_updater()
            elif selected == 5:
                termios.tcsetattr(sys.stdin, termios.TCSANOW, oldt)
                return

            # Pause after action
            print("\nPress Enter to continue...")
            while sys.stdin.read(1) != '\n':
                pass

    termios.tcsetattr(sys.stdin, termios.TCSANOW, oldt)

def main() -> None:
    # Save original terminal settings
    global original_term
    original_term = termios.tcgetattr(sys.stdin)

    distro = detect_distro()
    distro_name = get_distro_name(distro)

    items = [
        "SquashFS Creator",
        "ISO Creator",
        "Setup Script",
        "Command Installer",
        "Exit"
    ]
    selected = 0

    while True:
        key = show_menu("Main Menu", items, selected, distro)
        if key == 65:  # Up arrow
            if selected > 0:
                selected -= 1
        elif key == 66:  # Down arrow
            if selected < len(items) - 1:
                selected += 1
        elif key == 10:  # Enter key
            if selected == 0:
                squashfs_menu(distro)
            elif selected == 1:
                iso_creator_menu(distro)
            elif selected == 2:
                setup_script_menu(distro)
            elif selected == 3:
                command_installer_menu(distro)
            elif selected == 4:
                disable_raw_mode()
                sys.exit(0)

if __name__ == "__main__":
    main()
