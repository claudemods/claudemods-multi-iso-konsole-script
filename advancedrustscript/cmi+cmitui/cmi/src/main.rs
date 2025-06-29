use std::{
    env, fs,
    io::{self, BufRead},
    path::{Path, PathBuf},
    process::Command,
    fmt,
};

// Constants (only keeping what's actually used)
const GREEN: &str = "\x1b[32m";
const RED: &str = "\x1b[31m";
const RESET: &str = "\x1b[0m";
const COLOR_CYAN: &str = "\x1b[38;2;0;255;255m";

// Paths
const VERSION_FILE: &str = "/home/$USER/.config/cmi/version.txt";
const GUIDE_PATH: &str = "/home/$USER/.config/cmi/readme.txt";
const CHANGELOG_PATH: &str = "/home/$USER/.config/cmi/changelog.txt";
const CLONE_DIR_FILE: &str = "/home/$USER/.config/cmi/clonedir.txt";
const ISO_NAME_FILE: &str = "/home/$USER/.config/cmi/isoname.txt";

#[derive(Debug, Clone, Copy, PartialEq)]
enum Distro {
    Arch,
    Ubuntu,
    Debian,
    Cachyos,
    Neon,
    Unknown,
}

impl fmt::Display for Distro {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Distro::Arch => write!(f, "Arch Linux"),
            Distro::Ubuntu => write!(f, "Ubuntu"),
            Distro::Debian => write!(f, "Debian"),
            Distro::Cachyos => write!(f, "CachyOS"),
            Distro::Neon => write!(f, "KDE Neon"),
            Distro::Unknown => write!(f, "Unknown"),
        }
    }
}

struct CmiUtils;

impl CmiUtils {
    fn expand_path(path: &str) -> PathBuf {
        let user = env::var("USER").unwrap_or_else(|_| "".to_string());
        PathBuf::from(path.replace("$USER", &user))
    }

    fn get_cmi_version() -> String {
        let path = Self::expand_path(VERSION_FILE);
        fs::read_to_string(path).unwrap_or_else(|_| "unknown".to_string())
        .lines().next().unwrap_or("unknown").to_string()
    }

    fn get_kernel_version() -> String {
        let output = Command::new("uname").arg("-r").output()
        .expect("Failed to execute uname");
        String::from_utf8_lossy(&output.stdout).trim().to_string()
    }

    fn get_vmlinuz_version() -> String {
        let output = Command::new("ls").arg("/boot/vmlinuz*").output()
        .expect("Failed to execute ls");
        let output_str = String::from_utf8_lossy(&output.stdout);
        output_str.lines().next()
        .map(|s| s.split('/').last().unwrap_or("unknown").to_string())
        .unwrap_or_else(|| "unknown".to_string())
    }

    fn get_distro_version() -> String {
        if let Ok(file) = fs::File::open("/etc/os-release") {
            let reader = io::BufReader::new(file);
            for line in reader.lines().flatten() {
                if line.starts_with("VERSION_ID=") {
                    let version = line.trim_start_matches("VERSION_ID=");
                    return version.trim_matches('"').to_string();
                }
            }
        }
        "unknown".to_string()
    }

    fn detect_distro() -> Distro {
        if let Ok(file) = fs::File::open("/etc/os-release") {
            let reader = io::BufReader::new(file);
            for line in reader.lines().flatten() {
                if line.starts_with("ID=") {
                    let id = line.trim_start_matches("ID=");
                    if id.contains("arch") { return Distro::Arch; }
                    if id.contains("ubuntu") { return Distro::Ubuntu; }
                    if id.contains("debian") { return Distro::Debian; }
                    if id.contains("cachyos") { return Distro::Cachyos; }
                    if id.contains("neon") { return Distro::Neon; }
                }
            }
        }
        Distro::Unknown
    }

