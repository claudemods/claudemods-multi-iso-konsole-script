#include <QApplication>
#include <QMainWindow>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QScrollBar>
#include <QInputDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QStyleFactory>
#include <QSettings>
#include <QStandardPaths>
#include <QButtonGroup>
#include <QRadioButton>
#include <fstream>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <dirent.h>

// Constants - preserved exactly as in original
const std::string ORIG_IMG_NAME = "system.img";
const std::string COMPRESSED_IMG_NAME = "rootfs.img";
std::string MOUNT_POINT = "/mnt/btrfs_temp";
const std::string SOURCE_DIR = "/";
const std::string COMPRESSION_LEVEL = "22";
const std::string SQUASHFS_COMPRESSION = "zstd";
const std::vector<std::string> SQUASHFS_COMPRESSION_ARGS = {"-Xcompression-level", "22"};
std::string BUILD_DIR = "/home/$USER/.config/cmi/build-image-arch-img";
std::string USERNAME = "";
const std::string BTRFS_LABEL = "LIVE_SYSTEM";
const std::string BTRFS_COMPRESSION = "zstd";

// Dependencies list - preserved exactly
const std::vector<std::string> DEPENDENCIES = {
    "rsync",
    "squashfs-tools",
    "xorriso",
    "grub",
    "dosfstools",
    "unzip",
    "arch-install-scripts",
    "bash-completion",
    "erofs-utils",
    "findutils",
    "jq",
    "libarchive",
    "libisoburn",
    "lsb-release",
    "lvm2",
    "mkinitcpio-archiso",
    "mkinitcpio-nfs-utils",
    "mtools",
    "nbd",
    "pacman-contrib",
    "parted",
    "procps-ng",
    "pv",
    "python",
    "sshfs",
    "syslinux",
    "xdg-utils",
    "zsh-completions",
    "kernel-modules-hook",
    "virt-manager",
    "qt6-tools",
    "qt5-tools"
};

// Configuration state - preserved exactly
struct ConfigState {
    std::string isoTag;
    std::string isoName;
    std::string outputDir;
    std::string vmlinuzPath;
    bool mkinitcpioGenerated = false;
    bool grubEdited = false;
    bool dependenciesInstalled = false;

    bool isReadyForISO() const {
        return !isoTag.empty() && !isoName.empty() && !outputDir.empty() &&
        !vmlinuzPath.empty() && mkinitcpioGenerated && grubEdited && dependenciesInstalled;
    }
} config;

class PasswordDialog : public QDialog {
public:
    PasswordDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Enter Password");
        setModal(true);
        setFixedSize(300, 150);

        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *label = new QLabel("Enter sudo password:", this);
        passwordEdit = new QLineEdit(this);
        passwordEdit->setEchoMode(QLineEdit::Password);

        QPushButton *okButton = new QPushButton("OK", this);
        QPushButton *cancelButton = new QPushButton("Cancel", this);

        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);

        layout->addWidget(label);
        layout->addWidget(passwordEdit);
        layout->addLayout(buttonLayout);

        connect(okButton, &QPushButton::clicked, this, &PasswordDialog::accept);
        connect(cancelButton, &QPushButton::clicked, this, &PasswordDialog::reject);

        // Style the dialog
        setStyleSheet(
            "QDialog { background-color: #000033; }"
            "QLabel { color: cyan; }"
            "QLineEdit { background-color: #000055; color: cyan; border: 1px solid #0088FF; }"
            "QPushButton { background-color: #0066FF; color: white; border: 1px solid #0088FF; padding: 5px; }"
            "QPushButton:hover { background-color: #0088FF; }"
        );
    }

    QString getPassword() const {
        return passwordEdit->text();
    }

private:
    QLineEdit *passwordEdit;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        setupUI();
        loadConfig();
        updateConfigStatus();

        // Get current username
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            USERNAME = pw->pw_name;
        } else {
            QMessageBox::critical(this, "Error", "Failed to get username!");
            exit(1);
        }

        // Set BUILD_DIR based on username
        BUILD_DIR = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";

        // Create config directory if it doesn't exist
        QDir().mkpath(QString::fromStdString("/home/" + USERNAME + "/.config/cmi"));
    }

