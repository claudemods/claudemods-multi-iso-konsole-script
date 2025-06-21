use std::{
    fs::{self, File},
    io::{self, BufRead, BufReader, Read, Write},
    path::Path,
    process::Command,
    env,
    os::unix::process::CommandExt
};
use nix::{
    unistd::{fork, ForkResult, pipe, dup2, close},
    sys::wait::waitpid,
};
use termios::{Termios, tcsetattr, ICANON, ECHO, TCSANOW};
use std::os::unix::io::FromRawFd;
use chrono::Local;

const BLUE: &str = "\x1b[34m";
const GREEN: &str = "\x1b[32m";
const RED: &str = "\x1b[31m";
const RESET: &str = "\x1b[0m";
const COLOR_CYAN: &str = "\x1b[36m";
const COLOR_GOLD: &str = "\x1b[38;2;36;255;255m";

#[derive(Debug, PartialEq)]
enum Distro {
    Arch,
    Ubuntu,
    Debian,
    Unknown,
}

fn detect_distro() -> Distro {
    if let Ok(file) = File::open("/etc/os-release") {
        let reader = BufReader::new(file);
        for line in reader.lines().filter_map(Result::ok) {
            if line.starts_with("ID=") {
                if line.contains("arch") {
                    return Distro::Arch;
                } else if line.contains("ubuntu") {
                    return Distro::Ubuntu;
                } else if line.contains("debian") {
                    return Distro::Debian;
                }
            }
        }
    }
    Distro::Unknown
}

fn read_clone_dir() -> Option<String> {
    let home = env::var("HOME").unwrap_or_else(|_| "".to_string());
    let file_path = format!("{}/.config/cmi/clonedir.txt", home);
    fs::read_to_string(file_path).ok()
}

fn save_clone_dir(dir_path: &str) -> io::Result<()> {
    let home = env::var("HOME").unwrap_or_else(|_| "".to_string());
    let config_dir = format!("{}/.config/cmi", home);
    fs::create_dir_all(&config_dir)?;
    let file_path = format!("{}/clonedir.txt", config_dir);
    fs::write(file_path, dir_path)
}

fn print_banner(distro_name: &str) {
    println!("{}", RED);
    println!(
        "░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n\
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n\
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n\
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n\
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n\
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░"
    );
    println!("{}", RESET);
    println!(
        "{}Claudemods {} ISO Creator Advanced Rust Script v1.01 21-06-2025{}",
        RED, distro_name, RESET
    );

    let now = Local::now();
    println!(
        "{}Current UK Time: {}{}",
        GREEN,
        now.format("%d/%m/%Y %H:%M:%S"),
             RESET
    );

    println!("{}Disk Usage:{}", GREEN, RESET);
    let output = Command::new("df")
    .arg("-h")
    .arg("/")
    .output()
    .expect("Failed to execute df command");
    println!("{}", String::from_utf8_lossy(&output.stdout));
}

fn enable_raw_mode(original_term: &mut Termios) -> io::Result<()> {
    let mut term = original_term.clone();
    term.c_lflag &= !(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &term)?;
    Ok(())
}

fn disable_raw_mode(original_term: &Termios) -> io::Result<()> {
    tcsetattr(0, TCSANOW, original_term)?;
    Ok(())
}

fn get_key() -> io::Result<char> {
    let mut buf = [0; 1];
    io::stdin().read_exact(&mut buf)?;
    Ok(buf[0] as char)
}

fn message_box(title: &str, message: &str) {
    println!("{}{}{}", GREEN, title, RESET);
    println!("{}{}{}", GREEN, message, RESET);
}

fn error_box(title: &str, message: &str) {
    println!("{}{}{}", RED, title, RESET);
    println!("{}{}{}", RED, message, RESET);
}

fn progress_dialog(message: &str) {
    println!("{}{}{}", BLUE, message, RESET);
}

fn run_command(command: &str) -> io::Result<()> {
    println!("{}Running command: {}{}", BLUE, command, RESET);
    let status = Command::new("sh")
    .arg("-c")
    .arg(command)
    .status()?;

    if !status.success() {
        println!(
            "{}Command failed with exit code: {}{}",
            RED,
            status.code().unwrap_or(-1),
                 RESET
        );
    }
    Ok(())
}

