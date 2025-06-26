#!/usr/bin/perl
use strict;
use warnings;
use File::Copy;
use File::Find;
use File::Spec;
use Term::ANSIColor qw(:constants colored);
use Term::ReadLine;
use POSIX qw(strftime);
use Cwd 'abs_path';
use Time::HiRes qw(sleep);  # For more precise sleep

# Function to run a command in the current terminal and wait for it to finish
sub run_command {
    my ($command) = @_;
    print "Running command: $command\n";  # Debugging output
    my $exit_code = system($command);
    if ($exit_code != 0) {
        print "Command Execution Error: Command '$command' failed with exit code $exit_code.\n";
    }
}

# Function to prompt for user input
sub prompt {
    my ($prompt_text) = @_;
    my $term = Term::ReadLine->new('Input');
    return $term->readline($prompt_text);
}

# Function to display a progress dialog (simulated)
sub progress_dialog {
    my ($message) = @_;
    print "$message\n";
}

# Function to display a message box (simulated)
sub message_box {
    my ($title, $message) = @_;
    print "$title\n";
    print "$message\n";
}

# Function to display an error message box (simulated)
sub error_box {
    my ($title, $message) = @_;
    print "$title\n";
    print "$message\n";
}

# Function to install dependencies
sub install_dependencies {
    progress_dialog("Installing dependencies...");

    # List of packages to install
    my @packages = (
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
    );

    # Install all packages
    my $package_list = join(" ", @packages);
    run_command("sudo pacman -S --needed $package_list");

    message_box("Success", "Dependencies installed successfully.");
}

# Function to run the SquashFS command
sub run_squashfs_command {
    progress_dialog("Running SquashFS Command...");
    run_command("./KDENeonSquashFS-Creator.bin");
    message_box("Success", "SquashFS Command completed.");
}

# Function to convert to Dwarfs
sub convert_to_dwarfs {
    progress_dialog("Converting to Dwarfs...");
    run_command("./KDENeonDwarfs.bin");
    message_box("Success", "Conversion to Dwarfs completed.");
}

# Function to create an ISO
sub create_iso {
    my $iso_name = prompt("What do you want to name your .iso?");
    if (!$iso_name) {
        error_box("Input Error", "ISO name cannot be empty.");
        return;
    }

    my $application_dir_path = abs_path();
    my $build_image_dir = File::Spec->catfile($application_dir_path, "build-files/build-image", "kdeneon", "x86_64");
    my $airootfs_path = File::Spec->catfile($build_image_dir, "airootfs.sfs");

    if (!-e $airootfs_path) {
        my $file_name = prompt("Select SquashFS (full path):");
        if (!$file_name) {
            error_box("Input Error", "No SquashFS selected.");
            return;
        }
        # Copy SquashFS with progress (simulated)
        progress_dialog("Copying SquashFS...");
        copy($file_name, $airootfs_path) or error_box("Error", "Failed to copy SquashFS: $!");
    } else {
        message_box("File Detected", "airootfs.sfs detected. Proceeding with ISO creation.");
    }

    progress_dialog("Creating ISO...");

    my $iso_file_name = "$iso_name\_amd64_" . strftime("%Y-%m-%d_%H%M", localtime) . ".iso";
    my $xorriso_command = "xorriso -as mkisofs -o $iso_file_name -V 2024 -iso-level 3 -isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin -c isolinux/boot.cat -b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat build-files/build-image-kdeneon";

    my $sudo_password = prompt("Enter your sudo password:");
    if (!$sudo_password) {
        error_box("Input Error", "Sudo password cannot be empty.");
        return;
    }

    my $full_command = "echo '$sudo_password' | sudo -S $xorriso_command; echo \"ISO creation completed.\"";
    run_command($full_command);

    message_box("Success", "ISO creation completed.");
}

# Function to run the ISO in QEMU
sub run_iso_in_qemu {
    my $iso_file_path = prompt("Select ISO File (full path):");
    if (!$iso_file_path) {
        error_box("Input Error", "ISO file cannot be empty.");
        return;
    }

    my $qemu_command = "qemu-system-x86_64 -m 4000 -smp 2 -accel kvm -cdrom $iso_file_path";
    run_command($qemu_command);
}

# Function to show information from a text file
sub show_information {
    my $application_dir_path = abs_path();
    my $information_file_path = File::Spec->catfile($application_dir_path, "information.txt");

    if (!-e $information_file_path) {
        error_box("File Not Found", "The information.txt file does not exist in the application directory.");
        return;
    }

    run_command("kate $information_file_path");
}

