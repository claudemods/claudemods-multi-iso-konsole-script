# 🚀 cmiadvanced Beta v3.0 16-06-2026 Guide

> 💿 **"Create Bootable ISO Images from Your System"**

---

## 🔧 Quick Start Guide

### 1️⃣ Compile and Run
*If you haven't installed from the bash command*

📦 Compile the C++ script and run the executable in your terminal

---

### 2️⃣ Main Menu Options
The script provides these main functions:

| Function | Description |
|----------|-------------|
| ⚙️ **ISO Creation Setup** | Setup Needed Things |
| 🖥️ **Create System Image** | Clone your current system |
| 💿 **Generate Bootable ISO** | Build the final ISO |
| 📊 **Check Disk Usage** | View disk space information |

---

### 3️⃣ First Run Configuration
The script will automatically:

- 📁 Create configuration directory at `~/.config/cmi/`
- 📄 Load any existing settings from `configuration.txt`
- 👤 Detect your username and set appropriate paths

---

## 📝 Step-by-Step Usage Guide

### 1️⃣ System Image Creation 🖥️
*Recommended Option: Lzma Level 109*

Select **"Create Image"** from main menu

#### 🐢 Slow Squashfs Options:
- 🔹 Clone your current system (xz compression)
- 🔹 Clone your current system (zstd compression)

#### ⚡ Fast Erofs Options:
- 🔸 Clone your current system (Lz4hc compression)
- 🔸 Clone your current system (Lzma Max compression)

---

### 2️⃣ ISO Preparation ⚙️
Use the **"ISO Creation Setup"** menu to configure:

| Setting | Description | Example |
|---------|-------------|---------|
| 🏷️ **Set ISO Tag** | Identifier for your ISO | `"2026"` |
| 📛 **Set ISO Name** | Output filename | `"claudemods.iso"` |
| 📂 **Set Output Directory** | Where to save ISO | `"/home/$USER/Downloads"` |
| 🔩 **Select vmlinuz** | Choose kernel from `/boot` | - |
| 🛠️ **mkinitcpio config** | May need to be setup | ⚠️ `11-dm-initramfs.rules` file sometimes not provided by device mapper |
| 🧬 **Generate mkinitcpio** | Create initramfs | - |
| 📝 **Edit GRUB Config** | Customize bootloader settings | - |
| ✏️ **Edit Boot Text** | Modify boot messages | - |

> 💡 Supports `$USER` variable for paths

---

### 3️⃣ Calamares Setup 🎨

- 🖼️ Use the setup scripts menu to edit calamares `branding.desc`
- 📸 Edit branding pictures if needed in `/usr/share/calamares/branding`
- 🔧 Use the setup scripts menu to edit calamares `.confs` to your kernel
- 🐧 Examples: `linux-zen`, `linux`, or leave as `linux-cachyos` (default)

---

### 4️⃣ ISO Generation 💿

Select **"Create ISO"** from main menu

The script will:

- ✅ Verify all required settings are configured
- 💿 Use `xorriso` to create bootable ISO
- 📁 Save to your specified output directory

---

### 5️⃣ Post-Creation 🎉

- ⏱️ **Wait 4 minutes** if writing directly to USB
- 🧪 **Test ISO** in virtual machine before deployment
- 🔐 **Checksum file** (`.md5`) is generated for verification

---

## 💡 Pro Tips

- 💾 Configuration persists between runs in `~/.config/cmi/configuration.txt`
- 📊 Main menu shows current configuration status
- 🚫 Excludes temporary and system directories automatically

---

## ⚠️ Important Notes

- 🔒 **Close all applications** before system cloning
- 🔄 If you reboot, you'll need to re-select `vmlinuz` and regenerate
- ⏳ Large images will take time to process - **be patient**

---