fn prompt(prompt_text: &str) -> String {
    print!("{}{}{}", BLUE, prompt_text, RESET);
    io::stdout().flush().unwrap();
    let mut input = String::new();
    io::stdin().read_line(&mut input).unwrap();
    input.trim().to_string()
}

fn run_sudo_command(command: &str, password: &str) -> io::Result<()> {
    println!("{}Running command: {}{}", BLUE, command, RESET);

    let (pipe_read, pipe_write) = pipe()?;

    match unsafe { fork() }? {
        ForkResult::Parent { child } => {
            close(pipe_read)?;
            let mut pipe = unsafe { File::from_raw_fd(pipe_write) };
            write!(pipe, "{}\n", password)?;
            close(pipe_write)?;

            waitpid(child, None)?;
        }
        ForkResult::Child => {
            close(pipe_write)?;
            dup2(pipe_read, 0)?;
            close(pipe_read)?;

            let full_command = format!("sudo -S {}", command);
            let err = Command::new("sh")
            .arg("-c")
            .arg(full_command)
            .exec();

            eprintln!("Failed to execute command: {}", err);
            std::process::exit(1);
        }
    }
    Ok(())
}

fn dir_exists(path: &str) -> bool {
    Path::new(path).is_dir()
}

fn show_menu(title: &str, items: &[String], selected: usize, distro_name: &str) -> io::Result<char> {
    Command::new("clear").status()?;
    print_banner(distro_name);
    println!("{}  {}{}", COLOR_CYAN, title, RESET);
    println!("{}  {}{}", COLOR_CYAN, "-".repeat(title.len()), RESET);

    for (i, item) in items.iter().enumerate() {
        if i == selected {
            println!("{}➤ {}{}", COLOR_GOLD, item, RESET);
        } else {
            println!("{}  {}{}", COLOR_CYAN, item, RESET);
        }
    }

    get_key()
}

fn generate_initrd_arch() -> io::Result<()> {
    progress_dialog("Generating Initramfs (Arch)...");
    run_command("cd /home/$USER/.config/cmi")?;
    run_command("sudo mkinitcpio -c live.conf -g /home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img")?;
    message_box("Success", "Initramfs generated successfully.");
    Ok(())
}

fn edit_grub_cfg_arch() -> io::Result<()> {
    progress_dialog("Opening grub.cfg (arch)...");
    run_command("nano /home/$USER/.config/cmi/build-image-arch/boot/grub/grub.cfg")?;
    message_box("Success", "grub.cfg opened for editing.");
    Ok(())
}

fn edit_isolinux_cfg_arch() -> io::Result<()> {
    progress_dialog("Opening isolinux.cfg (arch)...");
    run_command("nano /home/$USER/.config/cmi/build-image-arch/isolinux/isolinux.cfg")?;
    message_box("Success", "isolinux.cfg opened for editing.");
    Ok(())
}

fn generate_initrd_ubuntu() -> io::Result<()> {
    progress_dialog("Generating Initramfs for Ubuntu...");
    run_command("cd /home/$USER/.config/cmi")?;
    run_command("sudo mkinitramfs -o \"/home/$USER/.config/cmi/build-image-noble/live/initrd.img-$(uname -r)\" \"$(uname -r)\"")?;
    message_box("Success", "Ubuntu initramfs generated successfully.");
    Ok(())
}

fn edit_grub_cfg_ubuntu() -> io::Result<()> {
    progress_dialog("Opening Ubuntu grub.cfg...");
    run_command("nano /home/$USER/.config/cmi/build-image-noble/boot/grub/grub.cfg")?;
    message_box("Success", "Ubuntu grub.cfg opened for editing.");
    Ok(())
}

fn edit_isolinux_cfg_ubuntu() -> io::Result<()> {
    progress_dialog("Opening Ubuntu isolinux.cfg...");
    run_command("nano /home/$USER/.config/cmi/build-image-noble/isolinux/isolinux.cfg")?;
    message_box("Success", "Ubuntu isolinux.cfg opened for editing.");
    Ok(())
}

fn create_squashfs_image_ubuntu() -> io::Result<()> {
    if let Some(clone_dir) = read_clone_dir() {
        let command = format!(
            "sudo mksquashfs {} /home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs \
-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery \
-always-use-fragments -wildcards -xattrs",
clone_dir
        );
        println!("Creating Ubuntu SquashFS image from: {}", clone_dir);
        run_command(&command)
    } else {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        Ok(())
    }
}

