ClaudeMods Multi ISO Creator Ruby, Perl and Cpp Script Beta 1.05 01-11-2024
Sailing the 7 seas like Penguin's Eggs Remastersys, Refracta, Systemback and father Knoppix!


changelog v1.05 01-11-2024
main menu colours changed
fixed demo squashfs files for all distributions
all squashfs methods and colours changed
alot changed in the configuration menu
5 second timers on creation scripts when they finish



known issues
the iso creator will not output the generation
you will need to wait for script to say its finished



guide below please read carefully updated 01-11-2024
only 3 isos can be created before this tool will lock itself!
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>


please install ruby and perl
then place claudemods-iso-konsole-script into /opt
run the konsole.sh in /opt/claudemods-iso-konsole-script


setup steps and guide to cloning your system into a installable iso:
1: use the iso configurator to install install Dependencies depending on distribution

2 with arch it will guide you to setting up calamares for installation
you will need to manually copy /Supported-Distros/Arch/calamares-files/etc
to /

3: optional Dependencies are virt-manager and gnome boxes

4: generate initramfs or initrd and copy your kernel

5: edit syslinux and grub.cfg and or boot artwork

6: you will need to setup calamares yourself for your distro this process can be troublesome
   but like i said above i provide for arch only their are some extra files for ubuntu
   but they may not work its upto you to test them at this current time
   though i will do my own tests sooner or later it
   debian is easy you can install with sudo apt install calamares calamares-settings
   then edit the /etc/calamares/modules/unpackfs.conf to your needs
   also you will need to edit /etc/calamares/settings.conf to remove users
   by adding # to the user attibute e.g #user their is 2 to change

7: make sure everything is ready and things are closed before doing next step

8: create a squashfs and the create an iso

isos will be created in the Supported-Distros "your distro" folder
you may copy you iso to usb after it is generated please wait 2-5 mins after its copied before testing
you make create upto 3 squashfs files before the demo will lock its self
the paid version will soon be available on my patreon im still creating it

9 test your isos with the qemu mechanism i provide in the script
  you can also edit the /claudemods-iso-konsole-script/Supported-Distros/qemu.rb


show appreciation and support me
with my paypal link >>> https://www.paypal.com/paypalme/claudemods?country.x=GB&locale

if you can fork my code from the binary files i provide here is my notice!!!!
Copyright <2024> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
