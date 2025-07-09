#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <dirent.h>
using namespace std;

#define COLOR_CYAN "\033[38;2;0;255;255m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET "\033[0m"

string exec(const char* cmd) {
    char buffer[128];
    string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw runtime_error("popen() failed!");
    while (fgets(buffer, sizeof buffer, pipe) != NULL) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

void execute_command(const string& cmd) {
    cout << COLOR_CYAN;
    fflush(stdout);
    string full_cmd = "sudo " + cmd;
    int status = system(full_cmd.c_str());
    cout << COLOR_RESET;
    if (status != 0) {
        cerr << COLOR_RED << "Error executing: " << full_cmd << COLOR_RESET << endl;
        exit(1);
    }
}

bool is_block_device(const string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) != 0) return false;
    return S_ISBLK(statbuf.st_mode);
}

bool directory_exists(const string& path) {
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) != 0) return false;
    return S_ISDIR(statbuf.st_mode);
}

string get_uk_date_time() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%d-%m-%Y_%H:%M", ltm);
    string time_str(buffer);
    int hour = ltm->tm_hour;
    string ampm = (hour < 12) ? "am" : "pm";
    if (hour > 12) hour -= 12;
    if (hour == 0) hour = 12;
    char time_buffer[80];
    strftime(time_buffer, sizeof(time_buffer), "%d-%m-%Y_%I:%M", ltm);
    string result(time_buffer);
    result += ampm;
    return result;
}

void display_header() {
    cout << COLOR_RED;
    cout << R"(
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
)" << endl;
cout << COLOR_CYAN << "claudemods Apex CKGE Full Stable Installer v1.04.3" << COLOR_RESET << endl;
cout << COLOR_CYAN << "Supports Btrfs (with Zstd compression) and Ext4 filesystems" << COLOR_RESET << endl << endl;
}

void prepare_target_partitions(const string& drive, const string& fs_type) {
    execute_command("umount -f " + drive + "* 2>/dev/null || true");
    execute_command("wipefs -a " + drive);
    execute_command("parted -s " + drive + " mklabel gpt");
    execute_command("parted -s " + drive + " mkpart primary fat32 1MiB 551MiB");
    execute_command("parted -s " + drive + " mkpart primary " + fs_type + " 551MiB 100%");
    execute_command("parted -s " + drive + " set 1 esp on");
    execute_command("partprobe " + drive);
    sleep(2);

    string efi_part = drive + "1";
    string root_part = drive + "2";

    if (!is_block_device(efi_part) || !is_block_device(root_part)) {
        cerr << COLOR_RED << "Error: Failed to create partitions" << COLOR_RESET << endl;
        exit(1);
    }

    execute_command("mkfs.vfat -F32 " + efi_part);
    if (fs_type == "btrfs") {
        execute_command("mkfs.btrfs -f -L ROOT " + root_part);
    } else {
        execute_command("mkfs.ext4 -F -L ROOT " + root_part);
    }
}

void setup_btrfs_subvolumes(const string& root_part) {
    // Mount root partition temporarily to create subvolumes
    execute_command("mount " + root_part + " /mnt");
    execute_command("btrfs subvolume create /mnt/@");
    execute_command("btrfs subvolume create /mnt/@home");
    execute_command("btrfs subvolume create /mnt/@root");
    execute_command("btrfs subvolume create /mnt/@srv");
    execute_command("btrfs subvolume create /mnt/@cache");
    execute_command("btrfs subvolume create /mnt/@tmp");
    execute_command("btrfs subvolume create /mnt/@log");
    execute_command("mkdir -p /mnt/@/var/lib");
    execute_command("btrfs subvolume create /mnt/@/var/lib/portables");
    execute_command("btrfs subvolume create /mnt/@/var/lib/machines");

    // Unmount temporary mount
    execute_command("umount /mnt");

    // Mount all subvolumes with Zstd compression (level 22)
    execute_command("mount -o subvol=@,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt");
    execute_command("mkdir -p /mnt/{home,root,srv,tmp,var/{cache,log},var/lib/{portables,machines},boot/efi}");

    // Mount other subvolumes with same compression settings
    execute_command("mount -o subvol=@home,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/home");
    execute_command("mount -o subvol=@root,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/root");
    execute_command("mount -o subvol=@srv,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/srv");
    execute_command("mount -o subvol=@cache,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/var/cache");
    execute_command("mount -o subvol=@tmp,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/tmp");
    execute_command("mount -o subvol=@log,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/var/log");
    execute_command("mount -o subvol=@/var/lib/portables,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/var/lib/portables");
    execute_command("mount -o subvol=@/var/lib/machines,compress=zstd:22,compress-force=zstd:22 " + root_part + " /mnt/var/lib/machines");

    string efi_part = root_part.substr(0, root_part.length()-1) + "1";
    execute_command("mount " + efi_part + " /mnt/boot/efi");
    execute_command("mkdir -p /mnt/{proc,sys,dev,run,tmp}");
    execute_command("sudo mkdir /mnt/opt");
    execute_command("cp btrfsfstabcompressed.sh /mnt/opt");
    execute_command("chmod +x /mnt/opt/btrfsfstabcompressed.sh");
}

void setup_ext4_filesystem(const string& root_part) {
    execute_command("mount " + root_part + " /mnt");
    execute_command("mkdir -p /mnt/boot/efi");
    string efi_part = root_part.substr(0, root_part.length()-1) + "1";
    execute_command("mount " + efi_part + " /mnt/boot/efi");
    execute_command("mkdir -p /mnt/{proc,sys,dev,run,tmp}");
}

