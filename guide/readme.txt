╭──────────────────────────────────────────────────────────────────────────────╮
│                                                                              │
│            ClaudeMods Multi ISO Creator v2.0 Guide 22-06-2025                │
│                                                                              │
│ "This Is For UEFI EXT4 Arch Ubuntu, Debian Systems Without Separate Swap Or Home" │
│                                                                              │
│ Edited from guide in [ApexArchIsoCreatorScriptAppImage]                      │
│ https://github.com/claudemods/ApexArchIsoCreatorScriptAppImage                │
│ I will add this into newer updates as a menu option shortly                   │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ 🔧 Please Follow Guide In Full                                               │
│                                                                              │
│ 1️⃣ Setup                                                                     │
│ Use one of the setup commands to install a version of my script              │
│                                                                              │
│ 2️⃣ Launch                                                                    │
│ Launch menu after install or type cmi.bin for c++ script or                  │
│ "distroname"isocreator.bin for c script into terminal                        │
│                                                                              │
│ 3️⃣ Kernel Preparation                                                        │
│ Copy Vmlinuz And Generate Initramfs Using Setup Script Menu                  │
│ Note: If You Reboot You Will Need To Do This Again                           │
│                                                                              │
│ 4️⃣ ISOLINUX Configuration                                                    │
│ Edit IsoLinux Configuration Using Setup Scripts Menu                         │
│ You Will Need to Edit Each Line Which Contains /live/vmlinuz-linux-zen       │
│ To Your Current Kernel If It's Not The Default Zen Change To e.g Cachyos,    │
│ hardened, linux or lts                                                       │
│ Optionally Edit archisolabel from 2025 to Whatever And Add Your Own Boot     │
│ Text and Kernel Version For Eye Candy                                        │
│                                                                              │
│ 5️⃣ GRUB Configuration                                                        │
│ Edit Grub Configuration Using Setup Scripts Menu                             │
│ You Will Need to Edit Each Line Which Contains /live/vmlinuz-linux-zen       │
│ To Your Current Kernel If It's Not The Default Zen Change To e.g Cachyos,    │
│ hardened, linux or lts                                                       │
│ Optionally Edit archisolabel from 2025 to Whatever And Add Your Own Boot     │
│ Text For Eye Candy                                                           │
│                                                                              │
│ 6️⃣ System Cloning                                                            │
│ Clone Your System Using Squashfs Creator Menu                                │
│ Before You Proceed Make Sure Everything Is Closed                            │
│ Go to setup script and select enter directory to store clone                 │
│ Clone Your System Into A Squashfs with squashfs creator menu option          │
│                                                                              │
│ 7️⃣ ISO Creation                                                              │
│ Create An Iso Of Your Cloned System Using Iso Creator Menu                   │
│ If You Changed The isotag e.g 2025 in the .cfg Please Set It To What         │
│ You Changed It To                                                            │
│ Select A Location To Save The Iso To                                         │
│ If You Are Going To Copy The Iso To A Usb After Selecting A Location         │
│ To Generate                                                                  │
│ Please Wait 4 Minutes After It's Copied Otherwise It Might Fail              │
│ Do the Same If You Directly Generate To A Usb E.g "Wait 4 Minutes"           │
│                                                                              │
│ 8️⃣ Configure Calamares (C++ and C Arch Only For Now)                         │ 
│    From the Setup Scripts Menu, select Install Calamares, or                 │                                            │     Execute: gen-calamares if you've installed the custom commands           │                                            │                                                                              │ 
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ 🌟 Optional Things To Do                                                     │
│ (First Option NOT Integrated Yet)                                            │
│                                                                              │
│ 1️⃣ Test Your Iso In My Custom Qemu                                           │
│                                                                              │
│ 2️⃣ Change iso boot artwork in                                                │
│ /home/$USER/.config/build-image-distroname/grub and or                       │
│ isolinux/splash.png                                                          │
│ 3️⃣ Install Custom Commands From Setup Scripts Menu                           │
│                                                                              │
╰──────────────────────────────────────────────────────────────────────────────╯