private slots:
    void onInstallDependencies() {
        PasswordDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            QString password = dialog.getPassword();
            if (password.isEmpty()) {
                QMessageBox::warning(this, "Warning", "Password cannot be empty!");
                return;
            }

            logOutput->append("Installing required dependencies...\n");

            // Build package list string - using original commands exactly
            QString packages;
            for (const auto& pkg : DEPENDENCIES) {
                packages += QString::fromStdString(pkg) + " ";
            }

            // Original command preserved exactly
            QString command = "sudo pacman -Sy --needed --noconfirm " + packages;
            executeCommand(command, password);

            config.dependenciesInstalled = true;
            saveConfig();
            logOutput->append("\nDependencies installed successfully!\n");
            updateConfigStatus();
        }
    }

    void onSetIsoTag() {
        bool ok;
        QString text = QInputDialog::getText(this, "Set ISO Tag", "Enter ISO tag (e.g., 2025):", QLineEdit::Normal, "", &ok);
        if (ok && !text.isEmpty()) {
            config.isoTag = text.toStdString();
            saveConfig();
            updateConfigStatus();
        }
    }

    void onSetIsoName() {
        bool ok;
        QString text = QInputDialog::getText(this, "Set ISO Name", "Enter ISO name (e.g., claudemods.iso):", QLineEdit::Normal, "", &ok);
        if (ok && !text.isEmpty()) {
            config.isoName = text.toStdString();
            saveConfig();
            updateConfigStatus();
        }
    }

    void onSetOutputDir() {
        QString defaultDir = "/home/" + QString::fromStdString(USERNAME) + "/Downloads";
        bool ok;
        QString text = QInputDialog::getText(this, "Set Output Directory",
                                             "Current output directory: " + QString::fromStdString(config.outputDir.empty() ? "Not set" : config.outputDir) +
                                             "\nDefault directory: " + defaultDir +
                                             "\nEnter output directory:", QLineEdit::Normal, defaultDir, &ok);

        if (ok && !text.isEmpty()) {
            QString dir = text;
            // Replace $USER with actual username - preserving original logic
            dir.replace("$USER", QString::fromStdString(USERNAME));

            // If empty, use default - preserving original logic
            if (dir.isEmpty()) {
                dir = defaultDir;
            }

            config.outputDir = dir.toStdString();
            saveConfig();
            updateConfigStatus();
        }
    }

    void onSelectVmlinuz() {
        QDir dir("/boot");
        QStringList vmlinuzFiles;

        // Preserving original directory scanning logic
        for (const auto& file : dir.entryList(QDir::Files)) {
            if (file.startsWith("vmlinuz")) {
                vmlinuzFiles.append("/boot/" + file);
            }
        }

        if (vmlinuzFiles.isEmpty()) {
            QMessageBox::critical(this, "Error", "No vmlinuz files found in /boot!");
            return;
        }

        bool ok;
        QString item = QInputDialog::getItem(this, "Select vmlinuz", "Available vmlinuz files:", vmlinuzFiles, 0, false, &ok);
        if (ok && !item.isEmpty()) {
            config.vmlinuzPath = item.toStdString();
            logOutput->append("Selected: " + item + "\n");
            saveConfig();
            updateConfigStatus();
        }
    }

    void onGenerateMkinitcpio() {
        if (config.vmlinuzPath.empty()) {
            QMessageBox::warning(this, "Warning", "Please select vmlinuz first!");
            return;
        }

        if (BUILD_DIR.empty()) {
            QMessageBox::warning(this, "Warning", "Build directory not set!");
            return;
        }

        PasswordDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            QString password = dialog.getPassword();
            if (password.isEmpty()) {
                QMessageBox::warning(this, "Warning", "Password cannot be empty!");
                return;
            }

            // Preserving original commands exactly
            QString vmlinuzDest = QString::fromStdString(BUILD_DIR) + "/boot/vmlinuz-x86_64";
            logOutput->append("Copying " + QString::fromStdString(config.vmlinuzPath) + " to " + vmlinuzDest + "\n");
            executeCommand("sudo cp " + QString::fromStdString(config.vmlinuzPath) + " " + vmlinuzDest, password);

            logOutput->append("Generating initramfs...\n");
            executeCommand("cd " + QString::fromStdString(BUILD_DIR) + " && sudo mkinitcpio -c mkinitcpio.conf -g " +
            QString::fromStdString(BUILD_DIR) + "/boot/initramfs-x86_64.img", password);

            config.mkinitcpioGenerated = true;
            saveConfig();
            logOutput->append("mkinitcpio generated successfully!\n");
            updateConfigStatus();
        }
    }

    void onEditGrubCfg() {
        if (BUILD_DIR.empty()) {
            QMessageBox::warning(this, "Warning", "Build directory not set!");
            return;
        }

        // Preserving original command exactly
        QString grubCfgPath = QString::fromStdString(BUILD_DIR) + "/boot/grub/grub.cfg";
        logOutput->append("Editing GRUB config: " + grubCfgPath + "\n");

        QProcess::startDetached("sudo", {"nano", grubCfgPath});

        config.grubEdited = true;
        saveConfig();
        logOutput->append("GRUB config edited!\n");
        updateConfigStatus();
    }

    void onCreateImage() {
        // Preserving original paths exactly
        QString outputDir = QString::fromStdString(getOutputDirectory());
        QString outputOrigImgPath = outputDir + "/" + QString::fromStdString(ORIG_IMG_NAME);
        QString outputCompressedImgPath = outputDir + "/" + QString::fromStdString(COMPRESSED_IMG_NAME);

        // Cleanup old files - preserving original commands exactly
        executeCommand("sudo umount " + QString::fromStdString(MOUNT_POINT) + " 2>/dev/null || true");
        executeCommand("sudo rm -rf " + outputOrigImgPath);
        executeCommand("sudo mkdir -p " + QString::fromStdString(MOUNT_POINT));
        executeCommand("sudo mkdir -p " + outputDir);

        bool ok;
        QString imgSize = QInputDialog::getText(this, "Image Size", "Enter the image size in GB (e.g., 6 for 6GB):", QLineEdit::Normal, "", &ok) + "G";
        if (!ok) return;

        // Filesystem selection dialog
        QDialog fsDialog(this);
        fsDialog.setWindowTitle("Choose Filesystem Type");
        QVBoxLayout fsLayout(&fsDialog);

        QButtonGroup fsGroup;
        QRadioButton *btrfsBtn = new QRadioButton("btrfs");
        QRadioButton *ext4Btn = new QRadioButton("ext4");
        fsGroup.addButton(btrfsBtn);
        fsGroup.addButton(ext4Btn);
        btrfsBtn->setChecked(true);

        QPushButton *okBtn = new QPushButton("OK");

        fsLayout.addWidget(btrfsBtn);
        fsLayout.addWidget(ext4Btn);
        fsLayout.addWidget(okBtn);

        connect(okBtn, &QPushButton::clicked, &fsDialog, &QDialog::accept);

        if (fsDialog.exec() != QDialog::Accepted) return;

        QString fsType = btrfsBtn->isChecked() ? "btrfs" : "ext4";

        PasswordDialog pwdDialog(this);
        if (pwdDialog.exec() != QDialog::Accepted) return;
        QString password = pwdDialog.getPassword();
        if (password.isEmpty()) {
            QMessageBox::warning(this, "Warning", "Password cannot be empty!");
            return;
        }

        // Preserving original image creation commands exactly
        logOutput->append("Creating image file...\n");
        executeCommand("sudo truncate -s " + imgSize + " " + outputOrigImgPath, password);

        if (fsType == "btrfs") {
            logOutput->append("Creating Btrfs filesystem with " + QString::fromStdString(BTRFS_COMPRESSION) + " compression\n");

            // Preserving original BTRFS commands exactly
            executeCommand("sudo mkdir -p " + QString::fromStdString(SOURCE_DIR) + "/btrfs_rootdir", password);

            QString command = "sudo mkfs.btrfs -L \"" + QString::fromStdString(BTRFS_LABEL) + "\" --compress=" +
            QString::fromStdString(BTRFS_COMPRESSION) + " --rootdir=" + QString::fromStdString(SOURCE_DIR) +
            "/btrfs_rootdir -f " + outputOrigImgPath;
            executeCommand(command, password);

            executeCommand("sudo rmdir " + QString::fromStdString(SOURCE_DIR) + "/btrfs_rootdir", password);
        } else {
            logOutput->append("Formatting as ext4\n");
            // Preserving original ext4 command exactly
            executeCommand("sudo mkfs.ext4 -F -L \"SYSTEM_BACKUP\" " + outputOrigImgPath, password);
        }

        // Preserving original mount command exactly
        logOutput->append("Mounting filesystem...\n");
        if (fsType == "btrfs") {
            executeCommand("sudo mount " + outputOrigImgPath + " " + QString::fromStdString(MOUNT_POINT), password);
        } else {
            executeCommand("sudo mount -o loop,discard,noatime " + outputOrigImgPath + " " + QString::fromStdString(MOUNT_POINT), password);
        }

        // Preserving original rsync command exactly
        logOutput->append("Copying files with rsync...\n");
        QString rsyncCmd = "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
        "--exclude=/etc/udev/rules.d/70-persistent-cd.rules "
        "--exclude=/etc/udev/rules.d/70-persistent-net.rules "
        "--exclude=/etc/mtab "
        "--exclude=/etc/fstab "
        "--exclude=/dev/* "
        "--exclude=/proc/* "
        "--exclude=/sys/* "
        "--exclude=/tmp/* "
        "--exclude=/run/* "
        "--exclude=/mnt/* "
        "--exclude=/media/* "
        "--exclude=/lost+found "
        "--exclude=*rootfs1.img "
        "--exclude=btrfs_temp "
        "--exclude=rootfs.img " +
        QString::fromStdString(SOURCE_DIR) + " " + QString::fromStdString(MOUNT_POINT);
        executeCommand(rsyncCmd, password);

        if (fsType == "btrfs") {
            // Preserving original BTRFS optimization commands exactly
            logOutput->append("Optimizing compression...\n");
            executeCommand("sudo btrfs filesystem defrag -r -v -c " + QString::fromStdString(BTRFS_COMPRESSION) + " " + QString::fromStdString(MOUNT_POINT), password);

            logOutput->append("Finalizing image...\n");
            executeCommand("sudo btrfs filesystem resize max " + QString::fromStdString(MOUNT_POINT), password);
        }

        // Preserving original unmount commands exactly
        logOutput->append("Unmounting...\n");
        executeCommand("sudo umount " + QString::fromStdString(MOUNT_POINT), password);
        executeCommand("sudo rmdir " + QString::fromStdString(MOUNT_POINT), password);

        // Preserving original squashfs command exactly
        logOutput->append("Creating SquashFS...\n");
        QString mksquashfsCmd = "sudo mksquashfs " + outputOrigImgPath + " " + outputCompressedImgPath +
        " -comp " + QString::fromStdString(SQUASHFS_COMPRESSION) +
        " " + QString::fromStdString(SQUASHFS_COMPRESSION_ARGS[0]) + " " + QString::fromStdString(SQUASHFS_COMPRESSION_ARGS[1]) +
        " -noappend";
        executeCommand(mksquashfsCmd, password);

        // Preserving original delete command exactly
        logOutput->append("Deleting original image...\n");
        executeCommand("sudo rm -f " + outputOrigImgPath, password);

        // Preserving original checksum command exactly
        logOutput->append("Creating checksum...\n");
        executeCommand("md5sum " + outputCompressedImgPath + " > " + outputCompressedImgPath + ".md5");

        // Show final message with original commands
        logOutput->append("\n" + QString::fromStdString(fsType == "btrfs" ? "Compressed BTRFS" : "Ext4") +
        " image created successfully: " + outputCompressedImgPath + "\n");
        logOutput->append("Checksum file: " + outputCompressedImgPath + ".md5\n");
        logOutput->append("Size: ");
        executeCommand("sudo du -h " + outputCompressedImgPath + " | cut -f1");
    }

    void onCreateISO() {
        if (!config.isReadyForISO()) {
            QMessageBox::warning(this, "Warning", "Cannot create ISO - setup is incomplete!");
            return;
        }

        PasswordDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) return;
        QString password = dialog.getPassword();
        if (password.isEmpty()) {
            QMessageBox::warning(this, "Warning", "Password cannot be empty!");
            return;
        }

        logOutput->append("\nStarting ISO creation process...\n");

        // Preserving original path expansion logic
        QString expandedOutputDir = QString::fromStdString(config.outputDir);
        expandedOutputDir.replace("$USER", QString::fromStdString(USERNAME));

        // Preserving original xorriso command exactly
        QString xorrisoCmd = "sudo xorriso -as mkisofs "
        "--modification-date=\"$(date +%Y%m%d%H%M%S00)\" "
        "--protective-msdos-label "
        "-volid \"" + QString::fromStdString(config.isoTag) + "\" "
        "-appid \"claudemods Linux Live/Rescue CD\" "
        "-publisher \"claudemods claudemods101@gmail.com >\" "
        "-preparer \"Prepared by user\" "
        "-r -graft-points -no-pad "
        "--sort-weight 0 / "
        "--sort-weight 1 /boot "
        "--grub2-mbr " + QString::fromStdString(BUILD_DIR) + "/boot/grub/i386-pc/boot_hybrid.img "
        "-partition_offset 16 "
        "-b boot/grub/i386-pc/eltorito.img "
        "-c boot.catalog "
        "-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info "
        "-eltorito-alt-boot "
        "-append_partition 2 0xef " + QString::fromStdString(BUILD_DIR) + "/boot/efi.img "
        "-e --interval:appended_partition_2:all:: "
        "-no-emul-boot "
        "-iso-level 3 "
        "-o \"" + expandedOutputDir + "/" + QString::fromStdString(config.isoName) + "\" " +
        QString::fromStdString(BUILD_DIR);

        executeCommand(xorrisoCmd, password);
        logOutput->append("ISO created successfully at " + expandedOutputDir + "/" + QString::fromStdString(config.isoName) + "\n");
    }

    void onShowDiskUsage() {
        // Preserving original command exactly
        logOutput->append("\nCurrent system disk usage:\n");
        executeCommand("df -h /");
    }

