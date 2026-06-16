╭──────────────────────────────────────────────────────────────────────────────╮
│                                                                              │
│                  cmiadvanced Beta v3.0 16-06-2026 Guide                    │
│                                                                              │
│            "Create Bootable ISO Images from Your System"                     │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                          🔧 Quick Start Guide                                │
│                                                                              │
│  1️⃣ Compile and Run if you havent installed from the bash command            │
│     Compile the C++ script and run the executable in your terminal           │
│                                                                              │
│  2️⃣ Main Menu Options                                                       │
│     The script provides these main functions:                                │
│     - Create System Image                                                    │
│     - ISO Creation Setup                                                     │
│     - Generate Bootable ISO                                                  │
│     - Check Disk Usage                                                       │
│     - Automatic Mode                                                         │
│                                                                              │
│  3️⃣ First Run Configuration                                                 │
│     The script will automatically:                                           │
│     - Create configuration directory at ~/.config/cmi/                       │
│     - Load any existing settings from configuration.txt                      │
│     - Detect your username and set appropriate paths                         │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                       📝 Step-by-Step Usage Guide                           │
│                                                                              │
│  1️⃣ System Image Creation  (Recommended Option Lzma Level 109)               │
│     - Select "Create Image" from main menu                                   │
│       - Slow Squashfs Options:                                               │
│       • Clone your current system (xz compression)                           │
│       • Clone your current system (zstd compression)                         │
        - Fast Erofs Options:                                                  │
│       • Clone your current system (Lz4hc compression)                        │
│       • Clone your current system (Lzma Max compression)                     │
│                                                                              │
│  2️⃣ ISO Preparation                                                         │
│     Use the "ISO Creation Setup" menu to configure:                          │
│     • Set ISO Tag - Identifier for your ISO (e.g., "2026")                   │
│     • Set ISO Name - Output filename (e.g., "claudemods.iso")                │
│     • Set Output Directory - Where to save ISO                               │
│      (Supports $USER variable, e.g., "/home/$USER/Downloads")                │
│     • Select vmlinuz - Choose kernel from /boot│                             │
│     • mkinitcpio config - may need to be setup                               │
      (11-dm-initramfs.rules file is sometimes not provided by device mapper)  │
│     • Generate mkinitcpio - Create initramfs                                 │
│     • Edit GRUB Config - Customize bootloader settings                       │
│     • Edit Boot Text                                                         │
│                                                                              │
│  3️⃣ Calamares Setup                                                         │
│     - Use the setup scripts menu to edit calamares branding.desc             │
│     - Edit the branding pictures if need be in /usr/share/calamares/branding │
│     - Use the setup scripts menu to edit calamares .confs to your kernel     │
│     - To e.g. linux-zen or linux, or leave as linux-cachyos as it is default │
│                                                                              │
│  4️⃣ ISO Generation                                                          │
│     - Select "Create ISO" from main menu                                     │
│     - The script will:                                                       │
│       • Verify all required settings are configured                          │
│       • Use xorriso to create bootable ISO                                   │
│       • Save to your specified output directory                              │
│                                                                              │
│  5️⃣ Post-Creation                                                           │
│     - Wait 4 minutes if writing directly to USB                              │
│     - Test ISO in virtual machine before deployment                          │
│     - Checksum file (.md5) is generated for verification                     │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                                💡 Pro Tips                                   │
│                                                                              │
│     • Configuration persists between runs in ~/.config/cmi/configuration.txt │
│     • Main menu shows current configuration status                           │
│     • Excludes temporary and system directories automatically                │
│                                                                              │
│                                ⚠️ Important Notes                            │
│                                                                              │
│     • Close all applications before system cloning                           │
│     • If you reboot, you'll need to re-select vmlinuz and Regenerate         │
│     • Large images will take time to process - be patient                    │
│                                                                              │
╰──────────────────────────────────────────────────────────────────────────────╯
