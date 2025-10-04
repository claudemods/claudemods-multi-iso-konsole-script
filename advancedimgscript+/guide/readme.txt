╭──────────────────────────────────────────────────────────────────────────────╮
│ │
│ ClaudeMods Img ISO Creator+ v2.03 Guide │
│ │
│ "Create Bootable ISO Images from Your EXT4/BTRFS System Without │
│ Separate Partitions" │
│ │
├──────────────────────────────────────────────────────────────────────────────┤
│ │
│ 🔧 Quick Start Guide │
│ │
│ 1️⃣ Compile and Run │
│ Compile the C++ script and run the executable in your terminal │
│ │
│ 2️⃣ Main Menu Options │
│ The script provides these main functions: │
│ - Create System Image (EXT4) │
│ - ISO Creation Setup │
│ - Generate Bootable ISO │
│ - Check Disk Usage │
│ │
│ 3️⃣ First Run Configuration │
│ The script will automatically: │
│ - Create configuration directory at ~/.config/cmi/ │
│ - Load any existing settings from configuration.txt │
│ - Detect your username and set appropriate paths │
│ │
├──────────────────────────────────────────────────────────────────────────────┤
│ │
│ 📝 Step-by-Step Usage Guide │
│ │
│ 1️⃣ System Image Creation │
│ - Select "Create Image" from main menu ││
│ - The script can: │
│ • clone your current system │
│ • clone your secondary system │
│ • clone a file or folder of choice │
│ • Compress into SquashFS format │
│ • Generate MD5 checksum │
│ │
│ 2️⃣ ISO Preparation │
│ Use the "ISO Creation Setup" menu to configure: │
│ │
│ • Set ISO Tag - Identifier for your ISO (e.g., "2025") │
│ • Set ISO Name - Output filename (e.g., "claudemods.iso") │
│ • Set Output Directory - Where to save ISO │
│ (Supports $USER variable, e.g., "/home/$USER/Downloads") │
│ • Select vmlinuz - Choose kernel from /boot │
│ • Generate mkinitcpio - Create initramfs │
│ • Edit GRUB Config - Customize bootloader settings │
│ • Edit Boot Text │
│ │
│ 3️⃣ Calamares Setup │
│ - use the setup scripts menu to edit calamres branding.desc │
│ - edit the branding pictures if need be in /usr/share/calamares/branding │
│ - use the setup scripts menu to edit calamres .confs to you kernel │
│ - to e.g linux-zen linux or leave as linux-cachyos as it is default │
│ │
│ 4️⃣ ISO Generation │
│ - Select "Create ISO" from main menu │
│ - The script will: │
│ • Verify all required settings are configured │
│ • Use xorriso to create bootable ISO │
│ • Save to your specified output directory │
│ │
│ 5️⃣ Post-Creation │
│ - Wait 4 minutes if writing directly to USB │
│ - Test ISO in virtual machine before deployment │
│ - Checksum file (.md5) is generated for verification │
│ │
├──────────────────────────────────────────────────────────────────────────────┤
│ │
│ 💡 Pro Tips │
│ │
│ • Configuration persists between runs in ~/.config/cmi/configuration.txt│
│ • Main menu shows current configuration status │
│ • Excludes temporary and system directories automatically │
│ │
│ ⚠️ Important Notes │
│ │
│ • Close all applications before system cloning │
│ • If you reboot, you'll need to re-select vmlinuz and Regenerate │
│ • Large images will take time to process - be patient │
│ │
╰──────────────────────────────────────────────────────────────────────────────╯