    fn execute_command(cmd: &str) -> Result<(), String> {
        let status = Command::new("sh")
        .arg("-c")
        .arg(cmd)
        .status()
        .map_err(|e| e.to_string())?;

        if status.success() {
            Ok(())
        } else {
            Err(format!("Command failed: {}", cmd))
        }
    }

    fn read_clone_dir() -> Option<String> {
        let path = Self::expand_path(CLONE_DIR_FILE);
        if let Ok(content) = fs::read_to_string(path) {
            let mut dir = content.trim().to_string();
            if !dir.ends_with('/') {
                dir.push('/');
            }
            Some(dir)
        } else {
            None
        }
    }

    fn dir_exists(path: &str) -> bool {
        Path::new(path).is_dir()
    }

    fn save_clone_dir(dir_path: &str) -> Result<(), String> {
        let config_dir = Self::expand_path("/home/$USER/.config/cmi");
        if !config_dir.exists() {
            Self::execute_command(&format!("mkdir -p {}", config_dir.display()))?;
        }

        let mut full_clone_path = dir_path.to_string();
        if !full_clone_path.ends_with('/') {
            full_clone_path.push('/');
        }

        let file_path = config_dir.join("clonedir.txt");
        fs::write(file_path, &full_clone_path).map_err(|e| e.to_string())?;

        let clone_folder = format!("{}clone_system_temp", full_clone_path);
        if !Self::dir_exists(&clone_folder) {
            Self::execute_command(&format!("mkdir -p {}", clone_folder))?;
        }

        Ok(())
    }

    fn save_iso_name(name: &str) -> Result<(), String> {
        let config_dir = Self::expand_path("/home/$USER/.config/cmi");
        if !config_dir.exists() {
            Self::execute_command(&format!("mkdir -p {}", config_dir.display()))?;
        }

        let file_path = config_dir.join("isoname.txt");
        fs::write(file_path, name).map_err(|e| e.to_string())
    }

    fn get_iso_name() -> Option<String> {
        let path = Self::expand_path(ISO_NAME_FILE);
        fs::read_to_string(path).ok()
        .map(|s| s.trim().to_string())
    }

    fn is_init_generated(distro: Distro) -> bool {
        let init_path = match distro {
            Distro::Ubuntu | Distro::Debian => {
                let distro_name = if distro == Distro::Ubuntu { "noble" } else { "debian" };
                format!("/home/$USER/.config/cmi/build-image-{}/live/initrd.img-{}",
                    distro_name, Self::get_kernel_version())
            },
            _ => "/home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img".to_string(),
        };
        let expanded_path = Self::expand_path(&init_path);
        expanded_path.exists()
    }

    fn install_dependencies(distro: Distro) -> Result<(), String> {
        match distro {
            Distro::Arch | Distro::Cachyos => {
                Self::execute_command(
                    "sudo pacman -S --needed --noconfirm arch-install-scripts bash-completion \
dosfstools erofs-utils findutils grub jq libarchive libisoburn lsb-release \
lvm2 mkinitcpio-archiso mkinitcpio-nfs-utils mtools nbd pacman-contrib \
parted procps-ng pv python rsync squashfs-tools sshfs syslinux xdg-utils \
bash-completion zsh-completions kernel-modules-hook virt-manager"
                )
            },
            Distro::Ubuntu | Distro::Neon => {
                Self::execute_command(
                    "sudo apt install -y cryptsetup dmeventd isolinux libaio-dev libcares2 \
libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot live-boot-doc \
live-boot-initramfs-tools live-config-systemd live-tools lvm2 pxelinux \
syslinux syslinux-common thin-provisioning-tools squashfs-tools xorriso"
                )
            },
            Distro::Debian => {
                Self::execute_command(
                    "sudo apt install -y cryptsetup dmeventd isolinux libaio1 libc-ares2 \
libdevmapper-event1.02.1 liblvm2cmd2.03 live-boot live-boot-doc \
live-boot-initramfs-tools live-config-systemd live-tools lvm2 pxelinux \
syslinux syslinux-common thin-provisioning-tools squashfs-tools xorriso"
                )
            },
            Distro::Unknown => Err("Unknown distribution".to_string()),
        }
    }