fn delete_clone_system_temp_ubuntu() -> io::Result<()> {
    if let Some(clone_dir) = read_clone_dir() {
        let command = format!("sudo rm -rf {}", clone_dir);
        println!("Deleting temporary clone directory: {}", clone_dir);
        run_command(&command)?;

        if Path::new("filesystem.squashfs").exists() {
            let command = "sudo rm -f /home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs";
            println!("Deleting SquashFS image: filesystem.squashfs");
            run_command(command)
        } else {
            println!("SquashFS image does not exist: filesystem.squashfs");
            Ok(())
        }
    } else {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        Ok(())
    }
}

fn generate_initrd_debian() -> io::Result<()> {
    progress_dialog("Generating Initramfs for Debian...");
    run_command("cd /home/$USER/.config/cmi")?;
    run_command("sudo mkinitramfs -o \"/home/$USER/.config/cmi/build-image-debian/live/initrd.img-$(uname -r)\" \"$(uname -r)\"")?;
    message_box("Success", "Debian initramfs generated successfully.");
    Ok(())
}

fn edit_grub_cfg_debian() -> io::Result<()> {
    progress_dialog("Opening Debian grub.cfg...");
    run_command("nano /home/$USER/.config/cmi/build-image-debian/boot/grub/grub.cfg")?;
    message_box("Success", "Debian grub.cfg opened for editing.");
    Ok(())
}

fn edit_isolinux_cfg_debian() -> io::Result<()> {
    progress_dialog("Opening Debian isolinux.cfg...");
    run_command("nano /home/$USER/.config/cmi/build-image-debian/isolinux/isolinux.cfg")?;
    message_box("Success", "Debian isolinux.cfg opened for editing.");
    Ok(())
}

fn create_squashfs_image_debian() -> io::Result<()> {
    if let Some(clone_dir) = read_clone_dir() {
        let command = format!(
            "sudo mksquashfs {} /home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs \
-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery \
-always-use-fragments -wildcards -xattrs",
clone_dir
        );
        println!("Creating Debian SquashFS image from: {}", clone_dir);
        run_command(&command)
    } else {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        Ok(())
    }
}

fn delete_clone_system_temp_debian() -> io::Result<()> {
    if let Some(clone_dir) = read_clone_dir() {
        let command = format!("sudo rm -rf {}", clone_dir);
        println!("Deleting temporary clone directory: {}", clone_dir);
        run_command(&command)?;

        if Path::new("filesystem.squashfs").exists() {
            let command = "sudo rm -f /home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs";
            println!("Deleting SquashFS image: filesystem.squashfs");
            run_command(command)
        } else {
            println!("SquashFS image does not exist: filesystem.squashfs");
            Ok(())
        }
    } else {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        Ok(())
    }
}

fn clone_system(clone_dir: &str) -> io::Result<()> {
    if !dir_exists(clone_dir) {
        fs::create_dir_all(clone_dir)?;
    }

    let command = format!(
        "sudo rsync -aHAxSr --numeric-ids --info=progress2 \
--include=dev --include=usr --include=proc --include=tmp --include=sys \
--include=run --include=media \
--exclude=dev/* --exclude=proc/* --exclude=tmp/* --exclude=sys/* \
--exclude=run/* --exclude=media/* --exclude={} \
/ {}",
clone_dir, clone_dir
    );
    println!("Cloning system directory to: {}", clone_dir);
    run_command(&command)
}

fn create_squashfs_image() -> io::Result<()> {
    if let Some(clone_dir) = read_clone_dir() {
        let command = format!(
            "sudo mksquashfs {} /home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs \
-comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery \
-always-use-fragments -wildcards -xattrs",
clone_dir
        );
        println!("Creating SquashFS image from: {}", clone_dir);
        run_command(&command)
    } else {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        Ok(())
    }
}

fn delete_clone_system_temp() -> io::Result<()> {
    if let Some(clone_dir) = read_clone_dir() {
        let command = format!("sudo rm -rf {}", clone_dir);
        println!("Deleting temporary clone directory: {}", clone_dir);
        run_command(&command)?;

        if Path::new("filesystem.squashfs").exists() {
            let command = "sudo rm -f /home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs";
            println!("Deleting SquashFS image: filesystem.squashfs");
            run_command(command)
        } else {
            println!("SquashFS image does not exist: filesystem.squashfs");
            Ok(())
        }
    } else {
        error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
        Ok(())
    }
}