void install_grub_ext4(const string& drive) {
    string root_part = drive + "1";
    execute_command("dd if=/opt/ext4boot.img of=" + root_part + " bs=4M status=progress && sync ");
    execute_command("unzip -oq /opt/boot.zip -d /mnt ");
    execute_command("curl -L -o /mnt/apexfullhome.squashfs https://github.com/claudemods/ApexCKGE/releases/download/CKGE-V1.04.3-base/apexfullhome.squashfs ");
    execute_command("unsquashfs -d -f /mnt/apexfullhome.squashfs /mnt/home ");
    execute_command("rm -rf /mnt/apexfullhome.squashfs ");
    execute_command("pacstrap /mnt a52dec aalib abseil-cpp accounts-qml-module accountsservice acl adobe-source-han-sans-cn-fonts adobe-source-han-sans-jp-fonts adobe-source-han-sans-kr-fonts adwaita-cursors adwaita-fonts adwaita-icon-theme adwaita-icon-theme-legacy aha alacritty alsa-card-profiles alsa-firmware alsa-lib alsa-plugins alsa-topology-conf alsa-ucm-conf alsa-utils amd-ucode ananicy-cpp android-tools android-udev aom appstream appstream-glib appstream-qt archlinux-appstream-data archlinux-keyring ark at-spi2-core atkmm attica attr audit aurorae autoconf automake avahi awesome-terminal-fonts babl baloo baloo-widgets base base-devel bash bash-completion bat bind binutils bison blas bluedevil bluez bluez-hid2hci bluez-libs bluez-qt bluez-utils bolt boost-libs bpf brave-bin breeze breeze-gtk breeze-icons brotli btop btrfs-assistant btrfs-progs bubblewrap bzip2 ca-certificates ca-certificates-mozilla ca-certificates-utils cachyos-alacritty-config cachyos-ananicy-rules cachyos-emerald-kde-theme-git cachyos-fish-config cachyos-hello cachyos-hooks cachyos-iridescent-kde cachyos-kde-settings cachyos-kernel-manager cachyos-keyring cachyos-micro-settings cachyos-mirrorlist cachyos-nord-kde-theme-git cachyos-packageinstaller cachyos-plymouth-theme cachyos-rate-mirrors cachyos-settings cachyos-themes-sddm cachyos-v3-mirrorlist cachyos-v4-mirrorlist cachyos-wallpapers cachyos-zsh-config cairo cairomm cairomm-1.16 cantarell-fonts capitaine-cursors capstone cblas cdparanoia cdrtools cfitsio char-white chromaprint chwd cifs-utils clang clinfo cmake compiler-rt composefs containerd convertlit coreutils cowsql cpio cppdap cpupower cryptsetup curl dav1d db5.3 dbus dbus-broker dbus-broker-units dbus-glib dbus-units dconf ddcutil debugedit default-cursors desktop-file-utils device-mapper dhclient diffutils ding-libs discount discover dmidecode dmraid dnsmasq dnssec-anchors docker dolphin dosfstools double-conversion dtc duf duktape e2fsprogs ebook-tools editorconfig-core-c edk2-ovmf efibootmgr efitools efivar egl-wayland eglexternalplatform ell enchant ethtool exfatprogs exiv2 expac expat eza f2fs-tools faac faad2 fakeroot fastfetch fd ffmpeg ffmpegthumbnailer ffmpegthumbs fftw file filelight filesystem findutils fish fish-autopair fish-pure-prompt fisher flac flatpak flex fluidsynth fmt fontconfig frameworkintegration freeglut freetype2 fribidi fsarchiver fuse-common fuse2 fuse3 fzf gawk gc gcc gcc-libs gdbm gdk-pixbuf2 gegl gettext gfxstream ghostscript giflib gimp git glew glib-networking glib2 glib2-devel glibc glibmm glibmm-2.68 glm glslang glu gmp gnome-boxes gnu-free-fonts gnulib-l10n gnupg gnutls gobject-introspection-runtime gparted gperftools gpgme gpgmepp gpm graphene graphicsmagick graphite grep groff grub grub-hook gsettings-desktop-schemas gsettings-system-schemas gsl gsm gspell gssdp gssproxy gst-libav gst-plugin-pipewire gst-plugins-bad gst-plugins-bad-libs gst-plugins-base gst-plugins-base-libs gst-plugins-good gst-plugins-ugly gstreamer gtest gtk-update-icon-cache gtk-vnc gtk3 gtk4 gtkmm-4.0 gtkmm3 gtksourceview4 guile gupnp gupnp-igd gwenview gzip harfbuzz harfbuzz-icu haruna haveged hdparm hicolor-icon-theme hidapi highway hunspell hwdata hwdetect hwinfo hwloc hyphen i2c-tools iana-etc icu ijs imagemagick imath imlib2 incus inetutils iniparser inkscape inxi iproute2 iptables-nft iputils iso-codes iw iwd jack2 jansson jasper jbig2dec jbigkit jemalloc jfsutils jq json-c json-glib jsoncpp kaccounts-integration kactivitymanagerd karchive karchive5 kate kauth kauth5 kbd kbookmarks kbookmarks5 kcalc kcmutils kcodecs kcodecs5 kcolorpicker kcolorscheme kcompletion kcompletion5 kconfig kconfig5 kconfigwidgets kconfigwidgets5 kcontacts kcoreaddons kcoreaddons5 kcrash kcrash5 kdbusaddons kdbusaddons5 kde-cli-tools kde-gtk-config kdeclarative kdeconnect kdecoration kded kded5 kdegraphics-mobipocket kdegraphics-thumbnailers kdeplasma-addons kdesu kdialog kdnssd kdsingleapplication kdsoap-qt6 kdsoap-ws-discovery-client keyutils kfilemetadata kglobalaccel kglobalaccel5 kglobalacceld kguiaddons kguiaddons5 kholidays ki18n ki18n5 kiconthemes kiconthemes5 kidletime kimageannotator kinfocenter kinit kio kio-admin kio-extras kio-fuse kio5 kirigami kirigami-addons kitemmodels kitemviews kitemviews5 kjobwidgets kjobwidgets5 kmenuedit kmod knewstuff knotifications knotifications5 knotifyconfig konsole kpackage kparts kpeople kpipewire kpty kquickcharts krb5 krunner kscreen kscreenlocker kservice kservice5 kstatusnotifieritem ksvg ksystemlog ksystemstats ktexteditor ktextwidgets ktextwidgets5 kunitconversion kuserfeedback  kwayland kwidgetsaddons kwidgetsaddons5 kwin kwindowsystem kwindowsystem5 kxmlgui kxmlgui5 l-smash lame lapack layer-shell-qt layer-shell-qt5 lcms2 ldb leancrypto lensfun less lib2geom lib32-alsa-lib lib32-alsa-plugins lib32-audit lib32-brotli lib32-bzip2 lib32-curl lib32-dbus lib32-e2fsprogs lib32-expat lib32-fontconfig lib32-freetype2 lib32-gcc-libs lib32-glib2 lib32-glibc lib32-harfbuzz lib32-icu lib32-json-c lib32-keyutils lib32-krb5 lib32-libcap lib32-libdrm lib32-libelf lib32-libffi lib32-libgcrypt lib32-libglvnd lib32-libgpg-error lib32-libidn2 lib32-libldap lib32-libnghttp2 lib32-libnghttp3 lib32-libnm lib32-libnsl lib32-libpciaccess lib32-libpipewire lib32-libpng lib32-libpsl lib32-libssh2 lib32-libtasn1 lib32-libtirpc lib32-libunistring lib32-libva lib32-libx11 lib32-libxau lib32-libxcb lib32-libxcrypt lib32-libxcrypt-compat lib32-libxdamage lib32-libxdmcp lib32-libxext lib32-libxfixes lib32-libxinerama lib32-libxml2 lib32-libxshmfence lib32-libxss lib32-libxxf86vm lib32-llvm-libs lib32-lm_sensors lib32-mesa-git lib32-ncurses lib32-nspr lib32-nss lib32-openssl lib32-p11-kit lib32-pam lib32-pcre2 lib32-pipewire lib32-spirv-tools lib32-sqlite lib32-systemd lib32-util-linux lib32-vulkan-icd-loader lib32-wayland lib32-xz lib32-zlib-ng lib32-zlib-ng-compat lib32-zstd libaccounts-glib libaccounts-qt libadwaita libaemu libaio libappimage libarchive libass libassuan libasyncns libatasmart libavc1394 libavif libavtp libb2 libblockdev libblockdev-crypto libblockdev-fs libblockdev-loop libblockdev-mdraid libblockdev-nvme libblockdev-part libblockdev-swap libbluray libbpf libbs2b libbsd libburn libbytesize libcaca libcacard libcanberra libcap libcap-ng libcbor libcdio libcdio-paranoia libcdr libcloudproviders libcolord libcups libdaemon libdatachannel libdatrie libdbusmenu-qt5 libdc1394 libdca libde265 libdecor libdeflate libdisplay-info libdmtx libdnet libdovi libdrm libdv libdvdcss libdvdnav libdvdread libebur128 libedit libei libelf libepoxy libevdev libevent libfakekey libfdk-aac libffi libfontenc libfreeaptx libgbinder libgcrypt libgexiv2 libgirepository libgit2 libglibutil libglvnd libgme libgpg-error libgsf libgudev libhandy libheif libice libid3tag libidn libidn2 libiec61883 libimagequant libimobiledevice libimobiledevice-glue libinih libinput libinstpatch libisl libisoburn libisofs libjpeg-turbo libjuice libjxl libkdcraw libkexiv2 libksba libkscreen libksysguard liblc3 libldac libldap liblqr liblrdf libltc libmalcontent libmanette libmaxminddb libmbim libmd libmicrodns libmm-glib libmng libmnl libmodplug libmpc libmpcdec libmpeg2 libmspack libmtp libmypaint libmysofa libnbd libndp libnetfilter_conntrack libnewt libnfnetlink libnfs libnftnl libnghttp2 libnghttp3 libnice libnih libnl libnm libnotify libnsl libnvme libogg libopenmpt libopenraw libosinfo libp11-kit libpaper libpcap libpciaccess libpgm libpipeline libpipewire libplacebo libplasma libplist libpng libportal libportal-gtk3 libproxy libpsl libpulse libqaccessibilityclient-qt6 libqalculate libqmi libqrtr-glib libraqm libratbag libraw libraw1394 librevenge librsvg libsamplerate libsasl libseccomp libsecret libshout libsigc++ libsigc++-3.0 libsixel libslirp libsm libsndfile libsodium libsoup3 libsoxr libspiro libsrtp libssh libssh2 libstemmer libsysprof-capture libtasn1 libteam libthai libtheora libtiff libtirpc libtommath libtool libtraceevent libtracefs libunibreak libunistring libunwind liburcu liburing libusb libusbmuxd libutempter libuv libva libvdpau libverto libvirt libvirt-glib libvirt-python libvisio libvorbis libvpl libvpx libwacom libwbclient libwebp libwireplumber libwmf libwnck3 libwpd libwpg libx11 libx86emu libxau libxaw libxcb libxcomposite libxcrypt libxcrypt-compat libxcursor libxcvt libxdamage libxdmcp libxdp libxext libxfixes libxfont2 libxft libxi libxinerama libxkbcommon libxkbcommon-x11 libxkbfile libxml2 libxmlb libxmu libxpm libxpresent libxrandr libxrender libxres libxshmfence libxslt libxss libxt libxtst libxv libxvmc libxxf86vm libyaml libyuv libzip licenses lilv linux-api-headers linux-cachyos linux-cachyos-headers linux-firmware linux-firmware-amdgpu linux-firmware-atheros linux-firmware-broadcom linux-firmware-cirrus linux-firmware-intel linux-firmware-mediatek linux-firmware-nvidia linux-firmware-other linux-firmware-radeon linux-firmware-realtek linux-firmware-whence lld llhttp llvm llvm-libs lm_sensors lmdb logrotate lsb-release lsof lsscsi lua luajit lv2 lvm2 lxc lxcfs lz4 lzo m4 mailcap make man-db man-pages mbedtls md4c mdadm media-player-info meld mesa mesa-utils micro milou miniupnpc minizip mjpegtools mkinitcpio mkinitcpio-busybox mobile-broadband-provider-info modemmanager modemmanager-qt mpdecimal mpfr mpg123 mpv mpvqt mtdev mtools mujs mypaint-brushes1 nano nano-syntax-highlighting ncurses ndctl neon netctl nettle networkmanager networkmanager-openvpn networkmanager-qt nfs-utils nfsidmap nftables nilfs-utils noto-color-emoji-fontconfig noto-fonts noto-fonts-cjk noto-fonts-emoji npth nspr nss nss-mdns ntp numactl obs-studio ocean-sound-theme ocl-icd octopi oh-my-zsh-git onetbb oniguruma open-vm-tools openal opencore-amr opencv opendesktop-fonts openexr openh264 openjpeg2 openssh openssl openvpn openxr opus orc os-prober osinfo-db ostree p11-kit pacman pacman-contrib pacman-mirrorlist pacutils pahole pam pambase pango pangomm pangomm-2.48 parallel parted paru patch pavucontrol pciutils pcre pcre2 pcsclite perl perl-clone perl-encode-locale perl-error perl-file-listing perl-html-parser perl-html-tagset perl-http-cookiejar perl-http-cookies perl-http-daemon perl-http-date perl-http-message perl-http-negotiate perl-io-html perl-libwww perl-lwp-mediatypes perl-mailtools perl-net-http perl-timedate perl-try-tiny perl-uri perl-www-robotrules perl-xml-parser perl-xml-writer phodav phonon-qt6 phonon-qt6-mpv pinentry piper pipewire pipewire-alsa pipewire-audio pipewire-pulse pixman pkcs11-helper pkgconf pkgfile plasma-activities plasma-activities-stats plasma-browser-integration plasma-desktop plasma-firewall plasma-integration plasma-nm plasma-pa plasma-systemmonitor plasma-thunderbolt plasma-workspace plasma5support plocate plymouth plymouth-kcm polkit polkit-kde-agent polkit-qt5 polkit-qt6 poppler poppler-data poppler-glib poppler-qt6 popt portaudio potrace power-profiles-daemon powerdevil powerline-fonts ppp ppsspp ppsspp-assets prison procps-ng protobuf psmisc pulseaudio-qt purpose pv python python-annotated-types python-appdirs python-babel python-beautifulsoup4 python-cachecontrol python-cairo python-certifi python-chardet python-charset-normalizer python-coverage python-cssselect python-dbus python-defusedxml python-docutils python-evdev python-filelock python-gbinder python-gobject python-idna python-imagesize python-jinja python-lockfile python-lxml python-markupsafe python-msgpack python-numpy python-orjson python-packaging python-pillow python-psutil python-pydantic python-pydantic-core python-pygments python-pyserial python-pytz python-requests python-roman-numerals-py python-six python-snowballstemmer python-soupsieve python-sphinx python-sphinx-alabaster-theme python-sphinxcontrib-applehelp python-sphinxcontrib-devhelp python-sphinxcontrib-htmlhelp python-sphinxcontrib-jsmath python-sphinxcontrib-qthelp python-sphinxcontrib-serializinghtml python-tinycss2 python-typing-inspection python-typing_extensions python-urllib3 python-webencodings python-zstandard qca-qt5 qca-qt6 qcoro qemu-audio-alsa qemu-audio-dbus qemu-audio-jack qemu-audio-oss qemu-audio-pa qemu-audio-pipewire qemu-audio-sdl qemu-audio-spice qemu-base qemu-block-curl qemu-block-dmg qemu-block-nfs qemu-block-ssh qemu-chardev-spice qemu-common qemu-desktop qemu-guest-agent qemu-hw-display-qxl qemu-hw-display-virtio-gpu qemu-hw-display-virtio-gpu-gl qemu-hw-display-virtio-gpu-pci qemu-hw-display-virtio-gpu-pci-gl qemu-hw-display-virtio-gpu-pci-rutabaga qemu-hw-display-virtio-gpu-rutabaga qemu-hw-display-virtio-vga qemu-hw-display-virtio-vga-gl qemu-hw-display-virtio-vga-rutabaga qemu-hw-uefi-vars qemu-hw-usb-host qemu-hw-usb-redirect qemu-hw-usb-smartcard qemu-img qemu-system-x86 qemu-system-x86-firmware qemu-ui-curses qemu-ui-dbus qemu-ui-egl-headless qemu-ui-gtk qemu-ui-opengl qemu-ui-sdl qemu-ui-spice-app qemu-ui-spice-core qemu-vhost-user-gpu qqc2-breeze-style qqc2-desktop-style qrencode qt-sudo qt5-base qt5-declarative qt5-multimedia qt5-speech qt5-svg qt5-translations qt5-wayland qt5-x11extras qt6-5compat qt6-base qt6-connectivity qt6-declarative qt6-imageformats qt6-location qt6-multimedia qt6-multimedia-ffmpeg qt6-positioning qt6-quick3d qt6-quicktimeline qt6-sensors qt6-shadertools qt6-speech qt6-svg qt6-tools qt6-translations qt6-virtualkeyboard qt6-wayland qt6-webchannel qt6-webengine qt6-websockets qt6-webview qtermwidget raft ragel raptor rate-mirrors rav1e re2 readline rebuild-detector reflector rhash ripgrep ripgrep-all rnnoise rpcbind rsync rtkit rtmpdump rubberband run-parts runc rutabaga-ffi s-nail sbc scour scrcpy scx-manager scx-scheds sd sddm sddm-kcm sdl2-compat sdl2_image sdl2_ttf sdl3 seabios sed serd sg3_utils shaderc shadow shared-mime-info signon-kwallet-extension signon-plugin-oauth2 signon-ui signond slang smartmontools smbclient snapper snappy socat sof-firmware solid solid5 sonnet sonnet5 sord sound-theme-freedesktop soundtouch sox spandsp spdlog spectacle speex speexdsp spice spice-gtk spice-protocol spice-vdagent spirv-tools sqlite squashfs-tools squashfuse sratom srt startup-notification steam sudo suitesparse svt-av1 svt-hevc syndication syntax-highlighting sysfsutils systemd systemd-libs systemd-resolvconf systemd-sysvcompat systemsettings taglib talloc tar tcl tdb tealdeer tevent texinfo thin-provisioning-tools tinysparql tpm2-tss tslib ttf-bitstream-vera ttf-dejavu ttf-fantasque-nerd ttf-fira-sans ttf-hack ttf-liberation ttf-meslo-nerd ttf-opensans twolame tzdata uchardet udisks2 ufw unrar unzip upower uriparser usb_modeswitch usbredir usbutils uthash util-linux util-linux-libs v4l-utils vapoursynth vde2 verdict vi vid.stab vim vim-runtime virglrenderer virt-install virt-manager virtiofsd virtualbox-guest-utils vmaf volume_key vte-common vte3 vulkan-icd-loader vulkan-tools vulkan-virtio wavpack waydroid wayland wayland-utils webkit2gtk-4.1 webrtc-audio-processing-1 wget which wildmidi wireless-regdb wireplumber woff2 wolfssl wpa_supplicant x264 x265 xcb-proto xcb-util xcb-util-cursor xcb-util-image xcb-util-keysyms xcb-util-renderutil xcb-util-wm xdg-dbus-proxy xdg-desktop-portal xdg-desktop-portal-kde xdg-user-dirs xdg-utils xf86-input-elographics xf86-input-evdev xf86-input-libinput xf86-input-synaptics xf86-input-vmmouse xf86-input-void xf86-input-wacom xf86-video-amdgpu xf86-video-ati xf86-video-dummy xf86-video-fbdev xf86-video-intel xf86-video-nouveau xf86-video-sisusb xf86-video-vesa xf86-video-vmware xf86-video-voodoo xfsprogs xkeyboard-config xl2tpd xmlsec xorg-fonts-encodings xorg-server xorg-server-common xorg-setxkbmap xorg-xauth xorg-xdpyinfo xorg-xinit xorg-xinput xorg-xkbcomp xorg-xkill xorg-xmessage xorg-xmodmap xorg-xprop xorg-xrandr xorg-xrdb xorg-xset xorg-xwayland xorgproto xsettingsd xvidcore xxhash xz yyjson zbar zenity zeromq zimg zix zlib-ng zlib-ng-compat zram-generator zsh zsh-autosuggestions zsh-completions zsh-history-substring-search zsh-syntax-highlighting zsh-theme-powerlevel10k zstd zvbi zxing-cpp");
    execute_command("cp /opt/sudoers /etc ");
    execute_command("mount --bind /dev /mnt/dev");
    execute_command("mount --bind /dev/pts /mnt/dev/pts");
    execute_command("mount --bind /proc /mnt/proc");
    execute_command("mount --bind /sys /mnt/sys");
    execute_command("mount --bind /run /mnt/run");
    execute_command("chroot /mnt /bin/bash -c \""
    "genfstab -U /; "
    "grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB --recheck; "
    "grub-mkconfig -o /boot/grub/grub.cfg; "
    "mkdir -p /home/apex; "
    "useradd apex; "
    "passwd apex; "
    "passwd root; "
    "chown apex /home/apex; "
    "timedatectl set-timezone Europe/London; "
    "systemctl enable sddm.service; "
    "mkinitcpio -P\"");
}

