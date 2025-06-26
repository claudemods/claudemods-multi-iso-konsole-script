#!/bin/bash

# Function to detect the distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
    else
        echo "Cannot detect the distribution. Exiting."
        exit 1
    fi
}

# Main script starts here
cd /home/$USER >/dev/null 2>&1
mkdir -p /home/$USER/.config/cmi >/dev/null 2>&1

# Detect the distribution
detect_distro

# Conditional logic based on the detected distribution
if [[ "$DISTRO" == "arch" || "$DISTRO" == "cachyos" ]]; then
    # Commands for Arch/CachyOS
    git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git  >/dev/null 2>&1
    cd /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one/updatermain && qmake && make >/dev/null 2>&1
    ./advancedcscriptupdater.bin && rm -rf /home/$USER/claudemods-multi-iso-konsole-script
elif [[ "$DISTRO" == "ubuntu" || "$DISTRO" == "debian" ]]; then
    # Commands for Ubuntu/Debian (same as Arch/CachyOS for now)
    git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git  >/dev/null 2>&1
    cd /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one/updatermain && qmake6 && make >/dev/null 2>&1
    ./advancedcscriptupdater.bin && rm -rf /home/$USER/claudemods-multi-iso-konsole-script
else
    echo "Unsupported distribution: $DISTRO"
    exit 1
fi
