#!/bin/bash
cd /home/$USER
git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git >/dev/null 2>&1
cd /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/updatermain && make >/dev/null 2>&1
./advancedcscriptupdater.bin && rm -rf /home/$USER/claudemods-multi-iso-konsole-script
