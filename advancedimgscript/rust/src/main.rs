use std::{
    fs,
    fs::File,
    io::{self, BufRead, BufReader, Read, Write},
    path::Path,
    process::Command,
    sync::{Arc, Mutex},
    thread,
};
use libc::{getuid, getpwuid, passwd};
use termios::{Termios, tcsetattr, TCSANOW, ICANON, ECHO};
use std::os::unix::io::AsRawFd;
use std::sync::atomic::{AtomicBool, Ordering};
use chrono::Local;

// Constants
const ORIG_IMG_NAME: &str = "rootfs.img";
const FINAL_IMG_NAME: &str = "rootfs.img";
const MOUNT_POINT: &str = "/mnt/btrfs_temp";
const SOURCE_DIR: &str = "/";
const COMPRESSION_LEVEL: &str = "22";
const SQUASHFS_COMPRESSION: &str = "zstd";
const BTRFS_LABEL: &str = "LIVE_SYSTEM";
const BTRFS_COMPRESSION: &str = "zstd";

// ANSI color codes
const COLOR_RED: &str = "\x1b[31m";
const COLOR_GREEN: &str = "\x1b[32m";
const COLOR_BLUE: &str = "\x1b[34m";
const COLOR_CYAN: &str = "\x1b[38;2;0;255;255m";
const COLOR_YELLOW: &str = "\x1b[33m";
const COLOR_RESET: &str = "\x1b[0m";
const COLOR_HIGHLIGHT: &str = "\x1b[38;2;0;255;255m";
const COLOR_NORMAL: &str = "\x1b[34m";

// Dependencies list
const DEPENDENCIES: [&str; 34] = [
    "rsync", "squashfs-tools", "xorriso", "grub", "dosfstools", "unzip", "nano",
"arch-install-scripts", "bash-completion", "erofs-utils", "findutils", "unzip",
"jq", "libarchive", "libisoburn", "lsb-release", "lvm2", "mkinitcpio-archiso",
"mkinitcpio-nfs-utils", "mtools", "nbd", "pacman-contrib", "parted", "procps-ng",
"pv", "python", "sshfs", "syslinux", "xdg-utils", "zsh-completions",
"kernel-modules-hook", "virt-manager", "qt6-tools", "qt5-tools"
];

// Configuration state
#[derive(Default, Clone)]
struct ConfigState {
    iso_tag: String,
    iso_name: String,
    output_dir: String,
    vmlinuz_path: String,
    mkinitcpio_generated: bool,
    grub_edited: bool,
    dependencies_installed: bool,
}

impl ConfigState {
    fn is_ready_for_iso(&self) -> bool {
        !self.iso_tag.is_empty() &&
        !self.iso_name.is_empty() &&
        !self.output_dir.is_empty() &&
        !self.vmlinuz_path.is_empty() &&
        self.mkinitcpio_generated &&
        self.grub_edited &&
        self.dependencies_installed
    }
}

struct AppState {
    config: ConfigState,
    time_thread_running: Arc<AtomicBool>,
    current_time_str: Arc<Mutex<String>>,
    username: String,
    build_dir: String,
}

impl AppState {
    fn new() -> Self {
        let username = get_username().unwrap_or_else(|| {
            eprintln!("{}Failed to get username!{}", COLOR_RED, COLOR_RESET);
            std::process::exit(1);
        });

        let build_dir = format!("/home/{}/.config/cmi/build-image-arch-img", username);

        // Create config directory if it doesn't exist
        let config_dir = format!("/home/{}/.config/cmi", username);
        fs::create_dir_all(&config_dir).unwrap_or_else(|_| {
            eprintln!("{}Failed to create config directory{}", COLOR_RED, COLOR_RESET);
        });

        AppState {
            config: load_config(&username),
            time_thread_running: Arc::new(AtomicBool::new(true)),
            current_time_str: Arc::new(Mutex::new(String::new())),
            username,
            build_dir,
        }
    }
}

fn get_username() -> Option<String> {
    unsafe {
        let uid = getuid();
        let pw = getpwuid(uid);
        if pw.is_null() {
            None
        } else {
            let passwd: &passwd = &*pw;
            let username = std::ffi::CStr::from_ptr(passwd.pw_name)
            .to_string_lossy()
            .into_owned();
            Some(username)
        }
    }
}