    fn generate_initrd(distro: Distro) -> Result<(), String> {
        match distro {
            Distro::Arch | Distro::Cachyos => {
                let build_dir = Self::expand_path("/home/$USER/.config/cmi/build-image-arch");
                let init_path = Self::expand_path("/home/$USER/.config/cmi/build-image-arch/live/initramfs-linux.img");
                Self::execute_command(&format!(
                    "cd {} >/dev/null 2>&1 && sudo mkinitcpio -c live.conf -g {}",
                    build_dir.display(),
                                               init_path.display()
                ))?;
                Self::execute_command(&format!(
                    "sudo cp /boot/vmlinuz-linux* {} 2>/dev/null",
                    Self::expand_path("/home/$USER/.config/cmi/build-image-arch/live/").display()
                ))
            },
            Distro::Ubuntu | Distro::Neon => {
                let init_path = Self::expand_path("/home/$USER/.config/cmi/build-image-noble/live/initrd.img-$(uname -r)");
                Self::execute_command(&format!(
                    "sudo mkinitramfs -o \"{}\" \"$(uname -r)\"",
                                               init_path.display()
                ))?;
                Self::execute_command(&format!(
                    "sudo cp /boot/vmlinuz* {} 2>/dev/null",
                    Self::expand_path("/home/$USER/.config/cmi/build-image-noble/live/").display()
                ))
            },
            Distro::Debian => {
                let init_path = Self::expand_path("/home/$USER/.config/cmi/build-image-debian/live/initrd.img-$(uname -r)");
                Self::execute_command(&format!(
                    "sudo mkinitramfs -o \"{}\" \"$(uname -r)\"",
                                               init_path.display()
                ))?;
                Self::execute_command(&format!(
                    "sudo cp /boot/vmlinuz* {} 2>/dev/null",
                    Self::expand_path("/home/$USER/.config/cmi/build-image-debian/live/").display()
                ))
            },
            Distro::Unknown => Err("Unknown distribution".to_string()),
        }
    }

    fn edit_isolinux_cfg(distro: Distro) -> Result<(), String> {
        let path = match distro {
            Distro::Arch | Distro::Cachyos => Self::expand_path("/home/$USER/.config/cmi/build-image-arch/isolinux/isolinux.cfg"),
            Distro::Ubuntu | Distro::Neon => Self::expand_path("/home/$USER/.config/cmi/build-image-noble/isolinux/isolinux.cfg"),
            Distro::Debian => Self::expand_path("/home/$USER/.config/cmi/build-image-debian/isolinux/isolinux.cfg"),
            Distro::Unknown => return Err("Unknown distribution".to_string()),
        };
        Self::execute_command(&format!("nano {}", path.display()))
    }

    fn edit_grub_cfg(distro: Distro) -> Result<(), String> {
        let path = match distro {
            Distro::Arch | Distro::Cachyos => Self::expand_path("/home/$USER/.config/cmi/build-image-arch/boot/grub/grub.cfg"),
            Distro::Ubuntu | Distro::Neon => Self::expand_path("/home/$USER/.config/cmi/build-image-noble/boot/grub/grub.cfg"),
            Distro::Debian => Self::expand_path("/home/$USER/.config/cmi/build-image-debian/boot/grub/grub.cfg"),
            Distro::Unknown => return Err("Unknown distribution".to_string()),
        };
        Self::execute_command(&format!("nano {}", path.display()))
    }

