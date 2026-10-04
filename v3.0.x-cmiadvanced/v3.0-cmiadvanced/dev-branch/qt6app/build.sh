#!/bin/bash
# Builds and installs the Qt6 version of cmiadvanced (Arch / CachyOS).
set -e

cd "$(dirname "$0")"

sudo pacman -S --needed --noconfirm qt6-base cmake gcc make unzip

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

sudo install -Dm755 build/cmiadvanced /usr/bin/cmiadvanced
echo "Installed /usr/bin/cmiadvanced"