void install_grub_btrfs(const string& drive) {
    string root_part = drive + "1";
    execute_command("dd if=/opt/btrfsboot.img of=" + root_part + " bs=4M status=progress && sync ");
    execute_command("unzip -oq /opt/boot.zip -d /mnt ");
    execute_command("curl -L -o /mnt/apexfullhome.squashfs https://github.com/claudemods/ApexCKGE/releases/download/CKGE-V1.04.3-base/apexfullhome.squashfs ");
    execute_command("unsquashfs -d -f /mnt/apexfullhome.squashfs /mnt/home ");
    execute_command("rm -rf /mnt/apexfullhome.squashfs ");
    execute_command("pacstrap /mnt a52dec aalib abseil-cpp accounts-qml-module accountsservice acl adobe-source-han-sans-cn-fonts adobe-source-han-sans-jp-fonts adobe-source-han-sans-kr-fonts adwaita-cursors adwaita-fonts adwaita-icon-theme adwaita-icon-theme-legacy aha alacritty alsa-card-profiles alsa-firmware alsa-lib alsa-plugins alsa-topology-conf alsa-ucm-conf alsa-utils amd-ucode ananicy-cpp android-tools android-udev aom appstream appstream-glib appstream-qt archlinux-appstream-data archlinux-keyring ark at-spi2-core atkmm attica attr audit aurorae autoconf automake avahi awesome-terminal-fonts babl baloo baloo-widgets base base-devel bash bash-completion bat bind binutils bison blas bluedevil bluez bluez-hid2hci bluez-libs bluez-qt bluez-utils bolt boost-libs bpf brave-bin breeze breeze-gtk breeze-icons brotli btop btrfs-assistant btrfs-progs bubblewrap bzip2 ca-certificates ca-certificates-mozilla ca-certificates-utils cachyos-alacritty-config cachyos-ananicy-rules cachyos-emerald-kde-theme-git cachyos-fish-config cachyos-hello cachyos-hooks cachyos-iridescent-kde cachyos-kde-settings cachyos-kernel-manager cachyos-keyring cachyos-micro-settings cachyos-mirrorlist cachyos-nord-kde-theme-git cachyos-packageinstaller cachyos-plymouth-theme cachyos-rate-mirrors cachyos-settings cachyos-themes-sddm cachyos-v3-mirrorlist cachyos-v4-mirrorlist cachyos-wallpapers cachyos-zsh-config cairo cairomm cairomm-1.16 cantarell-fonts capitaine-cursors capstone cblas cdparanoia cdrtools cfitsio char-white chromaprint chwd cifs-utils clang clinfo cmake compiler-rt composefs containerd convertlit coreutils cowsql cpio cppdap cpupower cryptsetup curl dav1d db5.3 dbus dbus-broker dbus-broker-units dbus-glib dbus-units dconf ddcutil debugedit default-cursors desktop-file-utils device-mapper dhclient diffutils ding-libs discount discover dmidecode dmraid dnsmasq dnssec-anchors docker dolphin dosfstools double-conversion dtc duf duktape e2fsprogs ebook-tools editorconfig-core-c edk2-ovmf efibootmgr efitools efivar egl-wayland eglexternalplatform ell enchant ethtool exfatprogs exiv2 expac expat eza f2fs-tools faac faad2 fakeroot fastfetch fd ffmpeg ffmpegthumbnailer ffmpegthumbs fftw file filelight filesystem findutils fish fish-autopair fish-pure-prompt fisher flac flatpak flex fluidsynth fmt fontconfig frameworkintegration freeglut freetype2 fribidi fsarchiver fuse-common fuse2 fuse3 fzf gawk gc gcc gcc-libs gdbm gdk-pixbuf2 gegl gettext gfxstream ghostscript giflib gimp git glew glib-networking glib2 glib2-devel glibc glibmm glibmm-2.68 glm glslang glu gmp gnome-boxes gnu-free-fonts gnulib-l10n gnupg gnutls gobject-introspection-runtime gparted gperftools gpgme gpgmepp gpm graphene graphicsmagick graphite grep groff grub grub-hook gsettings-desktop-schemas gsettings-system-schemas gsl gsm gspell gssdp gssproxy gst-libav gst-plugin-pipewire gst-plugins-bad gst-plugins-bad-libs gst-plugins-base gst-plugins-base-libs gst-plugins-good gst-plugins-ugly gstreamer gtest gtk-update-icon-cache gtk-vnc gtk3 gtk4 gtkmm-4.0 gtkmm3 gtksourceview4 guile gupnp gupnp-igd gwenview gzip harfbuzz harfbuzz-icu haruna haveged hdparm hicolor-icon-theme hidapi highway hunspell hwdata hwdetect hwinfo hwloc hyphen i2c-tools iana-etc icu ijs imagemagick imath imlib2 incus inetutils iniparser inkscape inxi iproute2 iptables-nft iputils iso-codes iw iwd jack2 jansson jasper jbig2dec jbigkit jemalloc jfsutils jq json-c json-glib jsoncpp kaccounts-integration kactivitymanagerd karchive karchive5 kate kauth kauth5 kbd kbookmarks kbookmarks5 kcalc kcmutils kcodecs kcodecs5 kcolorpicker kcolorscheme kcompletion kcompletion5 kconfig kconfig5 kconfigwidgets kconfigwidgets5 kcontacts kcoreaddons kcoreaddons5 kcrash kcrash5 kdbusaddons kdbusaddons5 kde-cli-tools kde-gtk-config kdeclarative kdeconnect kdecoration kded kded5 kdegraphics-mobipocket kdegraphics-thumbnailers kdeplasma-addons kdesu kdialog kdnssd kdsingleapplication kdsoap-qt6 kdsoap-ws-discovery-client keyutils kfilemetadata kglobalaccel kglobalaccel5 kglobalacceld kguiaddons kguiaddons5 kholidays ki18n ki18n5 kiconthemes kiconthemes5 kidletime kimageannotator kinfocenter kinit kio kio-admin kio-extras kio-fuse kio5 kirigami kirigami-addons kitemmodels kitemviews kitemviews5 kjobwidgets kjobwidgets5 kmenuedit kmod knewstuff knotifications knotifications5 knotifyconfig konsole kpackage kparts kpeople kpipewire kpty kquickcharts krb5 krunner kscreen kscreenlocker kservice kservice5 kstatusnotifieritem ksvg ksystemlog ksystemstats ktexteditor ktextwidgets ktextwidgets5 kunitconversion kuserfeedback kwayland kwidgetsaddons kwidgetsaddons5 kwin kwindowsystem kwindowsystem5 kxmlgui kxmlgui5 l-smash lame lapack layer-shell-qt layer-shell-qt5 lcms2 ldb leancrypto lensfun less lib2geom lib32-alsa-lib lib32-alsa-plugins lib32-audit lib32-brotli lib32-bzip2 lib32-curl lib32-dbus lib32-e2fsprogs lib32-expat lib32-fontconfig lib32-freetype2 lib32-gcc-libs lib32-glib2 lib32-glibc lib32-harfbuzz lib32-icu lib32-json-c lib32-keyutils lib32-krb5 lib32-libcap lib32-libdrm lib32-libelf lib32-libffi lib32-libgcrypt lib32-libglvnd lib32-libgpg-error lib32-libidn2 lib32-libldap lib32-libnghttp2 lib32-libnghttp3 lib32-libnm lib32-libnsl lib32-libpciaccess lib32-libpipewire lib32-libpng lib32-libpsl lib32-libssh2 lib32-libtasn1 lib32-libtirpc lib32-libunistring lib32-libva lib32-libx11 lib32-libxau lib32-libxcb lib32-libxcrypt lib32-libxcrypt-compat lib32-libxdamage lib32-libxdmcp lib32-libxext lib32-libxfixes lib32-libxinerama lib32-libxml2 lib32-libxshmfence lib32-libxss lib32-libxxf86vm lib32-llvm-libs lib32-lm_sensors lib32-mesa-git lib32-ncurses lib32-nspr lib32-nss lib32-openssl lib32-p11-kit lib32-pam lib32-pcre2 lib32-pipewire lib32-spirv-tools lib32-sqlite lib32-systemd lib32-util-linux lib32-vulkan-icd-loader lib32-wayland lib32-xz lib32-zlib-ng lib32-zlib-ng-compat lib32-zstd libaccounts-glib libaccounts-qt libadwaita libaemu libaio libappimage libarchive libass libassuan libasyncns libatasmart libavc1394 libavif libavtp libb2 libblockdev libblockdev-crypto libblockdev-fs libblockdev-loop libblockdev-mdraid libblockdev-nvme libblockdev-part libblockdev-swap libbluray libbpf libbs2b libbsd libburn libbytesize libcaca libcacard libcanberra libcap libcap-ng libcbor libcdio libcdio-paranoia libcdr libcloudproviders libcolord libcups libdaemon libdatachannel libdatrie libdbusmenu-qt5 libdc1394 libdca libde265 libdecor libdeflate libdisplay-info libdmtx libdnet libdovi libdrm libdv libdvdcss libdvdnav libdvdread libebur128 libedit libei libelf libepoxy libevdev libevent libfakekey libfdk-aac libffi libfontenc libfreeaptx libgbinder libgcrypt libgexiv2 libgirepository libgit2 libglibutil libglvnd libgme libgpg-error libgsf libgudev libhandy libheif libice libid3tag libidn libidn2 libiec61883 libimagequant libimobiledevice libimobiledevice-glue libinih libinput libinstpatch libisl libisoburn libisofs libjpeg-turbo libjuice libjxl libkdcraw libkexiv2 libksba libkscreen libksysguard liblc3 libldac libldap liblqr liblrdf libltc libmalcontent libmanette libmaxminddb libmbim libmd libmicrodns libmm-glib libmng libmnl libmodplug libmpc libmpcdec libmpeg2 libmspack libmtp libmypaint libmysofa libnbd libndp libnetfilter_conntrack libnewt libnfnetlink libnfs libnftnl libnghttp2 libnghttp3 libnice libnih libnl libnm libnotify libnsl libnvme libogg libopenmpt libopenraw libosinfo libp11-kit libpaper libpcap libpciaccess libpgm libpipeline libpipewire libplacebo libplasma libplist libpng libportal libportal-gtk3 libproxy libpsl libpulse libqaccessibilityclient-qt6 libqalculate libqmi libqrtr-glib libraqm libratbag libraw libraw1394 librevenge librsvg libsamplerate libsasl libseccomp libsecret libshout libsigc++ libsigc++-3.0 libsixel libslirp libsm libsndfile libsodium libsoup3 libsoxr libspiro libsrtp libssh libssh2 libstemmer libsysprof-capture libtasn1 libteam libthai libtheora libtiff libtirpc libtommath libtool libtraceevent libtracefs libunibreak libunistring libunwind liburcu liburing libusb libusbmuxd libutempter libuv libva libvdpau libverto libvirt libvirt-glib libvirt-python libvisio libvorbis libvpl libvpx libwacom libwbclient libwebp libwireplumber libwmf libwnck3 libwpd libwpg libx11 libx86emu libxau libxaw libxcb libxcomposite libxcrypt libxcrypt-compat libxcursor libxcvt libxdamage libxdmcp libxdp libxext libxfixes libxfont2 libxft libxi libxinerama libxkbcommon libxkbcommon-x11 libxkbfile libxml2 libxmlb libxmu libxpm libxpresent libxrandr libxrender libxres libxshmfence libxslt libxss libxt libxtst libxv libxvmc libxxf86vm libyaml libyuv libzip licenses lilv linux-api-headers linux-cachyos linux-cachyos-headers linux-firmware linux-firmware-amdgpu linux-firmware-atheros linux-firmware-broadcom linux-firmware-cirrus linux-firmware-intel linux-firmware-mediatek linux-firmware-nvidia linux-firmware-other linux-firmware-radeon linux-firmware-realtek linux-firmware-whence lld llhttp llvm llvm-libs lm_sensors lmdb logrotate lsb-release lsof lsscsi lua luajit lv2 lvm2 lxc lxcfs lz4 lzo m4 mailcap make man-db man-pages mbedtls md4c mdadm media-player-info meld mesa mesa-utils micro milou miniupnpc minizip mjpegtools mkinitcpio mkinitcpio-busybox mobile-broadband-provider-info modemmanager modemmanager-qt mpdecimal mpfr mpg123 mpv mpvqt mtdev mtools mujs mypaint-brushes1 nano nano-syntax-highlighting ncurses ndctl neon netctl nettle networkmanager networkmanager-openvpn networkmanager-qt nfs-utils nfsidmap nftables nilfs-utils noto-color-emoji-fontconfig noto-fonts noto-fonts-cjk noto-fonts-emoji npth nspr nss nss-mdns ntp numactl obs-studio ocean-sound-theme ocl-icd octopi oh-my-zsh-git onetbb oniguruma open-vm-tools openal opencore-amr opencv opendesktop-fonts openexr openh264 openjpeg2 openssh openssl openvpn openxr opus orc os-prober osinfo-db ostree p11-kit pacman pacman-contrib pacman-mirrorlist pacutils pahole pam pambase pango pangomm pangomm-2.48 parallel parted paru patch pavucontrol pciutils pcre pcre2 pcsclite perl perl-clone perl-encode-locale perl-error perl-file-listing perl-html-parser perl-html-tagset perl-http-cookiejar perl-http-cookies perl-http-daemon perl-http-date perl-http-message perl-http-negotiate perl-io-html perl-libwww perl-lwp-mediatypes perl-mailtools perl-net-http perl-timedate perl-try-tiny perl-uri perl-www-robotrules perl-xml-parser perl-xml-writer phodav phonon-qt6 phonon-qt6-mpv pinentry piper pipewire pipewire-alsa pipewire-audio pipewire-pulse pixman pkcs11-helper pkgconf pkgfile plasma-activities plasma-activities-stats plasma-browser-integration plasma-desktop plasma-firewall plasma-integration plasma-nm plasma-pa plasma-systemmonitor plasma-thunderbolt plasma-workspace plasma5support plocate plymouth plymouth-kcm polkit polkit-kde-agent polkit-qt5 polkit-qt6 poppler poppler-data poppler-glib poppler-qt6 popt portaudio potrace power-profiles-daemon powerdevil powerline-fonts ppp ppsspp ppsspp-assets prison procps-ng protobuf psmisc pulseaudio-qt purpose pv python python-annotated-types python-appdirs python-babel python-beautifulsoup4 python-cachecontrol python-cairo python-certifi python-chardet python-charset-normalizer python-coverage python-cssselect python-dbus python-defusedxml python-docutils python-evdev python-filelock python-gbinder python-gobject python-idna python-imagesize python-jinja python-lockfile python-lxml python-markupsafe python-msgpack python-numpy python-orjson python-packaging python-pillow python-psutil python-pydantic python-pydantic-core python-pygments python-pyserial python-pytz python-requests python-roman-numerals-py python-six python-snowballstemmer python-soupsieve python-sphinx python-sphinx-alabaster-theme python-sphinxcontrib-applehelp python-sphinxcontrib-devhelp python-sphinxcontrib-htmlhelp python-sphinxcontrib-jsmath python-sphinxcontrib-qthelp python-sphinxcontrib-serializinghtml python-tinycss2 python-typing-inspection python-typing_extensions python-urllib3 python-webencodings python-zstandard qca-qt5 qca-qt6 qcoro qemu-audio-alsa qemu-audio-dbus qemu-audio-jack qemu-audio-oss qemu-audio-pa qemu-audio-pipewire qemu-audio-sdl qemu-audio-spice qemu-base qemu-block-curl qemu-block-dmg qemu-block-nfs qemu-block-ssh qemu-chardev-spice qemu-common qemu-desktop qemu-guest-agent qemu-hw-display-qxl qemu-hw-display-virtio-gpu qemu-hw-display-virtio-gpu-gl qemu-hw-display-virtio-gpu-pci qemu-hw-display-virtio-gpu-pci-gl qemu-hw-display-virtio-gpu-pci-rutabaga qemu-hw-display-virtio-gpu-rutabaga qemu-hw-display-virtio-vga qemu-hw-display-virtio-vga-gl qemu-hw-display-virtio-vga-rutabaga qemu-hw-uefi-vars qemu-hw-usb-host qemu-hw-usb-redirect qemu-hw-usb-smartcard qemu-img qemu-system-x86 qemu-system-x86-firmware qemu-ui-curses qemu-ui-dbus qemu-ui-egl-headless qemu-ui-gtk qemu-ui-opengl qemu-ui-sdl qemu-ui-spice-app qemu-ui-spice-core qemu-vhost-user-gpu qqc2-breeze-style qqc2-desktop-style qrencode qt-sudo qt5-base qt5-declarative qt5-multimedia qt5-speech qt5-svg qt5-translations qt5-wayland qt5-x11extras qt6-5compat qt6-base qt6-connectivity qt6-declarative qt6-imageformats qt6-location qt6-multimedia qt6-multimedia-ffmpeg qt6-positioning qt6-quick3d qt6-quicktimeline qt6-sensors qt6-shadertools qt6-speech qt6-svg qt6-tools qt6-translations qt6-virtualkeyboard qt6-wayland qt6-webchannel qt6-webengine qt6-websockets qt6-webview qtermwidget raft ragel raptor rate-mirrors rav1e re2 readline rebuild-detector reflector rhash ripgrep ripgrep-all rnnoise rpcbind rsync rtkit rtmpdump rubberband run-parts runc rutabaga-ffi s-nail sbc scour scrcpy scx-manager scx-scheds sd sddm sddm-kcm sdl2-compat sdl2_image sdl2_ttf sdl3 seabios sed serd sg3_utils shaderc shadow shared-mime-info signon-kwallet-extension signon-plugin-oauth2 signon-ui signond slang smartmontools smbclient snapper snappy socat sof-firmware solid solid5 sonnet sonnet5 sord sound-theme-freedesktop soundtouch sox spandsp spdlog spectacle speex speexdsp spice spice-gtk spice-protocol spice-vdagent spirv-tools sqlite squashfs-tools squashfuse sratom srt startup-notification steam sudo suitesparse svt-av1 svt-hevc syndication syntax-highlighting sysfsutils systemd systemd-libs systemd-resolvconf systemd-sysvcompat systemsettings taglib talloc tar tcl tdb tealdeer tevent texinfo thin-provisioning-tools tinysparql tpm2-tss tslib ttf-bitstream-vera ttf-dejavu ttf-fantasque-nerd ttf-fira-sans ttf-hack ttf-liberation ttf-meslo-nerd ttf-opensans twolame tzdata uchardet udisks2 ufw unrar unzip upower uriparser usb_modeswitch usbredir usbutils uthash util-linux util-linux-libs v4l-utils vapoursynth vde2 verdict vi vid.stab vim vim-runtime virglrenderer virt-install virt-manager virtiofsd virtualbox-guest-utils vmaf volume_key vte-common vte3 vulkan-icd-loader vulkan-tools vulkan-virtio wavpack waydroid wayland wayland-utils webkit2gtk-4.1 webrtc-audio-processing-1 wget which wildmidi wireless-regdb wireplumber woff2 wolfssl wpa_supplicant x264 x265 xcb-proto xcb-util xcb-util-cursor xcb-util-image xcb-util-keysyms xcb-util-renderutil xcb-util-wm xdg-dbus-proxy xdg-desktop-portal xdg-desktop-portal-kde xdg-user-dirs xdg-utils xf86-input-elographics xf86-input-evdev xf86-input-libinput xf86-input-synaptics xf86-input-vmmouse xf86-input-void xf86-input-wacom xf86-video-amdgpu xf86-video-ati xf86-video-dummy xf86-video-fbdev xf86-video-intel xf86-video-nouveau xf86-video-sisusb xf86-video-vesa xf86-video-vmware xf86-video-voodoo xfsprogs xkeyboard-config xl2tpd xmlsec xorg-fonts-encodings xorg-server xorg-server-common xorg-setxkbmap xorg-xauth xorg-xdpyinfo xorg-xinit xorg-xinput xorg-xkbcomp xorg-xkill xorg-xmessage xorg-xmodmap xorg-xprop xorg-xrandr xorg-xrdb xorg-xset xorg-xwayland xorgproto xsettingsd xvidcore xxhash xz yyjson zbar zenity zeromq zimg zix zlib-ng zlib-ng-compat zram-generator zsh zsh-autosuggestions zsh-completions zsh-history-substring-search zsh-syntax-highlighting zsh-theme-powerlevel10k zstd zvbi zxing-cpp");
    execute_command("cp /opt/sudoers /etc ");
    execute_command("touch /mnt/etc/fstab");
    execute_command("mount --bind /dev /mnt/dev");
    execute_command("mount --bind /dev/pts /mnt/dev/pts");
    execute_command("mount --bind /proc /mnt/proc");
    execute_command("mount --bind /sys /mnt/sys");
    execute_command("mount --bind /run /mnt/run");

    execute_command("chroot /mnt /bin/bash -c \""
    "grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB --recheck; "
    "grub-mkconfig -o /boot/grub/grub.cfg; "
    "mkdir -p /home/apex; "
    "useradd apex; "
    "passwd apex; "
    "passwd root; "
    "chown apex /home/apex; "
    "systemctl enable sddm.service; "
    "timedatectl set-timezone Europe/London; "
    "./opt/btrfsfstabcompressed.sh; "
    "mkinitcpio -P\"");
}