    fn install_calamares(distro: Distro) -> Result<(), String> {
        match distro {
            Distro::Arch => {
                let calamares_dir = Self::expand_path("/home/$USER/.config/cmi/calamares-per-distro/arch");
                Self::execute_command(&format!(
                    "cd {} >/dev/null 2>&1 && sudo pacman -U calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst \
calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar \
ckbcomp-1.227-2-any.pkg.tar.zst",
calamares_dir.display()
                ))
            },
            Distro::Cachyos => {
                Self::execute_command(
                    "sudo pacman -U --noconfirm calamares-3.3.14-5-x86_64_REPACKED.pkg.tar.zst \
calamares-oem-kde-settings-20240616-3-any.pkg.tar calamares-tools-0.1.0-1-any.pkg.tar \
ckbcomp-1.227-2-any.pkg.tar.zst"
                )
            },
            Distro::Ubuntu | Distro::Neon => {
                Self::execute_command(
                    "sudo apt install -y calamares-settings-ubuntu-common calamares"
                )
            },
            Distro::Debian => {
                Self::execute_command(
                    "sudo apt install -y calamares-settings-debian calamares"
                )
            },
            Distro::Unknown => Err("Unknown distribution".to_string()),
        }
    }

    fn edit_calamares_branding() -> Result<(), String> {
        Self::execute_command("sudo nano /usr/share/calamares/branding/claudemods/branding.desc")
    }

    fn clone_system(clone_dir: &str) -> Result<(), String> {
        let mut full_clone_path = clone_dir.to_string();
        if !full_clone_path.ends_with('/') {
            full_clone_path.push('/');
        }
        full_clone_path.push_str("clone_system_temp");

        if !Self::dir_exists(&full_clone_path) {
            Self::execute_command(&format!("mkdir -p {}", full_clone_path))?;
        }

        let command = format!(
            "sudo rsync -aHAXSr --numeric-ids --info=progress2 \
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
--exclude=clone_system_temp \
--include=dev \
--include=proc \
--include=tmp \
--include=sys \
--include=run \
--include=dev \
--include=proc \
--include=tmp \
--include=sys \
--include=usr \
--include=etc \
/ {}",
full_clone_path
        );

        Self::execute_command(&command)
    }

    fn create_squashfs(distro: Distro) -> Result<(), String> {
        let clone_dir = Self::read_clone_dir().ok_or("Clone directory not set")?;
        let mut full_clone_path = clone_dir;
        if !full_clone_path.ends_with('/') {
            full_clone_path.push('/');
        }
        full_clone_path.push_str("clone_system_temp");

        let output_path = match distro {
            Distro::Ubuntu | Distro::Neon => {
                Self::expand_path("/home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs")
            },
            Distro::Debian => {
                Self::expand_path("/home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs")
            },
            _ => {
                Self::expand_path("/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs")
            },
        };

        let command = format!(
            "sudo mksquashfs {} {} -comp xz -Xbcj x86 -b 1M -no-duplicates -no-recovery \
-always-use-fragments -wildcards -xattrs",
full_clone_path,
output_path.display()
        );

        Self::execute_command(&command)
    }

    fn create_squashfs_clone(distro: Distro) -> Result<(), String> {
        let clone_dir = Self::read_clone_dir().ok_or("Clone directory not set")?;
        let full_clone_path = format!("{}clone_system_temp", clone_dir);

        if !Self::dir_exists(&full_clone_path) {
            Self::clone_system(&clone_dir)?;
        }
        Self::create_squashfs(distro)
    }

    fn delete_clone(distro: Distro) -> Result<(), String> {
        let clone_dir = Self::read_clone_dir().ok_or("Clone directory not set")?;
        let mut full_clone_path = clone_dir;
        if !full_clone_path.ends_with('/') {
            full_clone_path.push('/');
        }
        full_clone_path.push_str("clone_system_temp");

        Self::execute_command(&format!("sudo rm -rf {}", full_clone_path))?;

        let squashfs_path = match distro {
            Distro::Ubuntu | Distro::Neon => {
                Self::expand_path("/home/$USER/.config/cmi/build-image-noble/live/filesystem.sfs")
            },
            Distro::Debian => {
                Self::expand_path("/home/$USER/.config/cmi/build-image-debian/live/filesystem.sfs")
            },
            _ => {
                Self::expand_path("/home/$USER/.config/cmi/build-image-arch/arch/x86_64/airootfs.sfs")
            },
        };

        if squashfs_path.exists() {
            Self::execute_command(&format!("sudo rm -f {}", squashfs_path.display()))
        } else {
            Ok(())
        }
    }