fn set_clone_directory() -> io::Result<()> {
    let dir_path = prompt("Enter full path for clone_system_temp directory: ");
    if dir_path.is_empty() {
        error_box("Error", "Directory path cannot be empty");
        return Ok(());
    }
    save_clone_dir(&dir_path)
}

fn install_one_time_updater() -> io::Result<()> {
    progress_dialog("Installing one-time updater...");
    let commands = [
        "cd /home/$USER",
        "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git >/dev/null 2>&1",
        "mkdir -p /home/$USER/.config/cmi >/dev/null 2>&1",
        "cp /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/updatermain/advancedcscriptupdater /home/$USER/.config/cmi >/dev/null 2>&1",
        "cp /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/installer/patch.sh /home/$USER/.config/cmi >/dev/null 2>&1",
        "chmod +x /home/$USER/.config/cmi/patch.sh >/dev/null 2>&1",
        "chmod +x /home/$USER/.config/cmi/advancedcscriptupdater >/dev/null 2>&1",
        "rm -rf /home/$USER/claudemods-multi-iso-konsole-script >/dev/null 2>&1",
    ];

    for cmd in &commands {
        run_command(cmd)?;
    }

    message_box("Success", "One-time updater installed successfully in /home/$USER/.config/cmi");
    Ok(())
}

fn squashfs_menu(distro_name: &str) -> io::Result<()> {
    let items = vec![
        "Max compression (xz)".to_string(),
        "Create SquashFS from clone directory".to_string(),
        "Delete clone directory and SquashFS image".to_string(),
        "Back to Main Menu".to_string(),
    ];

    let mut selected = 0;

    loop {
        let key = show_menu("SquashFS Creator", &items, selected, distro_name)?;

        match key {
            'A' => if selected > 0 { selected -= 1; },
            'B' => if selected < items.len() - 1 { selected += 1; },
            '\n' => {
                match selected {
                    0 => {
                        if let Some(clone_dir) = read_clone_dir() {
                            if !dir_exists(&clone_dir) {
                                clone_system(&clone_dir)?;
                            }
                            match distro_name {
                                "Ubuntu" => create_squashfs_image_ubuntu()?,
                                "Debian" => create_squashfs_image_debian()?,
                                _ => create_squashfs_image()?,
                            }
                        } else {
                            error_box("Error", "No clone directory specified. Please set it in Setup Script menu.");
                        }
                    }
                    1 => {
                        match distro_name {
                            "Ubuntu" => create_squashfs_image_ubuntu()?,
                            "Debian" => create_squashfs_image_debian()?,
                            _ => create_squashfs_image()?,
                        }
                    }
                    2 => {
                        match distro_name {
                            "Ubuntu" => delete_clone_system_temp_ubuntu()?,
                            "Debian" => delete_clone_system_temp_debian()?,
                            _ => delete_clone_system_temp()?,
                        }
                    }
                    3 => return Ok(()),
                    _ => {}
                }
                println!("\nPress Enter to continue...");
                io::stdin().read_line(&mut String::new())?;
            }
            _ => {}
        }
    }
}

