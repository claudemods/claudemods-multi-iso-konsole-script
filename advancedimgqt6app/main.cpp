#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>  // Added missing include
#include <QStatusBar> // Added missing include
#include <QDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QListWidget>
#include <QTimer>
#include <QDateTime>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QFileDialog>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QScrollArea>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QProgressDialog>
#include <QThread>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <dirent.h>
#include <pwd.h>
#include <ctime>
#include <termios.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <sstream>
#include <iomanip>
#include <fstream>  // Added for file operations

// Constants
const std::string ORIG_IMG_NAME = "rootfs.img";
const std::string FINAL_IMG_NAME = "rootfs.img";
std::string MOUNT_POINT = "/mnt/btrfs_temp";
const std::string SOURCE_DIR = "/";
const std::string COMPRESSION_LEVEL = "22";
const std::string SQUASHFS_COMPRESSION = "zstd";
const std::vector<std::string> SQUASHFS_COMPRESSION_ARGS = {"-Xcompression-level", "22"};
std::string BUILD_DIR = "/home/$USER/.config/cmi/build-image-arch-img";
std::string USERNAME = "";
const std::string BTRFS_LABEL = "LIVE_SYSTEM";
const std::string BTRFS_COMPRESSION = "zstd";

// Dependencies list
const std::vector<std::string> DEPENDENCIES = {
    "rsync",
    "squashfs-tools",
    "xorriso",
    "grub",
    "dosfstools",
    "unzip",
    "nano",
    "arch-install-scripts",
    "bash-completion",
    "erofs-utils",
    "findutils",
    "unzip",
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

// Configuration state
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

// Main Application Window
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        // Initialize user info
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            USERNAME = pw->pw_name;
        } else {
            QMessageBox::critical(this, "Error", "Failed to get username!");
            exit(1);
        }

        BUILD_DIR = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";
        std::string configDir = "/home/" + USERNAME + "/.config/cmi";
        system(("mkdir -p " + configDir).c_str());

        loadConfig();

        setupUI();
        setupMenuBar();
        setupStatusBar();
        setupConnections();

        setWindowTitle("ClaudeMods Image Builder");
        resize(900, 600);

        // Start time update thread
        timeThread = std::thread(&MainWindow::updateTimeThread, this);
    }

    ~MainWindow() {
        timeThreadRunning = false;
        if (timeThread.joinable()) {
            timeThread.join();
        }
    }