    fn create_iso(distro: Distro, output_dir: &str) -> Result<(), String> {
        let iso_name = Self::get_iso_name().ok_or("ISO name not set")?;
        if output_dir.is_empty() {
            return Err("Output directory not specified".to_string());
        }

        let timestamp = chrono::Local::now().format("%Y-%m-%d_%H%M").to_string();
        let iso_file_name = format!("{}/{}_amd64_{}.iso", output_dir, iso_name, timestamp);

        let build_image_dir = match distro {
            Distro::Ubuntu | Distro::Neon => {
                Self::expand_path("/home/$USER/.config/cmi/build-image-noble")
            },
            Distro::Debian => {
                Self::expand_path("/home/$USER/.config/cmi/build-image-debian")
            },
            _ => {
                Self::expand_path("/home/$USER/.config/cmi/build-image-arch")
            },
        };

        if !Self::dir_exists(output_dir) {
            Self::execute_command(&format!("mkdir -p {}", output_dir))?;
        }

        let mut xorriso_command = format!(
            "sudo xorriso -as mkisofs -o {} -V 2025 -iso-level 3",
            iso_file_name
        );

        xorriso_command.push_str(match distro {
            Distro::Ubuntu | Distro::Neon => {
                " -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin \
-c isolinux/boot.cat \
-b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
-eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot -isohybrid-gpt-basdat"
            },
            Distro::Debian => {
                " -isohybrid-mbr /usr/lib/ISOLINUX/isohdpfx.bin \
-c isolinux/boot.cat \
-b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
-eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat"
            },
            _ => {
                " -isohybrid-mbr /usr/lib/syslinux/bios/isohdpfx.bin \
-c isolinux/boot.cat \
-b isolinux/isolinux.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
-eltorito-alt-boot -e boot/grub/efiboot.img -no-emul-boot -isohybrid-gpt-basdat"
            },
        });

        xorriso_command.push_str(&format!(" {}", build_image_dir.display()));

        Self::execute_command(&xorriso_command)
    }

    fn run_qemu() -> Result<(), String> {
        Self::execute_command("ruby /opt/claudemods-iso-konsole-script/Supported-Distros/qemu.rb")
    }

    fn install_updater() -> Result<(), String> {
        Self::execute_command(&format!(
            "{}",
            Self::expand_path("./home/$USER/.config/cmi/patch.sh").display()
        ))
    }

    fn show_guide() -> Result<(), String> {
        Self::execute_command(&format!(
            "cat {}",
            Self::expand_path(GUIDE_PATH).display()
        ))
    }

    fn show_changelog() -> Result<(), String> {
        Self::execute_command(&format!(
            "cat {}",
            Self::expand_path(CHANGELOG_PATH).display()
        ))
    }

    fn show_status(distro: Distro) {
        let distro_name = distro.to_string();
        let distro_version = Self::get_distro_version();
        let cmi_version = Self::get_cmi_version();
        let kernel_version = Self::get_kernel_version();
        let vmlinuz_version = Self::get_vmlinuz_version();
        let clone_dir = Self::read_clone_dir();
        let iso_name = Self::get_iso_name();
        let init_generated = Self::is_init_generated(distro);

        println!("Current Distribution: {}{} {}{}", COLOR_CYAN, distro_name, distro_version, RESET);
        println!("CMI Version: {}{}{}", COLOR_CYAN, cmi_version, RESET);
        println!("Kernel Version: {}{}{}", COLOR_CYAN, kernel_version, RESET);
        println!("vmlinuz Version: {}{}{}", COLOR_CYAN, vmlinuz_version, RESET);

        println!("Clone Directory: {}{}{}",
                 if clone_dir.is_some() { GREEN } else { RED },
                     clone_dir.unwrap_or_else(|| "Not set".to_string()),
                 RESET);

        println!("Initramfs: {}{}{}",
                 if init_generated { GREEN } else { RED },
                     if init_generated { "Generated" } else { "Not generated" },
                         RESET);

        println!("ISO Name: {}{}{}",
                 if iso_name.is_some() { GREEN } else { RED },
                     iso_name.unwrap_or_else(|| "Not set".to_string()),
                 RESET);
    }

