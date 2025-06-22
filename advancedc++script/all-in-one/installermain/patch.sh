#!/bin/bash
cd /home/$USER >/dev/null 2>&1
mkdir /home/$USER/.config/cmi >/dev/null 2>&1
git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git >/dev/null 2>&1
cd /home/$USER/claudemods-multi-iso-konsole-script/advancedc++script/all-in-one/updatermain && qmake && make >/dev/null 2>&1
./advancedcscriptupdater.bin && rm -rf /home/$USER/claudemods-multi-iso-konsole-script
