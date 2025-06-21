#!/bin/bash
cd /home/$USER
mkdir /home/$USER/.config/cmi
git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git >/dev/null 2>&1
cp /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/updatermain/advancedcscriptupdater /home/$USER/.config >/dev/null 2>&1
cp /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/installer/patch.sh /home/$USER/.config >/dev/null 2>&1
chmod +x /home/$USER/.config/cmi/patch.sh >/dev/null 2>&1
chmod +x /home/$USER/.config/cmi/advancedcscript.bin >/dev/null 2>&1
cd /home/$USER/claudemods-multi-iso-konsole-script/advancedcscript/updatermain && make >/dev/null 2>&1
./advancedcscriptupdater.bin && rm -rf /home/$USER/claudemods-multi-iso-konsole-script