fn save_config(config: &ConfigState, username: &str) {
    let config_path = format!("/home/{}/.config/cmi/configuration.txt", username);
    let mut config_file = File::create(config_path).unwrap_or_else(|_| {
        eprintln!("{}Failed to save configuration{}", COLOR_RED, COLOR_RESET);
        std::process::exit(1);
    });

    writeln!(config_file, "isoTag={}", config.iso_tag).unwrap();
    writeln!(config_file, "isoName={}", config.iso_name).unwrap();
    writeln!(config_file, "outputDir={}", config.output_dir).unwrap();
    writeln!(config_file, "vmlinuzPath={}", config.vmlinuz_path).unwrap();
    writeln!(config_file, "mkinitcpioGenerated={}", if config.mkinitcpio_generated { "1" } else { "0" }).unwrap();
    writeln!(config_file, "grubEdited={}", if config.grub_edited { "1" } else { "0" }).unwrap();
    writeln!(config_file, "dependenciesInstalled={}", if config.dependencies_installed { "1" } else { "0" }).unwrap();
}

fn load_config(username: &str) -> ConfigState {
    let config_path = format!("/home/{}/.config/cmi/configuration.txt", username);
    let file = match File::open(&config_path) {
        Ok(f) => f,
        Err(_) => return ConfigState::default(),
    };

    let mut config = ConfigState::default();
    let reader = BufReader::new(file);

    for line in reader.lines() {
        if let Ok(line) = line {
            if let Some(equal_pos) = line.find('=') {
                let key = &line[..equal_pos];
                let value = &line[equal_pos + 1..];

                match key {
                    "isoTag" => config.iso_tag = value.to_string(),
                    "isoName" => config.iso_name = value.to_string(),
                    "outputDir" => config.output_dir = value.to_string(),
                    "vmlinuzPath" => config.vmlinuz_path = value.to_string(),
                    "mkinitcpioGenerated" => config.mkinitcpio_generated = value == "1",
                    "grubEdited" => config.grub_edited = value == "1",
                    "dependenciesInstalled" => config.dependencies_installed = value == "1",
                    _ => {}
                }
            }
        }
    }

    config
}

fn update_time_thread(running: Arc<AtomicBool>, time_str: Arc<Mutex<String>>) {
    while running.load(Ordering::Relaxed) {
        let now = Local::now();
        let datetime = now.format("%d/%m/%Y %H:%M:%S").to_string();

        {
            let mut time = time_str.lock().unwrap();
            *time = datetime;
        }

        std::thread::sleep(std::time::Duration::from_secs(1));
    }
}

fn clear_screen() {
    print!("\x1b[2J\x1b[1;1H");
}

fn getch() -> char {
    let stdin = io::stdin();
    let fd = stdin.as_raw_fd();
    let mut termios = Termios::from_fd(fd).unwrap();

    let original = termios.clone();
    termios.c_lflag &= !(ICANON | ECHO);
    tcsetattr(fd, TCSANOW, &termios).unwrap();

    let mut buf = [0u8; 1];
    io::stdin().read_exact(&mut buf).unwrap();

    tcsetattr(fd, TCSANOW, &original).unwrap();
    buf[0] as char
}

fn execute_command(cmd: &str, continue_on_error: bool) -> bool {
    println!("{}", COLOR_CYAN);
    io::stdout().flush().unwrap();

    let status = Command::new("sh")
    .arg("-c")
    .arg(cmd)
    .status();

    println!("{}", COLOR_RESET);

    match status {
        Ok(status) if status.success() => true,
        Ok(_) => {
            if !continue_on_error {
                eprintln!("{}Error executing: {}{}", COLOR_RED, cmd, COLOR_RESET);
                std::process::exit(1);
            } else {
                eprintln!("{}Command failed but continuing: {}{}", COLOR_YELLOW, cmd, COLOR_RESET);
                false
            }
        }
        Err(e) => {
            eprintln!("{}Failed to execute command: {}{}", COLOR_RED, e, COLOR_RESET);
            if !continue_on_error {
                std::process::exit(1);
            }
            false
        }
    }
}

fn print_checkbox(checked: bool) {
    if checked {
        print!("{}[✓]{}", COLOR_GREEN, COLOR_RESET);
    } else {
        print!("{}[ ]{}", COLOR_RED, COLOR_RESET);
    }
}