private:
    void setupUI() {
        QWidget *centralWidget = new QWidget(this);
        QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

        // Banner
        bannerLabel = new QLabel(this);
        bannerLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        bannerLabel->setAlignment(Qt::AlignCenter);
        bannerLabel->setStyleSheet("font-family: monospace; color: #FF0000;");
        mainLayout->addWidget(bannerLabel);

        updateBanner();

        // Time and disk info
        timeLabel = new QLabel(this);
        timeLabel->setAlignment(Qt::AlignCenter);
        timeLabel->setStyleSheet("color: #0000FF;");
        mainLayout->addWidget(timeLabel);

        diskInfoLabel = new QLabel(this);
        diskInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        diskInfoLabel->setAlignment(Qt::AlignCenter);
        diskInfoLabel->setStyleSheet("font-family: monospace; color: #00FF00;");
        mainLayout->addWidget(diskInfoLabel);

        updateDiskInfo();

        // Config status
        configGroup = new QGroupBox("Current Configuration", this);
        QVBoxLayout *configLayout = new QVBoxLayout(configGroup);

        dependenciesCheck = new QCheckBox("Dependencies Installed", this);
        isoTagLabel = new QLabel("ISO Tag: Not set", this);
        isoNameLabel = new QLabel("ISO Name: Not set", this);
        outputDirLabel = new QLabel("Output Directory: Not set", this);
        vmlinuzLabel = new QLabel("vmlinuz Selected: Not selected", this);
        mkinitcpioCheck = new QCheckBox("mkinitcpio Generated", this);
        grubCheck = new QCheckBox("GRUB Config Edited", this);

        configLayout->addWidget(dependenciesCheck);
        configLayout->addWidget(isoTagLabel);
        configLayout->addWidget(isoNameLabel);
        configLayout->addWidget(outputDirLabel);
        configLayout->addWidget(vmlinuzLabel);
        configLayout->addWidget(mkinitcpioCheck);
        configLayout->addWidget(grubCheck);

        mainLayout->addWidget(configGroup);

        // Main menu buttons
        QHBoxLayout *buttonLayout = new QHBoxLayout();

        QPushButton *guideButton = new QPushButton("Guide", this);
        QPushButton *setupButton = new QPushButton("Setup", this);
        QPushButton *createImageButton = new QPushButton("Create Image", this);
        QPushButton *createIsoButton = new QPushButton("Create ISO", this);
        QPushButton *diskUsageButton = new QPushButton("Disk Usage", this);
        QPushButton *installUsbButton = new QPushButton("Install to USB", this);
        QPushButton *installerButton = new QPushButton("CMI Installer", this);
        QPushButton *updateButton = new QPushButton("Update Script", this);
        QPushButton *exitButton = new QPushButton("Exit", this);

        buttonLayout->addWidget(guideButton);
        buttonLayout->addWidget(setupButton);
        buttonLayout->addWidget(createImageButton);
        buttonLayout->addWidget(createIsoButton);
        buttonLayout->addWidget(diskUsageButton);
        buttonLayout->addWidget(installUsbButton);
        buttonLayout->addWidget(installerButton);
        buttonLayout->addWidget(updateButton);
        buttonLayout->addWidget(exitButton);

        mainLayout->addLayout(buttonLayout);

        // Output console
        console = new QTextEdit(this);
        console->setReadOnly(true);
        console->setStyleSheet("font-family: monospace;");
        mainLayout->addWidget(console);

        setCentralWidget(centralWidget);

        // Connect buttons
        connect(guideButton, &QPushButton::clicked, this, &MainWindow::showGuide);
        connect(setupButton, &QPushButton::clicked, this, &MainWindow::showSetupMenu);
        connect(createImageButton, &QPushButton::clicked, this, &MainWindow::createImage);
        connect(createIsoButton, &QPushButton::clicked, this, &MainWindow::createISO);
        connect(diskUsageButton, &QPushButton::clicked, this, &MainWindow::showDiskUsage);
        connect(installUsbButton, &QPushButton::clicked, this, &MainWindow::installISOToUSB);
        connect(installerButton, &QPushButton::clicked, this, &MainWindow::runCMIInstaller);
        connect(updateButton, &QPushButton::clicked, this, &MainWindow::updateScript);
        connect(exitButton, &QPushButton::clicked, this, &QApplication::quit);
    }

    void setupMenuBar() {
        QMenu *fileMenu = menuBar()->addMenu("File");
        QAction *exitAction = fileMenu->addAction("Exit");
        connect(exitAction, &QAction::triggered, this, &QApplication::quit);

        QMenu *toolsMenu = menuBar()->addMenu("Tools");
        toolsMenu->addAction("Show Config", this, &MainWindow::showConfigStatus);
        toolsMenu->addAction("Clear Console", console, &QTextEdit::clear);
    }

    void setupStatusBar() {
        statusBar()->showMessage("Ready");
    }

    void setupConnections() {
        // Timer for updating time and disk info
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &MainWindow::updateTimeDisplay);
        connect(timer, &QTimer::timeout, this, &MainWindow::updateDiskInfo);
        timer->start(1000);
    }

    void updateBanner() {
        QString bannerText = R"(
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚══════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
)";
bannerLabel->setText(bannerText + "\nAdvanced C++ Arch Img Iso Script Beta v2.01 26-07-2025");
    }

    void updateTimeDisplay() {
        std::lock_guard<std::mutex> lock(timeMutex);
        timeLabel->setText(QString("Current UK Time: %1").arg(QString::fromStdString(currentTimeStr)));
    }

    void updateDiskInfo() {
        QProcess process;
        process.start("df", QStringList() << "-h" << "/");
        process.waitForFinished();
        QString output = process.readAllStandardOutput();

        QStringList lines = output.split('\n');
        if (lines.size() > 1) {
            diskInfoLabel->setText("Filesystem      Size  Used Avail Use% Mounted on\n" + lines.last());
        }
    }

    void showConfigStatus() {
        dependenciesCheck->setChecked(config.dependenciesInstalled);
        isoTagLabel->setText(QString("ISO Tag: %1").arg(config.isoTag.empty() ? "Not set" : QString::fromStdString(config.isoTag)));
        isoNameLabel->setText(QString("ISO Name: %1").arg(config.isoName.empty() ? "Not set" : QString::fromStdString(config.isoName)));
        outputDirLabel->setText(QString("Output Directory: %1").arg(config.outputDir.empty() ? "Not set" : QString::fromStdString(config.outputDir)));
        vmlinuzLabel->setText(QString("vmlinuz Selected: %1").arg(config.vmlinuzPath.empty() ? "Not selected" : QString::fromStdString(config.vmlinuzPath)));
        mkinitcpioCheck->setChecked(config.mkinitcpioGenerated);
        grubCheck->setChecked(config.grubEdited);
    }

    void execute_command(const std::string& cmd, bool continueOnError = false) {
        console->append(QString("> %1").arg(QString::fromStdString(cmd)));

        QProcess process;
        process.start("bash", QStringList() << "-c" << QString::fromStdString(cmd));

        // If the command requires sudo, send the password
        if (cmd.find("sudo") != std::string::npos && !sudoPassword.isEmpty()) {
            process.write((sudoPassword + "\n").toUtf8());
            process.closeWriteChannel();
        }

        process.waitForFinished(-1);

        QString output = process.readAllStandardOutput();
        QString error = process.readAllStandardError();

        if (!output.isEmpty()) {
            console->append(output);
        }
        if (!error.isEmpty()) {
            console->append(error);
        }

        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            if (!continueOnError) {
                console->append("Error executing command!");
                QMessageBox::critical(this, "Error", QString("Failed to execute: %1").arg(QString::fromStdString(cmd)));
                return;
            } else {
                console->append("Command failed but continuing...");
            }
        }
    }

    void updateTimeThread() {
        while (timeThreadRunning) {
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char datetime[50];
            strftime(datetime, sizeof(datetime), "%d/%m/%Y %H:%M:%S", t);

            {
                std::lock_guard<std::mutex> lock(timeMutex);
                currentTimeStr = datetime;
            }
            sleep(1);
        }
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
            console->append("Failed to save configuration!");
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

    bool askPassword() {
        bool ok;
        QString password = QInputDialog::getText(this, "Authentication Required",
                                                 "Enter your sudo password:",
                                                 QLineEdit::Password,
                                                 "",
                                                 &ok);

        if (ok && !password.isEmpty()) {
            sudoPassword = password;

            // Verify the password works with a simple command
            QProcess testProcess;
            testProcess.start("sudo", QStringList() << "-S" << "true");
            testProcess.write((sudoPassword + "\n").toUtf8());
            testProcess.closeWriteChannel();

            if (!testProcess.waitForFinished() || testProcess.exitCode() != 0) {
                QMessageBox::warning(this, "Error", "Incorrect sudo password!");
                return false;
            }

            return true;
        }
        return false;
    }

private slots:
    void showGuide() {
        std::string readmePath = "/home/" + USERNAME + "/.config/cmi/readme.txt";
        execute_command("mkdir -p /home/" + USERNAME + "/.config/cmi", true);
        execute_command("nano " + readmePath, true);
    }

    void showSetupMenu() {
        if (!askPassword()) {
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle("ISO Creation Setup");
        dialog.resize(500, 400);

        QVBoxLayout *layout = new QVBoxLayout(&dialog);

        // Dependencies
        QPushButton *depsButton = new QPushButton("Install Dependencies", &dialog);
        connect(depsButton, &QPushButton::clicked, [this, &dialog]() {
            installDependencies();
            dialog.close();
        });
        layout->addWidget(depsButton);

        // ISO Tag
        QPushButton *isoTagButton = new QPushButton("Set ISO Tag", &dialog);
        connect(isoTagButton, &QPushButton::clicked, [this, &dialog]() {
            bool ok;
            QString text = QInputDialog::getText(&dialog, "Set ISO Tag",
                                                 "Enter ISO tag (e.g., 2025):", QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty()) {
                config.isoTag = text.toStdString();
                saveConfig();
            }
        });
        layout->addWidget(isoTagButton);

        // ISO Name
        QPushButton *isoNameButton = new QPushButton("Set ISO Name", &dialog);
        connect(isoNameButton, &QPushButton::clicked, [this, &dialog]() {
            bool ok;
            QString text = QInputDialog::getText(&dialog, "Set ISO Name",
                                                 "Enter ISO name (e.g., claudemods.iso):", QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty()) {
                config.isoName = text.toStdString();
                saveConfig();
            }
        });
        layout->addWidget(isoNameButton);

        // Output Directory
        QPushButton *outputDirButton = new QPushButton("Set Output Directory", &dialog);
        connect(outputDirButton, &QPushButton::clicked, [this, &dialog]() {
            QString dir = QFileDialog::getExistingDirectory(&dialog, "Select Output Directory",
                                                            QString::fromStdString(config.outputDir.empty() ?
                                                            "/home/" + USERNAME + "/Downloads" : config.outputDir));
            if (!dir.isEmpty()) {
                config.outputDir = dir.toStdString();
                saveConfig();
            }
        });
        layout->addWidget(outputDirButton);

        // vmlinuz
        QPushButton *vmlinuzButton = new QPushButton("Select vmlinuz", &dialog);
        connect(vmlinuzButton, &QPushButton::clicked, this, &MainWindow::selectVmlinuz);
        layout->addWidget(vmlinuzButton);

        // mkinitcpio
        QPushButton *mkinitcpioButton = new QPushButton("Generate mkinitcpio", &dialog);
        connect(mkinitcpioButton, &QPushButton::clicked, [this, &dialog]() {
            generateMkinitcpio();
            dialog.close();
        });
        layout->addWidget(mkinitcpioButton);

        // GRUB Config
        QPushButton *grubButton = new QPushButton("Edit GRUB Config", &dialog);
        connect(grubButton, &QPushButton::clicked, [this, &dialog]() {
            editGrubCfg();
            dialog.close();
        });
        layout->addWidget(grubButton);

        // Close button
        QPushButton *closeButton = new QPushButton("Close", &dialog);
        connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::close);
        layout->addWidget(closeButton);

        dialog.exec();
    }

    void installDependencies() {
        console->append("\nInstalling required dependencies...\n");

        std::string packages;
        for (const auto& pkg : DEPENDENCIES) {
            packages += pkg + " ";
        }

        std::string command = "sudo pacman -Sy --needed --noconfirm " + packages;
        execute_command(command);

        config.dependenciesInstalled = true;
        saveConfig();
        console->append("\nDependencies installed successfully!\n");
    }

    void selectVmlinuz() {
        DIR *dir;
        struct dirent *ent;
        std::vector<std::string> vmlinuzFiles;

        if ((dir = opendir("/boot")) != nullptr) {
            while ((ent = readdir(dir)) != nullptr) {
                std::string filename = ent->d_name;
                if (filename.find("vmlinuz") == 0) {
                    vmlinuzFiles.push_back("/boot/" + filename);
                }
            }
            closedir(dir);
        } else {
            console->append("Could not open /boot directory");
            return;
        }

        if (vmlinuzFiles.empty()) {
            console->append("No vmlinuz files found in /boot!");
            return;
        }

        QStringList items;
        for (const auto& file : vmlinuzFiles) {
            items.append(QString::fromStdString(file));
        }

        bool ok;
        QString item = QInputDialog::getItem(this, "Select vmlinuz",
                                             "Available vmlinuz files:", items, 0, false, &ok);
        if (ok && !item.isEmpty()) {
            config.vmlinuzPath = item.toStdString();

            std::string destPath = BUILD_DIR + "/boot/vmlinuz-x86_64";
            std::string copyCmd = "sudo cp " + config.vmlinuzPath + " " + destPath;
            execute_command(copyCmd);

            console->append(QString("Selected: %1").arg(item));
            console->append(QString("Copied to: %1").arg(QString::fromStdString(destPath)));
            saveConfig();
        }
    }

    void generateMkinitcpio() {
        if (config.vmlinuzPath.empty()) {
            console->append("Please select vmlinuz first!");
            return;
        }

        if (BUILD_DIR.empty()) {
            console->append("Build directory not set!");
            return;
        }

        console->append("Generating initramfs...");
        execute_command("cd " + BUILD_DIR + " && sudo mkinitcpio -c mkinitcpio.conf -g " + BUILD_DIR + "/boot/initramfs-x86_64.img");

        config.mkinitcpioGenerated = true;
        saveConfig();
        console->append("mkinitcpio generated successfully!");
    }

    void editGrubCfg() {
        if (BUILD_DIR.empty()) {
            console->append("Build directory not set!");
            return;
        }

        std::string grubCfgPath = BUILD_DIR + "/boot/grub/grub.cfg";
        console->append(QString("Editing GRUB config: %1").arg(QString::fromStdString(grubCfgPath)));

        std::string nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + grubCfgPath;
        execute_command(nanoCommand);

        config.grubEdited = true;
        saveConfig();
        console->append("GRUB config edited!");
    }

    void createImage() {
        if (!askPassword()) {
            return;
        }

        std::string outputDir = getOutputDirectory();
        std::string outputImgPath = outputDir + "/" + ORIG_IMG_NAME;

        execute_command("sudo umount " + MOUNT_POINT + " 2>/dev/null || true", true);
        execute_command("sudo rm -rf " + outputImgPath, true);
        execute_command("sudo mkdir -p " + MOUNT_POINT, true);
        execute_command("sudo mkdir -p " + outputDir, true);

        // Get size input
        bool ok;
        double sizeGB = QInputDialog::getDouble(this, "Image Size",
                                                "Enter the image size in GB (e.g., 1.2 for 1.2GB):", 1.0, 0.1, 100.0, 1, &ok);
        if (!ok) return;

        // Filesystem type
        QStringList fsOptions = {"btrfs", "ext4"};
        QString fsType = QInputDialog::getItem(this, "Filesystem Type",
                                               "Choose filesystem type:", fsOptions, 0, false, &ok);
        if (!ok) return;

        // Create image
        if (!createImageFile(std::to_string(sizeGB), outputImgPath) ||
            !formatFilesystem(fsType.toStdString(), outputImgPath) ||
            !mountFilesystem(fsType.toStdString(), outputImgPath, MOUNT_POINT) ||
            !copyFilesWithRsync(SOURCE_DIR, MOUNT_POINT, fsType.toStdString())) {
            return;
            }

            unmountAndCleanup(MOUNT_POINT);

        if (fsType == "ext4") {
            // For ext4, create SquashFS version
            std::string squashPath = outputDir + "/" + FINAL_IMG_NAME;
            createSquashFS(outputImgPath, squashPath);
        }

        createChecksum(outputImgPath);
        printFinalMessage(fsType.toStdString(), outputImgPath);
    }

    bool createImageFile(const std::string& sizeStr, const std::string& filename) {
        try {
            double sizeGB = std::stod(sizeStr);
            if (sizeGB <= 0) {
                console->append("Invalid size: must be greater than 0");
                return false;
            }

            size_t sizeBytes = static_cast<size_t>(sizeGB * 1073741824);

            console->append(QString("Creating image file of size %1GB (%2 bytes)...")
            .arg(sizeGB).arg(sizeBytes));

            std::string command = "sudo fallocate -l " + std::to_string(sizeBytes) + " " + filename;
            execute_command(command, true);

            return true;
        } catch (const std::exception& e) {
            console->append(QString("Error creating image file: %1")
            .arg(QString::fromStdString(e.what())));
            return false;
        }
    }

    bool formatFilesystem(const std::string& fsType, const std::string& filename) {
        if (fsType == "btrfs") {
            console->append(QString("Creating Btrfs filesystem with %1 compression")
            .arg(QString::fromStdString(BTRFS_COMPRESSION)));

            execute_command("sudo mkdir -p " + SOURCE_DIR + "/btrfs_rootdir", true);

            std::string command = "sudo mkfs.btrfs -L \"" + BTRFS_LABEL + "\" --compress=" + BTRFS_COMPRESSION +
            " --rootdir=" + SOURCE_DIR + "/btrfs_rootdir -f " + filename;
            execute_command(command, true);

            execute_command("sudo rmdir " + SOURCE_DIR + "/btrfs_rootdir", true);
        } else {
            console->append("Formatting as ext4 with SquashFS-style compression");
            std::string command = "sudo mkfs.ext4 -F -O ^has_journal,^resize_inode -E lazy_itable_init=0 -m 0 -L \"SYSTEM_BACKUP\" " + filename;
            execute_command(command, true);
        }
        return true;
    }

    bool mountFilesystem(const std::string& fsType, const std::string& filename, const std::string& mountPoint) {
        execute_command("sudo mkdir -p " + mountPoint, true);

        if (fsType == "btrfs") {
            std::string command = "sudo mount -o compress=" + BTRFS_COMPRESSION + ",compress-force=zstd:" + COMPRESSION_LEVEL + " " + filename + " " + mountPoint;
            execute_command(command, true);
        } else {
            std::string options = "loop,discard,noatime,data=writeback,commit=60,barrier=0,nobh,errors=remount-ro";
            std::string command = "sudo mount -o " + options + " " + filename + " " + mountPoint;
            execute_command(command, true);
        }
        return true;
    }

    bool copyFilesWithRsync(const std::string& source, const std::string& destination, const std::string& fsType) {
        std::string compressionFlag = (fsType == "btrfs") ? "--compress" : "";
        std::string compressionLevel = (fsType == "btrfs") ? "--compress-level=" + COMPRESSION_LEVEL : "";

        std::string command = "sudo rsync -aHAXSr --numeric-ids --info=progress2 " + compressionFlag + " " + compressionLevel +
        " --exclude=/etc/udev/rules.d/70-persistent-cd.rules "
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
        source + " " + destination;
        execute_command(command, true);

        if (fsType == "btrfs") {
            console->append("Optimizing compression...");
            execute_command("sudo btrfs filesystem defrag -r -v -c " + BTRFS_COMPRESSION + " " + destination, true);
            execute_command("sudo btrfs filesystem resize max " + destination, true);
        } else {
            console->append("Optimizing ext4 filesystem...");
            execute_command("sudo e4defrag " + destination, true);
            execute_command("sudo tune2fs -o journal_data_writeback " + destination.substr(0, destination.find(' ')), true);
        }

        return true;
    }

    bool unmountAndCleanup(const std::string& mountPoint) {
        sync();
        try {
            execute_command("sudo umount " + mountPoint, true);
            execute_command("sudo rmdir " + mountPoint, true);
        } catch (...) {
            return false;
        }
        return true;
    }

    bool createSquashFS(const std::string& inputFile, const std::string& outputFile) {
        console->append("Creating optimized SquashFS image...");

        std::string command = "sudo mksquashfs " + inputFile + " " + outputFile +
        " -comp " + SQUASHFS_COMPRESSION +
        " " + SQUASHFS_COMPRESSION_ARGS[0] + " " + SQUASHFS_COMPRESSION_ARGS[1] +
        " -noappend";

        execute_command(command, true);
        return true;
    }

    bool createChecksum(const std::string& filename) {
        std::string command = "md5sum " + filename + " > " + filename + ".md5";
        execute_command(command, true);
        return true;
    }

    void printFinalMessage(const std::string& fsType, const std::string& outputFile) {
        console->append("");
        if (fsType == "btrfs") {
            console->append(QString("Compressed BTRFS image created successfully: %1")
            .arg(QString::fromStdString(outputFile)));
        } else {
            console->append(QString("Compressed ext4 image created successfully: %1")
            .arg(QString::fromStdString(outputFile)));
        }
        console->append(QString("Checksum file: %1.md5")
        .arg(QString::fromStdString(outputFile)));

        QProcess process;
        process.start("du", QStringList() << "-h" << QString::fromStdString(outputFile));
        process.waitForFinished();
        QString size = process.readAllStandardOutput().split('\t').first();
        console->append(QString("Size: %1").arg(size));
    }

    void createISO() {
        if (!config.isReadyForISO()) {
            console->append("Cannot create ISO - setup is incomplete!");
            return;
        }

        console->append("\nStarting ISO creation process...\n");

        std::string expandedOutputDir = expandPath(config.outputDir);

        execute_command("mkdir -p " + expandedOutputDir, true);

        std::string xorrisoCmd = "sudo xorriso -as mkisofs "
        "--modification-date=\"$(date +%Y%m%d%H%M%S00)\" "
        "--protective-msdos-label "
        "-volid \"" + config.isoTag + "\" "
        "-appid \"claudemods Linux Live/Rescue CD\" "
        "-publisher \"claudemods claudemods101@gmail.com >\" "
        "-preparer \"Prepared by user\" "
        "-r -graft-points -no-pad "
        "--sort-weight 0 / "
        "--sort-weight 1 /boot "
        "--grub2-mbr " + BUILD_DIR + "/boot/grub/i386-pc/boot_hybrid.img "
        "-partition_offset 16 "
        "-b boot/grub/i386-pc/eltorito.img "
        "-c boot.catalog "
        "-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info "
        "-eltorito-alt-boot "
        "-append_partition 2 0xef " + BUILD_DIR + "/boot/efi.img "
        "-e --interval:appended_partition_2:all:: "
        "-no-emul-boot "
        "-iso-level 3 "
        "-o \"" + expandedOutputDir + "/" + config.isoName + "\" " +
        BUILD_DIR;

        execute_command(xorrisoCmd, true);
        console->append(QString("ISO created successfully at %1/%2")
        .arg(QString::fromStdString(expandedOutputDir))
        .arg(QString::fromStdString(config.isoName)));
    }

    void showDiskUsage() {
        execute_command("df -h");
    }

    void installISOToUSB() {
        if (config.outputDir.empty()) {
            console->append("Output directory not set!");
            return;
        }

        DIR *dir;
        struct dirent *ent;
        std::vector<std::string> isoFiles;

        std::string expandedOutputDir = expandPath(config.outputDir);
        if ((dir = opendir(expandedOutputDir.c_str())) != nullptr) {
            while ((ent = readdir(dir)) != nullptr) {
                std::string filename = ent->d_name;
                if (filename.find(".iso") != std::string::npos) {
                    isoFiles.push_back(filename);
                }
            }
            closedir(dir);
        } else {
            console->append(QString("Could not open output directory: %1")
            .arg(QString::fromStdString(expandedOutputDir)));
            return;
        }

        if (isoFiles.empty()) {
            console->append("No ISO files found in output directory!");
            return;
        }

        QStringList items;
        for (const auto& file : isoFiles) {
            items.append(QString::fromStdString(file));
        }

        bool ok;
        QString isoFile = QInputDialog::getItem(this, "Select ISO",
                                                "Available ISO files:", items, 0, false, &ok);
        if (!ok || isoFile.isEmpty()) return;

        std::string selectedISO = expandedOutputDir + "/" + isoFile.toStdString();

        console->append("\nAvailable drives:");
        execute_command("lsblk -d -o NAME,SIZE,MODEL | grep -v 'loop'", true);

        QString targetDrive = QInputDialog::getText(this, "Target Drive",
                                                    "Enter target drive (e.g., /dev/sda):", QLineEdit::Normal, "", &ok);
        if (!ok || targetDrive.isEmpty()) {
            console->append("No drive specified!");
            return;
        }

        QMessageBox::StandardButton confirm = QMessageBox::warning(this, "Warning",
                                                                   QString("WARNING: This will overwrite all data on %1!").arg(targetDrive),
                                                                   QMessageBox::Ok | QMessageBox::Cancel);
        if (confirm != QMessageBox::Ok) {
            console->append("Operation cancelled.");
            return;
        }

        console->append(QString("\nWriting %1 to %2...")
        .arg(QString::fromStdString(selectedISO))
        .arg(targetDrive));

        QProgressDialog progress("Writing ISO to USB...", "Cancel", 0, 0, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setCancelButton(nullptr); // No cancel button for this operation

        QProcess ddProcess;
        ddProcess.start("dd", QStringList()
        << "if=" + QString::fromStdString(selectedISO)
        << "of=" + targetDrive
        << "bs=4M"
        << "status=progress"
        << "oflag=sync");

        while (ddProcess.state() == QProcess::Running) {
            QCoreApplication::processEvents();
            QThread::msleep(100);
        }

        progress.close();

        if (ddProcess.exitStatus() == QProcess::NormalExit && ddProcess.exitCode() == 0) {
            console->append("\nISO successfully written to USB drive!");
        } else {
            console->append("\nFailed to write ISO to USB drive!");
        }
    }

    void runCMIInstaller() {
        execute_command("cmirsyncinstaller", true);
    }

    void updateScript() {
        console->append("\nUpdating script from GitHub...");
        execute_command("bash -c \"$(curl -fsSL https://raw.githubusercontent.com/claudemods/claudemods-multi-iso-konsole-script/main/advancedimgscript/installer/patch.sh)\"");
        console->append("\nScript updated successfully!");
    }

    std::string getOutputDirectory() {
        std::string dir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img/LiveOS";
        return dir;
    }

    std::string expandPath(const std::string& path) {
        std::string result = path;
        size_t pos;
        if ((pos = result.find("~")) != std::string::npos) {
            const char* home = getenv("HOME");
            if (home) result.replace(pos, 1, home);
        }
        if ((pos = result.find("$USER")) != std::string::npos) {
            result.replace(pos, 5, USERNAME);
        }
        return result;
    }

private:
    // UI Elements
    QLabel *bannerLabel;
    QLabel *timeLabel;
    QLabel *diskInfoLabel;
    QGroupBox *configGroup;
    QCheckBox *dependenciesCheck;
    QLabel *isoTagLabel;
    QLabel *isoNameLabel;
    QLabel *outputDirLabel;
    QLabel *vmlinuzLabel;
    QCheckBox *mkinitcpioCheck;
    QCheckBox *grubCheck;
    QTextEdit *console;

    // Thread management
    std::thread timeThread;
    std::atomic<bool> timeThreadRunning{true};
    std::mutex timeMutex;
    std::string currentTimeStr;

    // Password storage
    QString sudoPassword;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Set application style and palette
    app.setStyle("Fusion");

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(25, 25, 25));
    palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(42, 130, 218));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(palette);

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}

#include "main.moc"