fn create_iso(distro_name: &str) -> io::Result<()> {
    let iso_name = prompt("What do you want to name your .iso? ");
    if iso_name.is_empty() {
        error_box("Input Error", "ISO name cannot be empty.");
        return Ok(());
    }

    let mut output_dir = prompt("Enter the output directory path (or press Enter for current directory): ");
    if output_dir.is_empty() {
        output_dir = ".".to_string();
    }

    let application_dir_path = env::current_dir()?;

    let build_image_dir = match distro_name {
        "Ubuntu" => application_dir_path.join("build-image-noble"),
        "Debian" => application_dir_path.join("build-image-debian"),
        _ => application_dir_path.join("build-image-arch"),
    };

    if !build_image_dir.exists() {
        fs::create_dir_all(&build_image_dir)?;
    }

    let now = Local::now();
    let timestamp = now.format("%Y-%m-%d_%H%M").to_string();
    let iso_file_name = format!("{}/{}_amd64_{}.iso", output_dir, iso_name, timestamp);

    let xorriso_command = if distro_name == "Ubuntu" || distro_name == "Debian" {
        format!(
            "sudo xorriso -as mkisofs -o \"{}\" -V 2025 -iso-level 3 \
-isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin \
-c isolinux/boot.cat -b isolinux/isolinux.bin \
-no-emul-boot -boot-load-size 4 -boot-info-table \
-eltorito-alt-boot -e boot/grub/efi.img \
-no-emul-boot -isohybrid-gpt-basdat \"{}\"",
iso_file_name,
build_image_dir.display()
        )
    } else {
        format!(
            "sudo xorriso -as mkisofs -o \"{}\" -V 2025 -iso-level 3 \
-isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin -c isolinux/boot.cat \
-b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
-eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat \"{}\"",
iso_file_name,
build_image_dir.display()
        )
    };

    let sudo_password = prompt("Enter your sudo password: ");
    if sudo_password.is_empty() {
        error_box("Input Error", "Sudo password cannot be empty.");
        return Ok(());
    }

    run_sudo_command(&xorriso_command, &sudo_password)?;
    message_box("Success", "ISO creation completed.");

    let choice = prompt("Press 'm' to go back to main menu or Enter to exit: ");
    if !choice.is_empty() && (choice.to_lowercase().starts_with('m')) {
        run_command("ruby /opt/claudemods-iso-konsole-script/demo.rb")?;
    }

    Ok(())
}

fn run_iso_in_qemu() -> io::Result<()> {
    let qemu_script = "/opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb";
    let command = format!("ruby {}", qemu_script);
    run_command(&command)
}

fn iso_creator_menu(distro_name: &str) -> io::Result<()> {
    let items = vec![
        "Create ISO".to_string(),
        "Run ISO in QEMU".to_string(),
        "Back to Main Menu".to_string(),
    ];

    let mut selected = 0;

    loop {
        let key = show_menu("ISO Creator Menu", &items, selected, distro_name)?;

        match key {
            'A' => if selected > 0 { selected -= 1; },
            'B' => if selected < items.len() - 1 { selected += 1; },
            '\n' => {
                match selected {
                    0 => create_iso(distro_name)?,
                    1 => run_iso_in_qemu()?,
                    2 => return Ok(()),
                    _ => {}
                }
            }
            _ => {}
        }
    }
}

fn create_command_files() -> io::Result<()> {
    let exe_path = env::current_exe()?;
    let exe_path_str = exe_path.to_str().unwrap_or("");

    let sudo_password = prompt("Enter sudo password to create command files: ");
    if sudo_password.is_empty() {
        error_box("Error", "Sudo password cannot be empty");
        return Ok(());
    }

    // gen-init
    let gen_init = format!(
        "#!/bin/sh\n\
if [ \"$1\" = \"--help\" ]; then\n\
echo \"Usage: gen-init\"\n\
echo \"Generate initcpio configuration\"\n\
exit 0\n\
fi\n\
exec {} 5\n",
exe_path_str
    );

    run_sudo_command(
        &format!("bash -c 'echo -e \"{}\" > /usr/bin/gen-init && chmod 755 /usr/bin/gen-init'", gen_init),
                     &sudo_password,
    )?;

    // edit-isocfg
    let edit_isocfg = format!(
        "#!/bin/sh\n\
if [ \"$1\" = \"--help\" ]; then\n\
echo \"Usage: edit-isocfg\"\n\
echo \"Edit isolinux.cfg file\"\n\
exit 0\n\
fi\n\
exec {} 6\n",
exe_path_str
    );

    run_sudo_command(
        &format!("bash -c 'echo -e \"{}\" > /usr/bin/edit-isocfg && chmod 755 /usr/bin/edit-isocfg'", edit_isocfg),
                     &sudo_password,
    )?;

    // edit-grubcfg
    let edit_grubcfg = format!(
        "#!/bin/sh\n\
if [ \"$1\" = \"--help\" ]; then\n\
echo \"Usage: edit-grubcfg\"\n\
echo \"Edit grub.cfg file\"\n\
exit 0\n\
fi\n\
exec {} 7\n",
exe_path_str
    );

    run_sudo_command(
        &format!("bash -c 'echo -e \"{}\" > /usr/bin/edit-grubcfg && chmod 755 /usr/bin/edit-grubcfg'", edit_grubcfg),
                     &sudo_password,
    )?;

    // setup-script
    let setup_script = format!(
        "#!/bin/sh\n\
if [ \"$1\" = \"--help\" ]; then\n\
echo \"Usage: setup-script\"\n\
echo \"Open setup script menu\"\n\
exit 0\n\
fi\n\
exec {} 8\n",
exe_path_str
    );

    run_sudo_command(
        &format!("bash -c 'echo -e \"{}\" > /usr/bin/setup-script && chmod 755 /usr/bin/setup-script'", setup_script),
                     &sudo_password,
    )?;

    // make-iso
    let make_iso = format!(
        "#!/bin/sh\n\
if [ \"$1\" = \"--help\" ]; then\n\
echo \"Usage: make-iso\"\n\
echo \"Launches the ISO creation menu\"\n\
exit 0\n\
fi\n\
exec {} 3\n",
exe_path_str
    );

    run_sudo_command(
        &format!("bash -c 'echo -e \"{}\" > /usr/bin/make-iso && chmod 755 /usr/bin/make-iso'", make_iso),
                     &sudo_password,
    )?;

    // make-squashfs
    let make_squashfs = format!(
        "#!/bin/sh\n\
if [ \"$1\" = \"--help\" ]; then\n\
echo \"Usage: make-squashfs\"\n\
echo \"Launches the SquashFS creation menu\"\n\
exit 0\n\
fi\n\
exec {} 4\n",
exe_path_str
    );

    run_sudo_command(
        &format!("bash -c 'echo -e \"{}\" > /usr/bin/make-squashfs && chmod 755 /usr/bin/make-squashfs'", make_squashfs),
                     &sudo_password,
    )?;

    println!("{}Activated! You can now use all commands in your terminal.{}", GREEN, RESET);
    Ok(())
}

