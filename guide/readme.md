# 🔧 ClaudeMods Multi ISO Creator 

> *"v2.0 Guide — 22-06-2025"*

> *"This Is For UEFI EXT4 Arch/Ubuntu/Debian Systems Without Separate Swap Or Home"*

*(Edited from guide in [ApexArchIsoCreatorScriptAppImage](https://github.com/claudemods/ApexArchIsoCreatorScriptAppImage))* 

*(I will add this into newer updates as a menu option shortly)*

---

## 📌 Please Follow Guide In Full

### 1️⃣ Setup  
Use one of the setup commands to install a version of my script.

### 2️⃣ Launch  
Launch the menu after installation or type:
- `cmi.bin` for C++ script
- `"distroname"isocreator.bin` for C script in terminal

### 3️⃣ Kernel Preparation  
- Copy `Vmlinuz` and generate `Initramfs` using the Setup Script Menu  
**Note:** If you reboot, you'll need to do this again.

### 4️⃣ ISOLINUX Configuration  
- Edit ISOLINUX configuration using the Setup Scripts Menu.
- Replace `/live/vmlinuz-linux-zen` with your current kernel if not using Zen (e.g., `cachyos`, `hardened`, `linux`, or `lts`)
- Optionally change `archisolabel` from `2025` to your desired label
- Customize boot text and kernel version for visual appeal

### 5️⃣ GRUB Configuration  
- Edit GRUB configuration using the Setup Scripts Menu.
- Replace `/live/vmlinuz-linux-zen` with your actual kernel if needed
- Change `archisolabel` and customize boot text for aesthetics

### 6️⃣ System Cloning  
- Clone your system using the SquashFS Creator Menu
- Make sure all applications are closed before proceeding
- Go to the setup script and select directory to store clone
- Create a SquashFS image using the menu option

### 7️⃣ ISO Creation  
- Use the ISO Creator Menu to generate an ISO of your cloned system
- If you changed the ISO tag (e.g., `2025`), update it accordingly
- Choose a location to save the ISO
- If copying to USB afterward:
  - Wait **4 minutes** after generation to avoid failure
  - Same applies if generating directly to USB

### 8️⃣ Configure Calamares *(C++ and C Arch Only For Now)*  
- From the Setup Scripts Menu, select **Install Calamares**, or  
- Execute: `gen-calamares` if you've installed the custom commands

---

## 🌟 Optional Things To Do

> ⚠️ First Option NOT Integrated Yet

1. **Test Your ISO in Custom QEMU**

2. **Change ISO Boot Artwork**  
   Location: /home/$USER/.config/build-image-distroname/grub and or isolinux/splash.png

3. **Install Custom Commands For Setup Scripts Menu**
