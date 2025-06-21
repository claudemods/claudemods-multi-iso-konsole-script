#!/bin/bash
cd /home/$USER
git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git
cd /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/updatermain && make
./advancedcscriptupdater.bin && rm -rf /home/$USER/claudemods-multi-iso-konsole-script