fn remove_command_files() -> io::Result<()> {
    let commands = [
        "/usr/bin/gen-init",
        "/usr/bin/edit-isocfg",
        "/usr/bin/edit-grubcfg",
        "/usr/bin/setup-script",
        "/usr/bin/make-iso",
        "/usr/bin/make-squashfs",
    ];

    for cmd in &commands {
        run_command(&format!("sudo rm -f {}", cmd))?;
    }

    println!("{}Commands deactivated and removed from system.{}", GREEN, RESET);
    Ok(())
}

fn command_installer_menu(distro_name: &str) -> io::Result<()> {
    let items = vec![
        "Activate terminal commands".to_string(),
        "Deactivate terminal commands".to_string(),
        "Back to Main Menu".to_string(),
    ];

    let mut selected = 0;

    loop {
        let key = show_menu("Command Installer Menu", &items, selected, distro_name)?;

        match key {
            'A' => if selected > 0 { selected -= 1; },
            'B' => if selected < items.len() - 1 { selected += 1; },
            '\n' => {
                match selected {
                    0 => create_command_files()?,
                    1 => remove_command_files()?,
                    2 => return Ok(()),
                    _ => {}
                }
                println!("\nPress Enter to continue...");
                io::stdin().read_line(&mut String::new())?;
            }
            _ => {}
        }
    }
}

