claudemods Multi Iso Creator Guide
Please Follow Guide In Full

1:) use one of the setup commands to install a version of my script

2:) launch menu after install or type cmi.bin for c++ script or "distroname"isocreator.bin for c script into terminal

3:) Copy Vmlinuz And Generate Initramfs "Note If You Reboot You Will Need To This Again"


3:) Edit IsoLinux Configuration
You Will Need to Edit Each Line Which Contains /live/vmlinuz-linux-zen
To Your Current Kernel If Its Not The Default Zen Change To e.g Cachyos, hardened, linux or lts
Optionally Edit archisolabel from 2025 to Whatever And Add Your Own Boot Text and Kernel Version For Eye Candy

4) Edit Grub Configuration
You Will Need to Edit Each Line Which Contains /live/vmlinuz-linux-zen
To Your Current Kernel If Its Not The Default Zen Change To e.g Cachyos, hardened, linux or lts
Optionally Edit archisolabel from 2025 to Whatever And Add Your Own Boot Text For Eye Candy

5:) Clone Your System
Before Your Proceed Make Sure Everything Is Closed
goto setup script and select enter directory to store clone
Clone Your System Into A Squashfs with squashfs creator menu option

6:) Create An Iso Of Your Cloned System
If Your Changed The isotag e.g 2025 in the .cfg Please Set It To What Your Changed It To
Select A Location To Save The Iso To
If Your Going To Copy The Iso To A Usb After Selecting A location To Generate
Please Wait 4 Minutes After Its Copied Otherwise It Might Fail
Do the Same If You Directly Generate To A Usb E.g "Wait 4 Minutes"

Optional Things Todo "NOT Integrated Yet"
1:) Test Your Iso In My Custom Qemu 