private:
    QTextEdit *logOutput;
    QListWidget *menuList;
    QLabel *headerLabel;
    QLabel *versionLabel;

    void setupUI() {
        setWindowTitle("Advanced C++ Arch Img Iso Script");
        resize(800, 600);

        // Set main window style
        setStyleSheet(
            "QMainWindow { background-color: #000033; }"
            "QTextEdit { background-color: #000022; color: cyan; border: 1px solid #0088FF; }"
            "QListWidget { background-color: #000044; color: cyan; border: 1px solid #0088FF; }"
            "QPushButton { background-color: #0066FF; color: white; border: 1px solid #0088FF; padding: 5px; }"
            "QPushButton:hover { background-color: #0088FF; }"
            "QLabel { color: cyan; }"
            "QGroupBox { border: 1px solid #0088FF; margin-top: 10px; }"
            "QGroupBox::title { color: cyan; subcontrol-origin: margin; left: 10px; }"
            "QRadioButton { color: cyan; }"
        );

        QWidget *centralWidget = new QWidget(this);
        QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setContentsMargins(5, 5, 5, 5);
        mainLayout->setSpacing(5);

        // Add header with ASCII art
        QLabel *asciiArt = new QLabel(
            "░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗\n"
            "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n"
            "██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n"
            "██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n"
            "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n"
            "░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚══════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░"
        );
        asciiArt->setStyleSheet("color: red; font-family: monospace;");
        mainLayout->addWidget(asciiArt);

        // Add version label
        versionLabel = new QLabel("Advanced C++ Arch Img Iso Script Beta v2.01 24-07-2025");
        versionLabel->setStyleSheet("color: cyan; font-weight: bold; font-size: 12px;");
        versionLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(versionLabel);

        // Horizontal layout for menu and content
        QHBoxLayout *contentLayout = new QHBoxLayout();
        contentLayout->setSpacing(10);

        // Left side - menu
        QWidget *menuWidget = new QWidget;
        QVBoxLayout *menuLayout = new QVBoxLayout(menuWidget);
        menuLayout->setContentsMargins(0, 0, 0, 0);

        QLabel *titleLabel = new QLabel("Main Menu");
        titleLabel->setStyleSheet("font-weight: bold; font-size: 16px; color: cyan;");
        menuLayout->addWidget(titleLabel);

        menuList = new QListWidget;
        menuList->addItems({
            "Create Image",
            "ISO Creation Setup",
            "Create ISO",
            "Show Disk Usage",
            "Exit"
        });
        menuList->setCurrentRow(0);
        menuLayout->addWidget(menuList);

        // Right side - content
        QWidget *contentWidget = new QWidget;
        QVBoxLayout *rightLayout = new QVBoxLayout(contentWidget);

        // Config status at the top
        QGroupBox *statusGroup = new QGroupBox("Configuration Status");
        QVBoxLayout *statusLayout = new QVBoxLayout;

        configStatus = new QLabel;
        configStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
        statusLayout->addWidget(configStatus);

        statusGroup->setLayout(statusLayout);
        rightLayout->addWidget(statusGroup);

        // Command output below
        QLabel *logLabel = new QLabel("Command Output");
        logLabel->setStyleSheet("font-weight: bold; font-size: 16px; color: cyan;");
        rightLayout->addWidget(logLabel);

        logOutput = new QTextEdit;
        logOutput->setReadOnly(true);
        logOutput->setFontFamily("Monospace");
        rightLayout->addWidget(logOutput);

        // Add to content layout
        contentLayout->addWidget(menuWidget, 1);
        contentLayout->addWidget(contentWidget, 3);

        // Add to main layout
        mainLayout->addLayout(contentLayout);

        setCentralWidget(centralWidget);

        // Connect menu
        connect(menuList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
            int row = menuList->row(item);
            switch(row) {
                case 0: onCreateImage(); break;
                case 1: showSetupMenu(); break;
                case 2: onCreateISO(); break;
                case 3: onShowDiskUsage(); break;
                case 4: close(); break;
            }
        });
    }

    void showSetupMenu() {
        QDialog setupDialog(this);
        setupDialog.setWindowTitle("ISO Creation Setup");
        setupDialog.resize(600, 400);
        setupDialog.setStyleSheet(
            "QDialog { background-color: #000033; }"
            "QListWidget { background-color: #000044; color: cyan; border: 1px solid #0088FF; }"
            "QLabel { color: cyan; }"
        );

        QVBoxLayout *layout = new QVBoxLayout(&setupDialog);

        QListWidget *setupList = new QListWidget;
        setupList->addItems({
            "Install Dependencies",
            "Set ISO Tag",
            "Set ISO Name",
            "Set Output Directory",
            "Select vmlinuz",
            "Generate mkinitcpio",
            "Edit GRUB Config",
            "Back to Main Menu"
        });
        layout->addWidget(setupList);

        connect(setupList, &QListWidget::itemDoubleClicked, &setupDialog, [&](QListWidgetItem *item) {
            int row = setupList->row(item);
            switch(row) {
                case 0: onInstallDependencies(); break;
                case 1: onSetIsoTag(); break;
                case 2: onSetIsoName(); break;
                case 3: onSetOutputDir(); break;
                case 4: onSelectVmlinuz(); break;
                case 5: onGenerateMkinitcpio(); break;
                case 6: onEditGrubCfg(); break;
                case 7: setupDialog.accept(); break;
            }
        });

        setupDialog.exec();
    }

    void executeCommand(const QString &command, const QString &password = "") {
        logOutput->append("$ " + command + "\n");
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);

        if (!password.isEmpty()) {
            process.start("bash", {"-c", "echo '" + password + "' | sudo -S " + command});
        } else {
            process.start("bash", {"-c", command});
        }

        QTimer timer;
        timer.setInterval(100);

        connect(&timer, &QTimer::timeout, this, [&]() {
            if (process.state() == QProcess::NotRunning) return;

            QByteArray newData = process.readAll();
            if (!newData.isEmpty()) {
                logOutput->insertPlainText(QString::fromUtf8(newData));
                logOutput->verticalScrollBar()->setValue(logOutput->verticalScrollBar()->maximum());
            }
        });

        timer.start();
        process.waitForFinished(-1);
        timer.stop();

        // Read any remaining data
        QByteArray remainingData = process.readAll();
        if (!remainingData.isEmpty()) {
            logOutput->insertPlainText(QString::fromUtf8(remainingData));
            logOutput->verticalScrollBar()->setValue(logOutput->verticalScrollBar()->maximum());
        }

        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            logOutput->append("\nCommand failed with exit code " + QString::number(process.exitCode()) + "\n");
        }
    }

    void updateConfigStatus() {
        QString statusText;

        // Use QString::arg() for proper string concatenation
        statusText += QString("Dependencies Installed: <font color='%1'>%2</font><br>")
        .arg(config.dependenciesInstalled ? "lime" : "red")
        .arg(config.dependenciesInstalled ? "✓" : "✗");

        statusText += QString("ISO Tag: <font color='%1'>%2</font><br>")
        .arg(config.isoTag.empty() ? "red" : "lime")
        .arg(config.isoTag.empty() ? "Not set" : QString::fromStdString(config.isoTag));

        statusText += QString("ISO Name: <font color='%1'>%2</font><br>")
        .arg(config.isoName.empty() ? "red" : "lime")
        .arg(config.isoName.empty() ? "Not set" : QString::fromStdString(config.isoName));

        statusText += QString("Output Directory: <font color='%1'>%2</font><br>")
        .arg(config.outputDir.empty() ? "red" : "lime")
        .arg(config.outputDir.empty() ? "Not set" : QString::fromStdString(config.outputDir));

        statusText += QString("vmlinuz Selected: <font color='%1'>%2</font><br>")
        .arg(config.vmlinuzPath.empty() ? "red" : "lime")
        .arg(config.vmlinuzPath.empty() ? "Not selected" : QString::fromStdString(config.vmlinuzPath));

        statusText += QString("mkinitcpio Generated: <font color='%1'>%2</font><br>")
        .arg(config.mkinitcpioGenerated ? "lime" : "red")
        .arg(config.mkinitcpioGenerated ? "✓" : "✗");

        statusText += QString("GRUB Config Edited: <font color='%1'>%2</font><br>")
        .arg(config.grubEdited ? "lime" : "red")
        .arg(config.grubEdited ? "✓" : "✗");

        configStatus->setText(statusText);
    }

    std::string getConfigFilePath() {
        return "/home/" + USERNAME + "/.config/cmi/configuration.txt";
    }

    void saveConfig() {
        std::string configPath = getConfigFilePath();
        std::ofstream configFile(configPath);
        if (configFile.is_open()) {
            configFile << "isoTag=" << config.isoTag << "\n";
            configFile << "isoName=" << config.isoName << "\n";
            configFile << "outputDir=" << config.outputDir << "\n";
            configFile << "vmlinuzPath=" << config.vmlinuzPath << "\n";
            configFile << "mkinitcpioGenerated=" << (config.mkinitcpioGenerated ? "1" : "0") << "\n";
            configFile << "grubEdited=" << (config.grubEdited ? "1" : "0") << "\n";
            configFile << "dependenciesInstalled=" << (config.dependenciesInstalled ? "1" : "0") << "\n";
            configFile.close();
        } else {
            logOutput->append("Failed to save configuration to " + QString::fromStdString(configPath) + "\n");
        }
    }

    void loadConfig() {
        std::string configPath = getConfigFilePath();
        std::ifstream configFile(configPath);
        if (configFile.is_open()) {
            std::string line;
            while (std::getline(configFile, line)) {
                size_t delimiter = line.find('=');
                if (delimiter != std::string::npos) {
                    std::string key = line.substr(0, delimiter);
                    std::string value = line.substr(delimiter + 1);

                    if (key == "isoTag") config.isoTag = value;
                    else if (key == "isoName") config.isoName = value;
                    else if (key == "outputDir") config.outputDir = value;
                    else if (key == "vmlinuzPath") config.vmlinuzPath = value;
                    else if (key == "mkinitcpioGenerated") config.mkinitcpioGenerated = (value == "1");
                    else if (key == "grubEdited") config.grubEdited = (value == "1");
                    else if (key == "dependenciesInstalled") config.dependenciesInstalled = (value == "1");
                }
            }
            configFile.close();
        }
    }

    std::string getOutputDirectory() {
        std::string dir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-ext4img/LiveOS";
        return dir;
    }

    QLabel *configStatus;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Set custom palette for blue theme
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0, 0, 51)); // Dark blue background
    palette.setColor(QPalette::WindowText, QColor(0, 255, 255)); // Cyan text
    palette.setColor(QPalette::Base, QColor(0, 0, 34)); // Darker blue for text inputs
    palette.setColor(QPalette::AlternateBase, QColor(0, 0, 68));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, QColor(0, 255, 255)); // Cyan text
    palette.setColor(QPalette::Button, QColor(0, 51, 102)); // Blue buttons
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(0, 170, 255));
    palette.setColor(QPalette::Highlight, QColor(0, 85, 255));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}

#include "main.moc"