# Function to change boot artwork
sub change_boot_artwork {
    my $image_path = prompt("Select Boot Artwork (full path):");
    if (!$image_path) {
        error_box("Input Error", "No image selected.");
        return;
    }

    my $application_dir_path = abs_path();
    my $destination_dir = File::Spec->catfile($application_dir_path, "build-files/build-image", "isolinux");
    my $destination_path = File::Spec->catfile($destination_dir, "splash.png");

    progress_dialog("Copying Boot Artwork...");
    copy($image_path, $destination_path) or error_box("Error", "Failed to update boot artwork: $!");
    message_box("Success", "Boot artwork updated successfully.");
}

# Function to generate initramfs
sub generate_initramfs {
    progress_dialog("Generating Initramfs...");
    my $kernel_version = `uname -r`;
    chomp($kernel_version);

    my $initramfs_command = "sudo mkinitramfs -o build-files/build-image-kdeneon/live/initrd.img-$kernel_version $kernel_version";
    my $copy_kernel_command = "sudo cp /boot/vmlinuz build-files/build-image-kdeneon/live/vmlinuz-$kernel_version";

    run_command($initramfs_command);
    run_command($copy_kernel_command);
    message_box("Success", "Initramfs and kernel copied successfully.");
}

# Function to copy vmlinuz
sub copy_vmlinuz {
    progress_dialog("Copying Vmlinuz...");
    my $kernel_version = `uname -r`;
    chomp($kernel_version);

    my $vmlinuz_path = "/boot/vmlinuz";
    if (!-e $vmlinuz_path) {
        error_box("Error", "Failed to find vmlinuz.");
        return;
    }

    my $application_dir_path = abs_path();
    my $destination_dir = File::Spec->catfile($application_dir_path, "build-files/build-image-kdeneon", "live");
    my $destination_path = File::Spec->catfile($destination_dir, "vmlinuz-$kernel_version");

    copy($vmlinuz_path, $destination_path) or error_box("Error", "Failed to copy vmlinuz: $!");

    message_box("Success", "Vmlinuz copied successfully.");
}

# Function to find vmlinuz
sub find_vmlinuz {
    my $vmlinuz_path = "/boot/vmlinuz";
    return $vmlinuz_path if -e $vmlinuz_path;
    return "";
}

# Function to install the system
sub install_your_system {
    run_command("perl installsystemkdeneon.pl");
}

# Tools And Extras Menu
sub tools_and_extras_menu {
    while (1) {
        print "Tools And Extras Menu:\n";
        print colored("1. Create SquashFS", 'blue') . "\n";
        print colored("2. Convert to Dwarfs", 'blue') . "\n";
        print colored("3. Create ISO", 'blue') . "\n";
        print colored("4. Run ISO in QEMU", 'blue') . "\n";
        print colored("5. Install Your System", 'blue') . "\n";
        print colored("0. Back to Main Menu", 'blue') . "\n";
        my $choice = prompt("Choose an option:");

        if ($choice eq '1') {
            run_squashfs_command();
        } elsif ($choice eq '2') {
            convert_to_dwarfs();
        } elsif ($choice eq '3') {
            create_iso();
        } elsif ($choice eq '4') {
            run_iso_in_qemu();
        } elsif ($choice eq '5') {
            install_your_system();
        } elsif ($choice eq '0') {
            last;
        } else {
            error_box("Input Error", "Invalid option.");
        }
    }
}

# Configuration Options Menu
sub configuration_options_menu {
    while (1) {
        print "Configuration Options Menu:\n";
        print colored("1. Edit Grub CFG", 'blue') . "\n";
        print colored("2. Edit ISOLINUX", 'blue') . "\n";
        print colored("3. Change Boot Artwork", 'blue') . "\n";
        print colored("4. Generate Initramfs", 'blue') . "\n";
        print colored("5. Copy Vmlinuz", 'blue') . "\n";
        print colored("0. Back to Main Menu", 'blue') . "\n";
        my $choice = prompt("Choose an option:");

        if ($choice eq '1') {
            run_command("kate /build-files/build-image-kdeneon/boot/grub/grub.cfg");
        } elsif ($choice eq '2') {
            run_command("kate /build-files/build-image-kdeneon/isolinux/isolinux.cfg");
        } elsif ($choice eq '3') {
            change_boot_artwork();
        } elsif ($choice eq '4') {
            generate_initramfs();
        } elsif ($choice eq '5') {
            copy_vmlinuz();
        } elsif ($choice eq '0') {
            last;
        } else {
            error_box("Input Error", "Invalid option.");
        }
    }
}

