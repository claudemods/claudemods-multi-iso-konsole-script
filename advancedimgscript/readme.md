# 🖥️ Claudemods Ext4/Btrfs Image & ISO Creator v1.0 🛠️

![Banner](https://via.placeholder.com/800x200?text=Claudemods+Image+Creator)

A powerful C++ utility to create compressed system images (ext4/btrfs) and bootable ISOs from your Linux system. 🔥

## 📝 Table of Contents
- [Features](#-features)
- [Requirements](#-requirements)
- [Installation](#-installation)
- [Usage](#-usage)
- [Code Overview](#-code-overview)
- [Full Source Code](#-full-source-code)
- [License](#-license)

## ✨ Features
- 🖼️ Create compressed system images (btrfs for compression and ext4 without)
- � Generate bootable ISOs with custom configurations
- 🔐 Automatic root privilege detection
- 📊 Disk usage reporting
- 🔄 Rsync-based file copying with intelligent exclusions
- 🗜️ SquashFS compression with zstd support
- 🔍 MD5 checksum generation
- 🎨 Colorful terminal output
- 🐧 Arch Based

## 📋 Requirements
- Linux system
- GCC compiler
- Root privileges (for most operations)
- Required packages: `rsync`, `squashfs-tools`, `btrfs-progs`, `xorriso`, `grub`, `dosfstools`

## 🚀 Installation
```bash
# Clone repository
git clone https://github.com/yourusername/claudemods-image-creator.git
cd claudemods-image-creator

# Compile
qmake6 && make

# Install
sudo cp claudemods-image-creator /usr/local/bin/