fn setup_script_menu(distro_name: &str) -> io::Result<()> {
    let items = match distro_name {
        "Arch" => vec![
            "Generate initcpio configuration (arch)".to_string(),
            "Edit isolinux.cfg (arch)".to_string(),
            "Edit grub.cfg (arch)".to_string(),
            "Set clone directory path".to_string(),
            "Install One Time Updater".to_string(),
            "Back to Main Menu".to_string(),
        ],
        "Ubuntu" => vec![
            "Generate initcpio configuration (ubuntu)".to_string(),
            "Edit isolinux.cfg (ubuntu)".to_string(),
            "Edit grub.cfg (ubuntu)".to_string(),
            "Set clone directory path".to_string(),
            "Install One Time Updater".to_string(),
            "Back to Main Menu".to_string(),
        ],
        "Debian" => vec![
            "Generate initcpio configuration (debian)".to_string(),
            "Edit isolinux.cfg (debian)".to_string(),
            "Edit grub.cfg (debian)".to_string(),
            "Set clone directory path".to_string(),
            "Install One Time Updater".to_string(),
            "Back to Main Menu".to_string(),
        ],
        _ => {
            error_box("Error", "Unsupported Linux distribution");
            return Ok(());
        }
    };

    let mut selected = 0;

    loop {
        let key = show_menu("Setup Script Menu", &items, selected, distro_name)?;

        match key {
            'A' => if selected > 0 { selected -= 1; },
            'B' => if selected < items.len() - 1 { selected += 1; },
            '\n' => {
                match selected {
                    0 => match distro_name {
                        "Arch" => generate_initrd_arch()?,
                        "Ubuntu" => generate_initrd_ubuntu()?,
                        "Debian" => generate_initrd_debian()?,
                        _ => {}
                    },
                    1 => match distro_name {
                        "Arch" => edit_isolinux_cfg_arch()?,
                        "Ubuntu" => edit_isolinux_cfg_ubuntu()?,
                        "Debian" => edit_isolinux_cfg_debian()?,
                        _ => {}
                    },
                    2 => match distro_name {
                        "Arch" => edit_grub_cfg_arch()?,
                        "Ubuntu" => edit_grub_cfg_ubuntu()?,
                        "Debian" => edit_grub_cfg_debian()?,
                        _ => {}
                    },
                    3 => set_clone_directory()?,
                    4 => install_one_time_updater()?,
                    5 => return Ok(()),
                    _ => {}
                }
                println!("\nPress Enter to continue...");
                io::stdin().read_line(&mut String::new())?;
            }
            _ => {}
        }
    }
}

fn main() -> io::Result<()> {
    let mut original_term = Termios::from_fd(0)?;
    enable_raw_mode(&mut original_term)?;

    let distro = detect_distro();
    let distro_name = match distro {
        Distro::Arch => "Arch",
        Distro::Ubuntu => "Ubuntu",
        Distro::Debian => "Debian",
        Distro::Unknown => "Linux",
    };

    let args: Vec<String> = env::args().collect();
    if args.len() > 1 {
        match args[1].as_str() {
            "3" => {
                iso_creator_menu(distro_name)?;
                disable_raw_mode(&original_term)?;
                return Ok(());
            }
            "4" => {
                squashfs_menu(distro_name)?;
                disable_raw_mode(&original_term)?;
                return Ok(());
            }
            "5" => {
                match distro {
                    Distro::Arch => generate_initrd_arch()?,
                    Distro::Ubuntu => generate_initrd_ubuntu()?,
                    Distro::Debian => generate_initrd_debian()?,
                    _ => {}
                }
                disable_raw_mode(&original_term)?;
                return Ok(());
            }
            "6" => {
                match distro {
                    Distro::Arch => edit_isolinux_cfg_arch()?,
                    Distro::Ubuntu => edit_isolinux_cfg_ubuntu()?,
                    Distro::Debian => edit_isolinux_cfg_debian()?,
                    _ => {}
                }
                disable_raw_mode(&original_term)?;
                return Ok(());
            }
            "7" => {
                match distro {
                    Distro::Arch => edit_grub_cfg_arch()?,
                    Distro::Ubuntu => edit_grub_cfg_ubuntu()?,
                    Distro::Debian => edit_grub_cfg_debian()?,
                    _ => {}
                }
                disable_raw_mode(&original_term)?;
                return Ok(());
            }
            "8" => {
                setup_script_menu(distro_name)?;
                disable_raw_mode(&original_term)?;
                return Ok(());
            }
            _ => {}
        }
    }

    let items = vec![
        "SquashFS Creator".to_string(),
        "ISO Creator".to_string(),
        "Setup Script".to_string(),
        "Command Installer".to_string(),
        "Exit".to_string(),
    ];

    let mut selected = 0;

    loop {
        let key = show_menu("Main Menu", &items, selected, distro_name)?;

        match key {
            'A' => if selected > 0 { selected -= 1; },
            'B' => if selected < items.len() - 1 { selected += 1; },
            '\n' => {
                match selected {
                    0 => squashfs_menu(distro_name)?,
                    1 => iso_creator_menu(distro_name)?,
                    2 => setup_script_menu(distro_name)?,
                    3 => command_installer_menu(distro_name)?,
                    4 => {
                        disable_raw_mode(&original_term)?;
                        return Ok(());
                    }
                    _ => {}
                }
            }
            _ => {}
        }
    }
}