fn print_banner(state: &AppState) {
    clear_screen();

    println!("{}", COLOR_RED);
    println!(r"
    ░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
    ██╔══██╗██╗░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
    ██║░░╚═╝██╗░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
    ██║░░██╗██╗░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
    ╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
    ░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚══════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
    ");
    println!("{}", COLOR_RESET);
    println!("{} Advanced C++ Arch Img Iso Script Beta v2.01 24-07-2025{}", COLOR_CYAN, COLOR_RESET);

    {
        let time = state.current_time_str.lock().unwrap();
        println!("{}Current UK Time: {}{}{}", COLOR_BLUE, COLOR_CYAN, time, COLOR_RESET);
    }

    println!("{}Filesystem      Size  Used Avail Use% Mounted on{}", COLOR_GREEN, COLOR_RESET);
    execute_command("df -h / | tail -1", false);
    println!();
}

fn print_config_status(config: &ConfigState) {
    println!("{}Current Configuration:{}", COLOR_CYAN, COLOR_RESET);

    print!(" ");
    print_checkbox(config.dependencies_installed);
    println!(" Dependencies Installed");

    print!(" ");
    print_checkbox(!config.iso_tag.is_empty());
    println!(" ISO Tag: {}{}{}",
             if config.iso_tag.is_empty() { COLOR_YELLOW } else { COLOR_CYAN },
                 if config.iso_tag.is_empty() { "Not set" } else { &config.iso_tag },
                     COLOR_RESET);

    print!(" ");
    print_checkbox(!config.iso_name.is_empty());
    println!(" ISO Name: {}{}{}",
             if config.iso_name.is_empty() { COLOR_YELLOW } else { COLOR_CYAN },
                 if config.iso_name.is_empty() { "Not set" } else { &config.iso_name },
                     COLOR_RESET);

    print!(" ");
    print_checkbox(!config.output_dir.is_empty());
    println!(" Output Directory: {}{}{}",
             if config.output_dir.is_empty() { COLOR_YELLOW } else { COLOR_CYAN },
                 if config.output_dir.is_empty() { "Not set" } else { &config.output_dir },
                     COLOR_RESET);

    print!(" ");
    print_checkbox(!config.vmlinuz_path.is_empty());
    println!(" vmlinuz Selected: {}{}{}",
             if config.vmlinuz_path.is_empty() { COLOR_YELLOW } else { COLOR_CYAN },
                 if config.vmlinuz_path.is_empty() { "Not selected" } else { &config.vmlinuz_path },
                     COLOR_RESET);

    print!(" ");
    print_checkbox(config.mkinitcpio_generated);
    println!(" mkinitcpio Generated");

    print!(" ");
    print_checkbox(config.grub_edited);
    println!(" GRUB Config Edited");
}

fn get_user_input(prompt: &str) -> String {
    print!("{}{}{}", COLOR_GREEN, prompt, COLOR_RESET);
    io::stdout().flush().unwrap();

    let stdin = io::stdin();
    let mut input = String::new();
    stdin.read_line(&mut input).unwrap();
    input.trim().to_string()
}

fn install_dependencies(state: &mut AppState) {
    println!("{}\nInstalling required dependencies...\n{}", COLOR_CYAN, COLOR_RESET);

    let packages = DEPENDENCIES.join(" ");
    let command = format!("sudo pacman -Sy --needed --noconfirm {}", packages);
    execute_command(&command, false);

    state.config.dependencies_installed = true;
    save_config(&state.config, &state.username);
    println!("{}\nDependencies installed successfully!\n{}", COLOR_GREEN, COLOR_RESET);
}

fn select_vmlinuz(state: &mut AppState) {
    let boot_dir = Path::new("/boot");
    let mut vmlinuz_files = Vec::new();

    if let Ok(entries) = fs::read_dir(boot_dir) {
        for entry in entries {
            if let Ok(entry) = entry {
                let file_name = entry.file_name();
                let file_name = file_name.to_string_lossy();
                if file_name.starts_with("vmlinuz") {
                    let path = boot_dir.join(file_name.as_ref());
                    vmlinuz_files.push(path.to_string_lossy().into_owned());
                }
            }
        }
    } else {
        eprintln!("{}Could not open /boot directory{}", COLOR_RED, COLOR_RESET);
        return;
    }

    if vmlinuz_files.is_empty() {
        eprintln!("{}No vmlinuz files found in /boot!{}", COLOR_RED, COLOR_RESET);
        return;
    }

    println!("{}Available vmlinuz files:{}", COLOR_GREEN, COLOR_RESET);
    for (i, file) in vmlinuz_files.iter().enumerate() {
        println!("{}{}) {}{}", COLOR_GREEN, i + 1, file, COLOR_RESET);
    }

    let selection = get_user_input(&format!("Select vmlinuz file (1-{}): ", vmlinuz_files.len()));
    if let Ok(choice) = selection.parse::<usize>() {
        if choice > 0 && choice <= vmlinuz_files.len() {
            state.config.vmlinuz_path = vmlinuz_files[choice - 1].clone();

            let dest_path = format!("{}/boot/vmlinuz-x86_64", state.build_dir);
            let copy_cmd = format!("sudo cp {} {}", state.config.vmlinuz_path, dest_path);
            execute_command(&copy_cmd, false);

            println!("{}Selected: {}{}", COLOR_CYAN, state.config.vmlinuz_path, COLOR_RESET);
            println!("{}Copied to: {}{}", COLOR_CYAN, dest_path, COLOR_RESET);
            save_config(&state.config, &state.username);
        } else {
            eprintln!("{}Invalid selection!{}", COLOR_RED, COLOR_RESET);
        }
    } else {
        eprintln!("{}Invalid input!{}", COLOR_RED, COLOR_RESET);
    }
}

fn generate_mkinitcpio(state: &mut AppState) {
    if state.config.vmlinuz_path.is_empty() {
        eprintln!("{}Please select vmlinuz first!{}", COLOR_RED, COLOR_RESET);
        return;
    }

    if state.build_dir.is_empty() {
        eprintln!("{}Build directory not set!{}", COLOR_RED, COLOR_RESET);
        return;
    }

    println!("{}Generating initramfs...{}", COLOR_CYAN, COLOR_RESET);
    let cmd = format!(
        "cd {} && sudo mkinitcpio -c mkinitcpio.conf -g {}/boot/initramfs-x86_64.img",
        state.build_dir, state.build_dir
    );
    execute_command(&cmd, false);

    state.config.mkinitcpio_generated = true;
    save_config(&state.config, &state.username);
    println!("{}mkinitcpio generated successfully!{}", COLOR_GREEN, COLOR_RESET);
}

fn edit_grub_cfg(state: &mut AppState) {
    if state.build_dir.is_empty() {
        eprintln!("{}Build directory not set!{}", COLOR_RED, COLOR_RESET);
        return;
    }

    let grub_cfg_path = format!("{}/boot/grub/grub.cfg", state.build_dir);
    println!("{}Editing GRUB config: {}{}", COLOR_CYAN, grub_cfg_path, COLOR_RESET);

    let nano_command = format!("sudo env TERM=xterm-256color nano -Y cyanish {}", grub_cfg_path);
    execute_command(&nano_command, false);

    state.config.grub_edited = true;
    save_config(&state.config, &state.username);
    println!("{}GRUB config edited!{}", COLOR_GREEN, COLOR_RESET);
}

fn set_iso_tag(state: &mut AppState) {
    state.config.iso_tag = get_user_input("Enter ISO tag (e.g., 2025): ");
    save_config(&state.config, &state.username);
}

fn set_iso_name(state: &mut AppState) {
    state.config.iso_name = get_user_input("Enter ISO name (e.g., claudemods.iso): ");
    save_config(&state.config, &state.username);
}

fn set_output_dir(state: &mut AppState) {
    let default_dir = format!("/home/{}/Downloads", state.username);
    println!(
        "{}Current output directory: {}{}{}",
        COLOR_GREEN,
        if state.config.output_dir.is_empty() {
            COLOR_YELLOW
        } else {
            COLOR_CYAN
        },
        if state.config.output_dir.is_empty() {
            "Not set"
        } else {
            &state.config.output_dir
        },
        COLOR_RESET
    );
    println!("{}Default directory: {}{}{}", COLOR_GREEN, COLOR_CYAN, default_dir, COLOR_RESET);

    let mut input = get_user_input(&format!("Enter output directory (e.g., {} or $USER/Downloads): ", default_dir));

    if input.contains("$USER") {
        input = input.replace("$USER", &state.username);
    }

    state.config.output_dir = if input.is_empty() { default_dir } else { input };

    execute_command(&format!("mkdir -p {}", state.config.output_dir), true);
    save_config(&state.config, &state.username);
}

fn show_menu(state: &AppState, title: &str, items: &[String], selected: usize) {
    clear_screen();
    print_banner(state);
    print_config_status(&state.config);

    println!("{}\n  {}{}", COLOR_CYAN, title, COLOR_RESET);
    println!("{}  {}{}", COLOR_CYAN, "-".repeat(title.len()), COLOR_RESET);

    for (i, item) in items.iter().enumerate() {
        if i == selected {
            println!("{}➤ {}{}", COLOR_HIGHLIGHT, item, COLOR_RESET);
        } else {
            println!("{}  {}{}", COLOR_NORMAL, item, COLOR_RESET);
        }
    }
}

fn show_filesystem_menu(state: &AppState, items: &[String], selected: usize) {
    clear_screen();
    print_banner(state);

    println!("{}\n  Choose filesystem type:{}", COLOR_CYAN, COLOR_RESET);
    println!("{}  ----------------------{}", COLOR_CYAN, COLOR_RESET);

    for (i, item) in items.iter().enumerate() {
        if i == selected {
            println!("{}➤ {}{}", COLOR_HIGHLIGHT, item, COLOR_RESET);
        } else {
            println!("{}  {}{}", COLOR_NORMAL, item, COLOR_RESET);
        }
    }
}

fn validate_size_input(input: &str) -> bool {
    input.parse::<f64>().map(|size| size > 0.1).unwrap_or(false)
}

fn show_setup_menu(state: &mut AppState) {
    let items = vec![
        "Install Dependencies".to_string(),
        "Set ISO Tag".to_string(),
        "Set ISO Name".to_string(),
        "Set Output Directory".to_string(),
        "Select vmlinuz".to_string(),
        "Generate mkinitcpio".to_string(),
        "Edit GRUB Config".to_string(),
        "Back to Main Menu".to_string(),
    ];

    let mut selected = 0;

    loop {
        show_menu(state, "ISO Creation Setup Menu:", &items, selected);

        let key = getch();
        match key {
            'A' => if selected > 0 { selected -= 1 },
            'B' => if selected < items.len() - 1 { selected += 1 },
            '\n' => {
                match selected {
                    0 => install_dependencies(state),
                    1 => set_iso_tag(state),
                    2 => set_iso_name(state),
                    3 => set_output_dir(state),
                    4 => select_vmlinuz(state),
                    5 => generate_mkinitcpio(state),
                    6 => edit_grub_cfg(state),
                    7 => return,
                    _ => {}
                }

                if selected != 7 {
                    println!("{}\nPress any key to continue...{}", COLOR_GREEN, COLOR_RESET);
                    getch();
                }
            }
            _ => {}
        }
    }
}

fn create_image_file(size_str: &str, filename: &str) -> bool {
    match size_str.parse::<f64>() {
        Ok(size_gb) if size_gb > 0.0 => {
            let size_bytes = (size_gb * 1073741824.0) as u64;
            println!(
                "{}Creating image file of size {}GB ({} bytes)...{}",
                     COLOR_CYAN, size_gb, size_bytes, COLOR_RESET
            );

            let cmd = format!("sudo fallocate -l {} {}", size_bytes, filename);
            execute_command(&cmd, true)
        }
        _ => {
            eprintln!("{}Invalid size: must be greater than 0{}", COLOR_RED, COLOR_RESET);
            false
        }
    }
}

fn format_filesystem(fs_type: &str, filename: &str) -> bool {
    if fs_type == "btrfs" {
        println!(
            "{}Creating Btrfs filesystem with {} compression{}",
            COLOR_CYAN, BTRFS_COMPRESSION, COLOR_RESET
        );

        execute_command(&format!("sudo mkdir -p {}/btrfs_rootdir", SOURCE_DIR), true);

        let cmd = format!(
            "sudo mkfs.btrfs -L \"{}\" --compress={} --rootdir={}/btrfs_rootdir -f {}",
            BTRFS_LABEL, BTRFS_COMPRESSION, SOURCE_DIR, filename
        );
        execute_command(&cmd, true);

        execute_command(&format!("sudo rmdir {}/btrfs_rootdir", SOURCE_DIR), true)
    } else {
        println!("{}Formatting as ext4 with SquashFS-style compression{}", COLOR_CYAN, COLOR_RESET);
        let cmd = format!(
            "sudo mkfs.ext4 -F -O ^has_journal,^resize_inode -E lazy_itable_init=0 -m 0 -L \"SYSTEM_BACKUP\" {}",
            filename
        );
        execute_command(&cmd, true)
    }
}

fn mount_filesystem(fs_type: &str, filename: &str, mount_point: &str) -> bool {
    execute_command(&format!("sudo mkdir -p {}", mount_point), true);

    if fs_type == "btrfs" {
        let cmd = format!(
            "sudo mount -o compress={},compress-force=zstd:{} {} {}",
            BTRFS_COMPRESSION, COMPRESSION_LEVEL, filename, mount_point
        );
        execute_command(&cmd, true)
    } else {
        let options = "loop,discard,noatime,data=writeback,commit=60,barrier=0,nobh,errors=remount-ro";
        let cmd = format!("sudo mount -o {} {} {}", options, filename, mount_point);
        execute_command(&cmd, true)
    }
}

fn copy_files_with_rsync(source: &str, destination: &str, fs_type: &str) -> bool {
    let compression_flag = if fs_type == "btrfs" { "--compress" } else { "" };
    let compression_level = if fs_type == "btrfs" {
        format!("--compress-level={}", COMPRESSION_LEVEL)
    } else {
        String::new()
    };

    let cmd = format!(
        "sudo rsync -aHAXSr --numeric-ids --info=progress2 {} {} \
--exclude=/etc/udev/rules.d/70-persistent-cd.rules \
--exclude=/etc/udev/rules.d/70-persistent-net.rules \
--exclude=/etc/mtab \
--exclude=/etc/fstab \
--exclude=/dev/* \
--exclude=/proc/* \
--exclude=/sys/* \
--exclude=/tmp/* \
--exclude=/run/* \
--exclude=/mnt/* \
--exclude=/media/* \
--exclude=/lost+found \
--exclude=*rootfs1.img \
--exclude=btrfs_temp \
--exclude=rootfs.img \
{} {}",
compression_flag, compression_level, source, destination
    );
    execute_command(&cmd, true);

    if fs_type == "btrfs" {
        println!("{}Optimizing compression...{}", COLOR_CYAN, COLOR_RESET);
        let cmd = format!(
            "sudo btrfs filesystem defrag -r -v -c {} {}",
            BTRFS_COMPRESSION, destination
        );
        execute_command(&cmd, true);
        execute_command(&format!("sudo btrfs filesystem resize max {}", destination), true)
    } else {
        println!("{}Optimizing ext4 filesystem...{}", COLOR_CYAN, COLOR_RESET);
        execute_command(&format!("sudo e4defrag {}", destination), true);
        execute_command(
            &format!(
                "sudo tune2fs -o journal_data_writeback {}",
                destination.split(' ').next().unwrap_or("")
            ),
            true,
        )
    }
}

fn unmount_and_cleanup(mount_point: &str) -> bool {
    execute_command("sync", true);
    execute_command(&format!("sudo umount {}", mount_point), true);
    execute_command(&format!("sudo rmdir {}", mount_point), true)
}

fn create_squashfs(input_file: &str, output_file: &str) -> bool {
    println!("{}Creating optimized SquashFS image...{}", COLOR_CYAN, COLOR_RESET);

    let cmd = format!(
        "sudo mksquashfs {} {} -comp {} -Xcompression-level {} -noappend -b 256K -Xbcj x86",
        input_file, output_file, SQUASHFS_COMPRESSION, COMPRESSION_LEVEL
    );
    execute_command(&cmd, true)
}

fn create_checksum(filename: &str) -> bool {
    let cmd = format!("md5sum {} > {}.md5", filename, filename);
    execute_command(&cmd, true)
}

fn print_final_message(fs_type: &str, output_file: &str) {
    println!();
    if fs_type == "btrfs" {
        println!(
            "{}Compressed BTRFS image created successfully: {}{}",
            COLOR_CYAN, output_file, COLOR_RESET
        );
    } else {
        println!(
            "{}Compressed ext4 image created successfully: {}{}",
            COLOR_CYAN, output_file, COLOR_RESET
        );
    }
    println!(
        "{}Checksum file: {}.md5{}",
        COLOR_CYAN, output_file, COLOR_RESET
    );
    println!("{}Size: {}", COLOR_CYAN, COLOR_RESET);
    execute_command(
        &format!("sudo du -h {} | cut -f1", output_file),
                    true,
    );
}

fn get_output_directory(username: &str) -> String {
    format!("/home/{}/.config/cmi/build-image-arch-img/LiveOS", username)
}

fn expand_path(path: &str, username: &str) -> String {
    path.replace("~", &format!("/home/{}", username))
    .replace("$USER", username)
}

fn create_iso(state: &AppState) -> bool {
    if !state.config.is_ready_for_iso() {
        eprintln!("{}Cannot create ISO - setup is incomplete!{}", COLOR_RED, COLOR_RESET);
        return false;
    }

    println!("{}\nStarting ISO creation process...\n{}", COLOR_CYAN, COLOR_RESET);

    let expanded_output_dir = expand_path(&state.config.output_dir, &state.username);
    execute_command(&format!("mkdir -p {}", expanded_output_dir), true);

    let xorriso_cmd = format!(
        "sudo xorriso -as mkisofs \
--modification-date=\"$(date +%Y%m%d%H%M%S00)\" \
--protective-msdos-label \
-volid \"{}\" \
-appid \"claudemods Linux Live/Rescue CD\" \
-publisher \"claudemods claudemods101@gmail.com >\" \
-preparer \"Prepared by user\" \
-r -graft-points -no-pad \
--sort-weight 0 / \
--sort-weight 1 /boot \
--grub2-mbr {}/boot/grub/i386-pc/boot_hybrid.img \
-partition_offset 16 \
-b boot/grub/i386-pc/eltorito.img \
-c boot.catalog \
-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info \
-eltorito-alt-boot \
-append_partition 2 0xef {}/boot/efi.img \
-e --interval:appended_partition_2:all:: \
-no-emul-boot \
-iso-level 3 \
-o \"{}/{}\" {}",
state.config.iso_tag,
state.build_dir,
state.build_dir,
expanded_output_dir,
state.config.iso_name,
state.build_dir
    );

    execute_command(&xorriso_cmd, true);
    println!(
        "{}ISO created successfully at {}/{} {}",
        COLOR_CYAN,
        expanded_output_dir,
        state.config.iso_name,
        COLOR_RESET
    );
    true
}

fn show_guide(username: &str) {
    let readme_path = format!("/home/{}/.config/cmi/readme.txt", username);
    execute_command(&format!("mkdir -p /home/{}/.config/cmi", username), true);

    println!("{}", COLOR_CYAN);
    execute_command(&format!("nano {}", readme_path), true);
    println!("{}", COLOR_RESET);
}

fn install_iso_to_usb(state: &AppState) {
    if state.config.output_dir.is_empty() {
        eprintln!("{}Output directory not set!{}", COLOR_RED, COLOR_RESET);
        return;
    }

    let expanded_output_dir = expand_path(&state.config.output_dir, &state.username);
    let mut iso_files = Vec::new();

    if let Ok(entries) = fs::read_dir(&expanded_output_dir) {
        for entry in entries {
            if let Ok(entry) = entry {
                let file_name = entry.file_name();
                let file_name = file_name.to_string_lossy();
                if file_name.ends_with(".iso") {
                    iso_files.push(file_name.into_owned());
                }
            }
        }
    } else {
        eprintln!(
            "{}Could not open output directory: {}{}",
            COLOR_RED, expanded_output_dir, COLOR_RESET
        );
        return;
    }

    if iso_files.is_empty() {
        eprintln!("{}No ISO files found in output directory!{}", COLOR_RED, COLOR_RESET);
        return;
    }

    println!("{}Available ISO files:{}", COLOR_GREEN, COLOR_RESET);
    for (i, file) in iso_files.iter().enumerate() {
        println!("{}{}) {}{}", COLOR_GREEN, i + 1, file, COLOR_RESET);
    }

    let selection = get_user_input(&format!("Select ISO file (1-{}): ", iso_files.len()));
    let choice = match selection.parse::<usize>() {
        Ok(n) if n >= 1 && n <= iso_files.len() => n - 1,
        _ => {
            eprintln!("{}Invalid selection!{}", COLOR_RED, COLOR_RESET);
            return;
        }
    };

    let selected_iso = format!("{}/{}", expanded_output_dir, iso_files[choice]);

    println!("{}\nAvailable drives:{}", COLOR_CYAN, COLOR_RESET);
    execute_command("lsblk -d -o NAME,SIZE,MODEL | grep -v 'loop'", true);

    let target_drive = get_user_input("\nEnter target drive (e.g., /dev/sda): ");
    if target_drive.is_empty() {
        eprintln!("{}No drive specified!{}", COLOR_RED, COLOR_RESET);
        return;
    }

    println!(
        "{}\nWARNING: This will overwrite all data on {}!{}",
        COLOR_RED, target_drive, COLOR_RESET
    );
    let confirm = get_user_input("Are you sure you want to continue? (y/N): ");
    if confirm != "y" && confirm != "Y" {
        println!("{}Operation cancelled.{}", COLOR_CYAN, COLOR_RESET);
        return;
    }

    println!(
        "{}\nWriting {} to {}...{}",
        COLOR_CYAN, selected_iso, target_drive, COLOR_RESET
    );
    let dd_cmd = format!(
        "sudo dd if={} of={} bs=4M status=progress oflag=sync",
        selected_iso, target_drive
    );
    execute_command(&dd_cmd, true);

    println!(
        "{}\nISO successfully written to USB drive!{}",
        COLOR_GREEN, COLOR_RESET
    );
    println!("{}Press any key to continue...{}", COLOR_GREEN, COLOR_RESET);
    getch();
}

fn run_cmi_installer() {
    execute_command("cmirsyncinstaller", true);
    println!("{}\nPress any key to continue...{}", COLOR_GREEN, COLOR_RESET);
    getch();
}

fn update_script() {
    println!("{}\nUpdating script from GitHub...{}", COLOR_CYAN, COLOR_RESET);
    execute_command(
        "bash -c \"$(curl -fsSL https://raw.githubusercontent.com/claudemods/claudemods-multi-iso-konsole-script/main/advancedimgscript/installer/patch.sh)\"",
                    false,
    );
    println!("{}\nScript updated successfully!{}", COLOR_GREEN, COLOR_RESET);
    println!("{}Press any key to continue...{}", COLOR_GREEN, COLOR_RESET);
    getch();
}

fn show_main_menu(state: &mut AppState) {
    let items = vec![
        "Guide".to_string(),
        "Setup Scripts".to_string(),
        "Create Image".to_string(),
        "Create ISO".to_string(),
        "Show Disk Usage".to_string(),
        "Install ISO to USB".to_string(),
        "CMI BTRFS/EXT4 Installer".to_string(),
        "Update Script".to_string(),
        "Exit".to_string(),
    ];

    let mut selected = 0;

    loop {
        show_menu(state, "Main Menu:", &items, selected);

        let key = getch();
        match key {
            'A' => if selected > 0 { selected -= 1 },
            'B' => if selected < items.len() - 1 { selected += 1 },
            '\n' => {
                match selected {
                    0 => show_guide(&state.username),
                    1 => show_setup_menu(state),
                    2 => {
                        let output_dir = get_output_directory(&state.username);
                        let output_img_path = format!("{}/{}", output_dir, ORIG_IMG_NAME);

                        execute_command(&format!("sudo umount {} 2>/dev/null || true", MOUNT_POINT), true);
                        execute_command(&format!("sudo rm -rf {}", output_img_path), true);
                        execute_command(&format!("sudo mkdir -p {}", MOUNT_POINT), true);
                        execute_command(&format!("sudo mkdir -p {}", output_dir), true);

                        // Get size input with validation
                        let size_input = loop {
                            let input = get_user_input("Enter the image size in GB (e.g., 1.2 for 1.2GB): ");
                            if validate_size_input(&input) {
                                break input;
                            }
                            eprintln!("{}Invalid size! Please enter a positive number (e.g., 1.2){}", COLOR_RED, COLOR_RESET);
                        };

                        let fs_options = vec!["btrfs".to_string(), "ext4".to_string()];
                        let mut fs_selected = 0;
                        let mut fs_chosen = false;

                        while !fs_chosen {
                            show_filesystem_menu(state, &fs_options, fs_selected);
                            let fs_key = getch();
                            match fs_key {
                                'A' => if fs_selected > 0 { fs_selected -= 1 },
                                'B' => if fs_selected < fs_options.len() - 1 { fs_selected += 1 },
                                '\n' => fs_chosen = true,
                                _ => {}
                            }
                        }

                        let fs_type = if fs_selected == 0 { "btrfs" } else { "ext4" };

                        if !create_image_file(&size_input, &output_img_path) ||
                            !format_filesystem(fs_type, &output_img_path) ||
                            !mount_filesystem(fs_type, &output_img_path, MOUNT_POINT) ||
                            !copy_files_with_rsync(SOURCE_DIR, MOUNT_POINT, fs_type)
                            {
                                break;
                            }

                            unmount_and_cleanup(MOUNT_POINT);

                        if fs_type == "ext4" {
                            // For ext4, create SquashFS version
                            let squash_path = format!("{}/{}", output_dir, FINAL_IMG_NAME);
                            create_squashfs(&output_img_path, &squash_path);
                        }

                        create_checksum(&output_img_path);
                        print_final_message(fs_type, &output_img_path);

                        println!("{}\nPress any key to continue...{}", COLOR_GREEN, COLOR_RESET);
                        getch();
                    }
                    3 => {
                        create_iso(state);
                        println!("{}\nPress any key to continue...{}", COLOR_GREEN, COLOR_RESET);
                        getch();
                    }
                    4 => {
                        execute_command("df -h", false);
                        println!("{}\nPress any key to continue...{}", COLOR_GREEN, COLOR_RESET);
                        getch();
                    }
                    5 => install_iso_to_usb(state),
                    6 => run_cmi_installer(),
                    7 => update_script(),
                    8 => return,
                    _ => {}
                }
            }
            _ => {}
        }
    }
}

fn main() {
    let mut state = AppState::new();

    // Start time update thread
    let running = state.time_thread_running.clone();
    let time_str = state.current_time_str.clone();
    thread::spawn(move || {
        update_time_thread(running, time_str);
    });

    // Save terminal settings
    let stdin = io::stdin();
    let fd = stdin.as_raw_fd();
    let mut termios = Termios::from_fd(fd).unwrap();
    let original = termios.clone();
    termios.c_lflag &= !ICANON;
    tcsetattr(fd, TCSANOW, &termios).unwrap();

    show_main_menu(&mut state);

    // Clean up
    state.time_thread_running.store(false, Ordering::Relaxed);
    tcsetattr(fd, TCSANOW, &original).unwrap();
}