    fn print_usage() {
        println!("Usage: cmi <command> [options]\n");
        println!("Available commands:");
        println!("{}  install-deps            - Install required dependencies", COLOR_CYAN);
        println!("  gen-init                - Generate initramfs");
        println!("  set-iso-name <name>     - Set ISO output name");
        println!("  edit-isolinux           - Edit isolinux configuration");
        println!("  edit-grub               - Edit GRUB configuration");
        println!("  set-clone-dir <path>    - Set directory for system cloning");
        println!("  install-calamares       - Install Calamares installer");
        println!("  edit-branding           - Edit Calamares branding");
        println!("  install-updater         - Install updater");
        println!("  create-squashfs         - Create squashfs from current system");
        println!("  create-squashfs-clone   - Clone system and create squashfs");
        println!("  delete-clone            - Delete cloned system");
        println!("  create-iso <out_dir>    - Create ISO image");
        println!("  run-qemu                - Run QEMU with the ISO");
        println!("  status                  - Show current status");
        println!("  guide                   - Show user guide");
        println!("  changelog               - Show changelog");
        println!("  mainmenu                - Launch main menu (cmitui){}", RESET);
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let distro = CmiUtils::detect_distro();

    if args.len() < 2 {
        CmiUtils::show_status(distro);
        println!();
        CmiUtils::print_usage();
        return;
    }

    let command = &args[1];
    let result = match command.as_str() {
        "install-deps" => CmiUtils::install_dependencies(distro),
        "gen-init" => CmiUtils::generate_initrd(distro),
        "set-iso-name" => {
            if args.len() < 3 {
                eprintln!("Error: ISO name not specified");
                std::process::exit(1);
            }
            CmiUtils::save_iso_name(&args[2])
        },
        "edit-isolinux" => CmiUtils::edit_isolinux_cfg(distro),
        "edit-grub" => CmiUtils::edit_grub_cfg(distro),
        "set-clone-dir" => {
            if args.len() < 3 {
                eprintln!("Error: Clone directory not specified");
                std::process::exit(1);
            }
            CmiUtils::save_clone_dir(&args[2])
        },
        "install-calamares" => CmiUtils::install_calamares(distro),
        "edit-branding" => CmiUtils::edit_calamares_branding(),
        "install-updater" => CmiUtils::install_updater(),
        "create-squashfs" => CmiUtils::create_squashfs(distro),
        "create-squashfs-clone" => CmiUtils::create_squashfs_clone(distro),
        "delete-clone" => CmiUtils::delete_clone(distro),
        "create-iso" => {
            if args.len() < 3 {
                eprintln!("Error: Output directory not specified");
                std::process::exit(1);
            }
            CmiUtils::create_iso(distro, &args[2])
        },
        "run-qemu" => CmiUtils::run_qemu(),
        "status" => {
            CmiUtils::show_status(distro);
            Ok(())
        },
        "guide" => CmiUtils::show_guide(),
        "changelog" => CmiUtils::show_changelog(),
        "mainmenu" => CmiUtils::execute_command("cmitui"),
        _ => {
            CmiUtils::show_status(distro);
            println!();
            CmiUtils::print_usage();
            std::process::exit(1);
        },
    };

    if let Err(e) = result {
        eprintln!("Error: {}", e);
        std::process::exit(1);
    }
}
