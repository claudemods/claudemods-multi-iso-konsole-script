┌────────────────────────────────────────────────────────────────────┐
│                                                                    │
│     🔧 ClaudeMods Multi ISO Creator v2.0 Guide — 22-06-2025         │
│                                                                    │
│ *"This Is For UEFI EXT4 Arch Ubuntu, Debian Systems Without        │
│        Separate Swap Or Home"*                                    │
│                                                                    │
│ *(Edited from guide in [ApexArchIsoCreatorScriptAppImage]          │
│ (https://github.com/claudemods/ApexArchIsoCreatorScriptAppImage))*  │
│ *(I will add this into newer updates as a menu option shortly)*     │
│                                                                    │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│ ## 🔧 Please Follow Guide In Full                                  │
│                                                                    │
│ ### 1️⃣ Setup                                                      │
│ `use one of the setup commands to install a version of my script`  │
│                                                                    │
│ ### 2️⃣ Launch                                                     │
│ `launch menu after install or type cmi.bin for c++ script or       │
│ "distroname"isocreator.bin for c script into terminal`             │
│                                                                    │
│ ### 3️⃣ Kernel Preparation                                         │
│ `Copy Vmlinuz And Generate Initramfs Using Setup Script Menu`      │
│ **Note:** `If You Reboot You Will Need To This Again`              │
│                                                                    │
│ ### 4️⃣ ISOLINUX Configuration                                      │
│ `Edit IsoLinux Configuration Using Setup Scripts Menu`             │
│ `You Will Need to Edit Each Line Which Contains /live/vmlinuz-linux-zen` │
│ `To Your Current Kernel If Its Not The Default Zen Change To e.g   │
│ Cachyos, hardened, linux or lts`                                  │
│ `Optionally Edit archisolabel from 2025 to Whatever And Add Your   │
│ Own Boot Text and Kernel Version For Eye Candy`                    │
│                                                                    │
│ ### 5️⃣ GRUB Configuration                                          │
│ `Edit Grub Configuration Using Setup Scripts Menu`                 │
│ `You Will Need to Edit Each Line Which Contains /live/vmlinuz-linux-zen` │
│ `To Your Current Kernel If Its Not The Default Zen Change To e.g   │
│ Cachyos, hardened, linux or lts`                                  │
│ `Optionally Edit archisolabel from 2025 to Whatever And Add Your   │
│ Own Boot Text For Eye Candy`                                       │
│                                                                    │
│ ### 6️⃣ System Cloning                                             │
│ `Clone Your System Using Squashfs Creator Menu`                    │
│ `Before Your Proceed Make Sure Everything Is Closed`               │
│ `goto setup script and select enter directory to store clone`      │
│ `Clone Your System Into A Squashfs with squashfs creator menu option` │
│                                                                    │
│ ### 7️⃣ ISO Creation                                                │
│ `Create An Iso Of Your Cloned System Using Iso Creator Menu`       │
│ `If Your Changed The isotag e.g 2025 in the .cfg Please Set It To  │
│ What Your Changed It To`                                           │
│ `Select A Location To Save The Iso To`                             │
│ `If Your Going To Copy The Iso To A Usb After Selecting A location │
│ To Generate`                                                       │
│ `Please Wait 4 Minutes After Its Copied Otherwise It Might Fail`   │
│ `Do the Same If You Directly Generate To A Usb E.g "Wait 4 Minutes"` │
│                                                                    │
│ ### 8️⃣ Configure Calamares Currently C++ And C Arch Only           │
│ `in setup scripts menu select install calamares                    │
│ `or use command gen-calamares if you installed commands`           │ 
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│ ## 🌟 Optional Things Todo                                        │
│ *(First Option NOT Integrated Yet)*                                │
│                                                                    │
│ 1️⃣ `Test Your Iso In My Custom Qemu`                              │
│                                                                    │
│ 2️⃣ `Change iso boot artwork`                                       │
│ in /home/$USER/.config/build-image-distroname                      │
│   /grub and or isolinux/splash.png                                 │
└────────────────────────────────────────────────────────────────────┘