# Function to get the used and available space on the drive
sub get_used_space {
    my $df_output = `df -h /`;
    my @lines = split /\n/, $df_output;
    my @fields = split /\s+/, $lines[1];
    return ($fields[2], $fields[3]);  # Used space, Available space
}

# Function to display news from a website
sub display_news {
    my $url = "https://www.claudemods.co.uk/iso-creator-news";
    run_command("xdg-open $url");
}

# Function to display help
sub display_help {
    print "Help:\n";
    print "Main Menu Options:\n";
    print colored("1. News - Display news from a website.", 'blue') . "\n";
    print colored("2. ClaudeMods Arch ISO Creator Script - Enter Arch commands.", 'blue') . "\n";
    print colored("0. Exit - Exit the program.", 'blue') . "\n";
    print "Commands:\n";
    print colored("command: news - Display news from a website.", 'blue') . "\n";
    print colored("command: help - Display this help information.", 'blue') . "\n";
    print colored("command: install dependencies - Install dependencies.", 'blue') . "\n";
    print colored("command: guided setup - Run guided setup.", 'blue') . "\n";
    print colored("command: make squashfs - Create SquashFS.", 'blue') . "\n";
    print colored("command: make iso - Create ISO.", 'blue') . "\n";
    print colored("command: run in qemu - Run ISO in QEMU.", 'blue') . "\n";

    print "Press Enter to exit...\n";
    <STDIN>;  # Wait for user to press Enter
}

# Function to run the guided setup
sub guided_setup {
    configuration_options_menu();
    run_command("kate /build-files/build-image-kdeneon/isolinux/isolinux.cfg");
    change_boot_artwork();
}

# Main menu
sub main_menu {
    my $ascii_art = colored(
        "░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n" .
        "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n" .
        "██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n" .
        "██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n" .
        "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n" .
        "░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░\n" .
        "\n" .
        "ClaudeMods ISO Creator Script v1.0 Written in cpp and perl!", 'red');

    while (1) {
        my ($used_space, $available_space) = get_used_space();
        my $current_date_time = strftime("%Y-%m-%d %H:%M:%S", localtime);

        print "\033[H\033[J";  # Clear the screen
        print "$ascii_art\n";
        print colored("Used Space: $used_space", 'dark green') . "\n";
        print colored("Available Space: $available_space", 'dark green') . "\n";
        print colored("Date and Time: $current_date_time", 'dark green') . "\n";
        print "Main Menu:\n";
        print colored("1. News", 'blue') . "\n";
        print colored("2. ClaudeMods Arch ISO Creator Script", 'blue') . "\n";
        print colored("0. Exit", 'blue') . "\n";
        print colored("command: news", 'blue') . "\n";
        print colored("command: help", 'blue') . "\n";
        print colored("command: install dependencies", 'blue') . "\n";
        print colored("command: guided setup", 'blue') . "\n";
        print colored("command: make squashfs", 'blue') . "\n";
        print colored("command: make iso", 'blue') . "\n";
        print colored("command: run in qemu", 'blue') . "\n";
        my $choice = prompt(colored("Choose an option:", 'bold'));

        if ($choice eq '1') {
            display_news();
        } elsif ($choice eq '2') {
            arch_iso_creator_menu();
        } elsif ($choice eq '0') {
            last;
        } elsif ($choice =~ /^\s*news\s*$/i) {
            display_news();
        } elsif ($choice =~ /^\s*arch\s*$/i) {
            handle_arch_commands();
        } elsif ($choice =~ /^\s*help\s*$/i) {
            display_help();
        } elsif ($choice =~ /^\s*install\s*dependencies\s*$/i) {
            install_dependencies();
        } elsif ($choice =~ /^\s*guided\s*setup\s*$/i) {
            guided_setup();
        } elsif ($choice =~ /^\s*make\s*squashfs\s*$/i) {
            run_squashfs_command();
        } elsif ($choice =~ /^\s*make\s*iso\s*$/i) {
            create_iso();
        } elsif ($choice =~ /^\s*run\s*in\s*qemu\s*$/i) {
            run_iso_in_qemu();
        } else {
            error_box("Input Error", "Invalid option.");
        }

        sleep(1);  # Update the time every second
    }
}