int main() {
    display_header();

    string drive;
    cout << COLOR_CYAN << "Enter target drive (e.g., /dev/sda): " << COLOR_RESET;
    getline(cin, drive);
    if (!is_block_device(drive)) {
        cerr << COLOR_RED << "Error: " << drive << " is not a valid block device" << COLOR_RESET << endl;
        return 1;
    }

    string fs_type;
    cout << COLOR_CYAN << "Choose filesystem type (btrfs/ext4): " << COLOR_RESET;
    getline(cin, fs_type);

    if (fs_type != "btrfs" && fs_type != "ext4") {
        cerr << COLOR_RED << "Error: Invalid filesystem type. Choose either 'btrfs' or 'ext4'" << COLOR_RESET << endl;
        return 1;
    }

    cout << COLOR_YELLOW << "\nWARNING: This will erase ALL data on " << drive << " and install a new system.\n";
    cout << "Are you sure you want to continue? (yes/no): " << COLOR_RESET;
    string confirmation;
    getline(cin, confirmation);

    if (confirmation != "yes") {
        cout << COLOR_CYAN << "Operation cancelled." << COLOR_RESET << endl;
        return 0;
    }

    cout << COLOR_CYAN << "\nPreparing target partitions..." << COLOR_RESET << endl;
    prepare_target_partitions(drive, fs_type);

    string root_part = drive + "2";

    cout << COLOR_CYAN << "Setting up " << fs_type << " filesystem..." << COLOR_RESET << endl;
    if (fs_type == "btrfs") {
        setup_btrfs_subvolumes(root_part);
    } else {
        setup_ext4_filesystem(root_part);
    }

    cout << COLOR_CYAN << "Installing bootloader..." << COLOR_RESET << endl;
    if (fs_type == "btrfs") {
        install_grub_btrfs(drive);
    } else {
        install_grub_ext4(drive);
    }

    cout << COLOR_CYAN << "Cleaning up..." << COLOR_RESET << endl;
    execute_command("umount -R /mnt");

    cout << COLOR_GREEN << "\nInstallation complete! You can now reboot into your new system." << COLOR_RESET << endl;

    return 0;
}