# Arch ISO Creator Menu
sub arch_iso_creator_menu {
    while (1) {
        print "ClaudeMods Arch ISO Creator Menu:\n";
        print colored("1. Show System Information", 'blue') . "\n";
        print colored("2. Show Information", 'blue') . "\n";
        print colored("3. Tools And Extras", 'blue') . "\n";
        print colored("4. Configuration Options", 'blue') . "\n";
        print colored("0. Back to Main Menu", 'blue') . "\n";
        print colored("command 1. install dependencies", 'blue') . "\n";  # New command
        print colored("command 2. guided setup", 'blue') . "\n";  # Added message
        print colored("command 3. make squashfs", 'blue') . "\n";  # Added command
        print colored("command 4. make iso", 'blue') . "\n";  # Added command
        print colored("command 5. run in qemu", 'blue') . "\n";  # New command
        my $choice = prompt(colored("Choose an option:", 'bold'));

        if ($choice =~ /^\s*1\s*$/) {
            run_command("kate /build-files/build-image-kdeneon/system_info.txt");
        } elsif ($choice =~ /^\s*2\s*$/) {
            show_information();
        } elsif ($choice =~ /^\s*3\s*$/) {
            tools_and_extras_menu();
        } elsif ($choice =~ /^\s*4\s*$/) {
            configuration_options_menu();
        } elsif ($choice =~ /^\s*0\s*$/) {
            last;
        } elsif ($choice =~ /^\s*install\s*dependencies\s*$/i) {
            install_dependencies();  # New command
        } elsif ($choice =~ /^\s*guided\s*setup\s*$/i) {
            guided_setup();
        } elsif ($choice =~ /^\s*make\s*squashfs\s*$/i) {
            run_squashfs_command();
        } elsif ($choice =~ /^\s*make\s*iso\s*$/i) {
            create_iso();
        } elsif ($choice =~ /^\s*run\s*in\s*qemu\s*$/i) {
            run_iso_in_qemu();  # New command
        } else {
            error_box("Input Error", "Invalid option.");
        }
    }
}

# Function to run the setup
sub run_setup {
    # Run syslinux setup
    progress_dialog("Setting up syslinux...");
    run_command("kate /build-files/build-image-kdeneon/isolinux/isolinux.cfg");

    # Wait for the user to close the editor
    print "Press Enter to continue after closing the editor...\n";
    <STDIN>;

    # Change boot artwork
    change_boot_artwork();
}

# Function to execute the sequence of options for "setup"
sub execute_setup_sequence {
    configuration_options_menu();
    run_command("kate /build-files/build-image-kdeneon/isolinux/isolinux.cfg");
    change_boot_artwork();
}

# Function to handle Arch commands
sub handle_arch_commands {
    while (1) {
        print "Available Arch Commands:\n";
        print colored("1. install dependencies", 'blue') . "\n";
        print colored("2. guided setup", 'blue') . "\n";
        print colored("3. make squashfs", 'blue') . "\n";
        print colored("4. make iso", 'blue') . "\n";
        print colored("5. run in qemu", 'blue') . "\n";
        print colored("6. exit", 'blue') . "\n";
        my $command = prompt("Enter Arch command:");
        if ($command =~ /^\s*exit\s*$/i) {
            last;
        } elsif ($command =~ /^\s*install\s*dependencies\s*$/i) {
            install_dependencies();
        } elsif ($command =~ /^\s*guided\s*setup\s*$/i) {
            guided_setup();
        } elsif ($command =~ /^\s*make\s*squashfs\s*$/i) {
            run_squashfs_command();
        } elsif ($command =~ /^\s*make\s*iso\s*$/i) {
            create_iso();
        } elsif ($command =~ /^\s*run\s*in\s*qemu\s*$/i) {
            run_iso_in_qemu();
        } else {
            error_box("Input Error", "Invalid command.");
        }
    }
}

# Function to handle all commands directly from the command line
sub handle_command_line_arguments {
    my ($command) = @_;
    if ($command =~ /^\s*news\s*$/i) {
        display_news();
    } elsif ($command =~ /^\s*help\s*$/i) {
        display_help();
    } elsif ($command =~ /^\s*install\s*dependencies\s*$/i) {
        install_dependencies();
    } elsif ($command =~ /^\s*guided\s*setup\s*$/i) {
        guided_setup();
    } elsif ($command =~ /^\s*make\s*squashfs\s*$/i) {
        run_squashfs_command();
    } elsif ($command =~ /^\s*make\s*iso\s*$/i) {
        create_iso();
    } elsif ($command =~ /^\s*run\s*in\s*qemu\s*$/i) {
        run_iso_in_qemu();
    } elsif ($command =~ /^\s*\.\/\S+\s*$/i) {
        run_command($command);
    } else {
        error_box("Input Error", "Invalid command.");
    }
}

# Check if the script is executed with any command
if (@ARGV) {
    my $command = $ARGV[0];
    handle_command_line_arguments($command);
} else {
    main_menu();
}
