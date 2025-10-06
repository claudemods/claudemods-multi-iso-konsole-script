anlyze this script #include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QTimer>
#include <QProcess>
#include <QMessageBox>
#include <QScrollBar>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QDir>
#include <QSettings>
#include <QDateTime>
#include <QResource>
#include <QFile>
#include <QThread>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QComboBox>
#include <QKeyEvent>
#include <QDebug>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
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
#include <QTemporaryFile>

// Forward declarations
void saveConfig();
void execute_command(const std::string& cmd, bool continueOnError = false);

// Time-related globals
std::atomic<bool> time_thread_running(true);
std::mutex time_mutex;
std::string current_time_str;
bool should_reset = false;

// Constants
const std::string ORIG_IMG_NAME = "rootfs1.img";
const std::string FINAL_IMG_NAME = "rootfs.img";
std::string MOUNT_POINT = "/mnt/ext4_temp";
const std::string SOURCE_DIR = "/";
const std::string COMPRESSION_LEVEL = "22";
const std::string SQUASHFS_COMPRESSION = "zstd";
const std::vector<std::string> SQUASHFS_COMPRESSION_ARGS = {"-Xcompression-level", "22"};
std::string BUILD_DIR = "";
std::string USERNAME = "";

// Password storage (in-memory only)
std::string SUDO_PASSWORD = "";
bool PASSWORD_VALIDATED = false;

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
    "btrfs-progs",
    "e2fsprogs",
    "f2fs-tools",
    "xfsprogs",
    "xfsdump",
    "qt5-tools"
};

// Configuration state
struct ConfigState {
    std::string isoTag;
    std::string isoName;
    std::string outputDir;
    std::string vmlinuzPath;
    std::string cloneDir;
    bool mkinitcpioGenerated = false;
    bool grubEdited = false;
    bool bootTextEdited = false;
    bool calamaresBrandingEdited = false;
    bool calamares1Edited = false;
    bool calamares2Edited = false;
    bool dependenciesInstalled = false;

    bool isReadyForISO() const {
        return !isoTag.empty() && !isoName.empty() && !outputDir.empty() &&
        !vmlinuzPath.empty() && mkinitcpioGenerated && grubEdited && dependenciesInstalled;
    }

    bool allCheckboxesChecked() const {
        return dependenciesInstalled && !isoTag.empty() && !isoName.empty() &&
        !outputDir.empty() && !vmlinuzPath.empty() && !cloneDir.empty() &&
        mkinitcpioGenerated && grubEdited && bootTextEdited &&
        calamaresBrandingEdited && calamares1Edited && calamares2Edited;
    }
} config;

// Password Dialog Class
class PasswordDialog : public QDialog {
    Q_OBJECT

public:
    PasswordDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Sudo Password Required");
        setModal(true);
        setFixedSize(300, 150);

        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *label = new QLabel("Enter your sudo password:");
        layout->addWidget(label);

        passwordEdit = new QLineEdit();
        passwordEdit->setEchoMode(QLineEdit::Password);
        layout->addWidget(passwordEdit);

        QHBoxLayout *buttonLayout = new QHBoxLayout();
        QPushButton *okButton = new QPushButton("OK");
        QPushButton *cancelButton = new QPushButton("Cancel");

        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);
        layout->addLayout(buttonLayout);

        connect(okButton, &QPushButton::clicked, this, &PasswordDialog::onOkClicked);
        connect(cancelButton, &QPushButton::clicked, this, &PasswordDialog::reject);
        connect(passwordEdit, &QLineEdit::returnPressed, this, &PasswordDialog::onOkClicked);
    }

    QString getPassword() const {
        return password;
    }

private slots:
    void onOkClicked() {
        password = passwordEdit->text();
        accept();
    }

private:
    QLineEdit *passwordEdit;
    QString password;
};

// Username Dialog Class
class UsernameDialog : public QDialog {
    Q_OBJECT

public:
    UsernameDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Enter Username");
        setModal(true);
        setFixedSize(350, 150);

        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *label = new QLabel("Please enter your current username:");
        layout->addWidget(label);

        usernameEdit = new QLineEdit();

        // Try to get current username as default
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            usernameEdit->setText(pw->pw_name);
        }

        layout->addWidget(usernameEdit);

        QLabel *infoLabel = new QLabel("This will be used for all home directory paths");
        infoLabel->setStyleSheet("color: gray; font-size: 10px;");
        layout->addWidget(infoLabel);

        QHBoxLayout *buttonLayout = new QHBoxLayout();
        QPushButton *okButton = new QPushButton("OK");
        QPushButton *cancelButton = new QPushButton("Cancel");

        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);
        layout->addLayout(buttonLayout);

        connect(okButton, &QPushButton::clicked, this, &UsernameDialog::onOkClicked);
        connect(cancelButton, &QPushButton::clicked, this, &UsernameDialog::reject);
        connect(usernameEdit, &QLineEdit::returnPressed, this, &UsernameDialog::onOkClicked);
    }

    QString getUsername() const {
        return username;
    }

private slots:
    void onOkClicked() {
        username = usernameEdit->text().trimmed();
        if (username.isEmpty()) {
            QMessageBox::warning(this, "Invalid Username", "Username cannot be empty!");
            return;
        }
        accept();
    }

private:
    QLineEdit *usernameEdit;
    QString username;
};

// Function to request sudo password
bool requestSudoPassword() {
    PasswordDialog dialog;
    if (dialog.exec() == QDialog::Accepted) {
        SUDO_PASSWORD = dialog.getPassword().toStdString();

        // Validate password by testing sudo
        QProcess process;
        process.start("sudo", QStringList() << "-S" << "echo" << "password_valid");
        process.write(SUDO_PASSWORD.c_str());
        process.write("\n");
        process.closeWriteChannel();
        process.waitForFinished(5000);

        if (process.exitCode() == 0) {
            PASSWORD_VALIDATED = true;
            return true;
        } else {
            QMessageBox::critical(nullptr, "Error", "Invalid password or sudo access denied!");
            SUDO_PASSWORD.clear();
            PASSWORD_VALIDATED = false;
            return false;
        }
    }
    return false;
}

// Function to request username
bool requestUsername() {
    UsernameDialog dialog;
    if (dialog.exec() == QDialog::Accepted) {
        USERNAME = dialog.getUsername().toStdString();

        // Validate username by checking if home directory exists
        std::string homeDir = "/home/" + USERNAME;
        struct stat info;
        if (stat(homeDir.c_str(), &info) != 0) {
            QMessageBox::critical(nullptr, "Error",
                                  QString("Home directory for user '%1' does not exist!\nPlease enter a valid username.")
                                  .arg(QString::fromStdString(USERNAME)));
            USERNAME.clear();
            return false;
        } else if (!(info.st_mode & S_IFDIR)) {
            QMessageBox::critical(nullptr, "Error",
                                  QString("Path '/home/%1' is not a directory!\nPlease enter a valid username.")
                                  .arg(QString::fromStdString(USERNAME)));
            USERNAME.clear();
            return false;
        }

        // Update BUILD_DIR with the actual username
        BUILD_DIR = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";

        QMessageBox::information(nullptr, "Success",
                                 QString("Username set to: %1\nBuild directory: %2")
                                 .arg(QString::fromStdString(USERNAME), QString::fromStdString(BUILD_DIR)));
        return true;
    }
    return false;
}

// Enhanced execute_command with proper password handling
void execute_command(const std::string& cmd, bool continueOnError) {
    // Check if command requires sudo
    bool requiresSudo = (cmd.find("sudo") == 0);

    if (requiresSudo && !PASSWORD_VALIDATED) {
        if (!requestSudoPassword()) {
            if (!continueOnError) {
                QMessageBox::critical(nullptr, "Error", "Sudo password required but not provided!");
                exit(1);
            }
            return;
        }
    }

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);

    if (requiresSudo && PASSWORD_VALIDATED) {
        // Extract the actual command without sudo
        std::string actualCmd = cmd.substr(5); // Remove "sudo "
        process.start("sudo", QStringList() << "-S" << "sh" << "-c" << QString::fromStdString(actualCmd));
        process.write(SUDO_PASSWORD.c_str());
        process.write("\n");
    } else {
        process.start("sh", QStringList() << "-c" << QString::fromStdString(cmd));
    }

    process.closeWriteChannel();

    if (!process.waitForStarted()) {
        QMessageBox::critical(nullptr, "Error", QString("Failed to start command: %1").arg(QString::fromStdString(cmd)));
        if (!continueOnError) exit(1);
        return;
    }

    process.waitForFinished(-1);

    if (process.exitStatus() != QProcess::NormalExit || (process.exitCode() != 0 && !continueOnError)) {
        QMessageBox::critical(nullptr, "Error", QString("Error executing: %1").arg(QString::fromStdString(cmd)));
        if (!continueOnError) exit(1);
    } else if (process.exitCode() != 0) {
        QMessageBox::warning(nullptr, "Warning", QString("Command failed but continuing: %1").arg(QString::fromStdString(cmd)));
    }
}

// Function to extract embedded zip using Qt resource system
bool extractEmbeddedZip() {
    try {
        std::string configDir = "/home/" + USERNAME + "/.config/cmi";
        std::string zipPath = configDir + "/build-image-arch-img.zip";
        std::string extractPath = configDir;

        // Create config directory
        execute_command("mkdir -p " + configDir, true);

        // Use Qt resource system to access embedded zip
        QFile embeddedZip(":/zip/build-image-arch-img.zip");
        if (!embeddedZip.exists()) {
            return false;
        }

        // Copy embedded resource to filesystem
        if (!embeddedZip.copy(QString::fromStdString(zipPath))) {
            return false;
        }

        // Set proper permissions
        QFile::setPermissions(QString::fromStdString(zipPath), QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

        // Extract zip file
        std::string extractCmd = "unzip -o " + zipPath + " -d " + extractPath;
        execute_command(extractCmd, true);

        // Clean up zip file
        execute_command("rm -f " + zipPath, true);

        return true;
    } catch (...) {
        return false;
    }
}

// Function to extract Calamares resources
bool extractCalamaresResources() {
    try {
        std::string configDir = "/home/" + USERNAME + "/.config/cmi";
        std::string calamaresDir = configDir + "/calamares-files";

        // Create calamares directory
        execute_command("mkdir -p " + calamaresDir, true);

        // Step 1: Extract calamares.zip to cmi folder
        std::string calamaresZipPath = configDir + "/calamares.zip";
        QFile embeddedCalamaresZip(":/zip/calamares.zip");
        if (!embeddedCalamaresZip.exists()) {
            return false;
        }

        // Copy calamares.zip to filesystem
        if (!embeddedCalamaresZip.copy(QString::fromStdString(calamaresZipPath))) {
            return false;
        }

        // Set proper permissions
        QFile::setPermissions(QString::fromStdString(calamaresZipPath), QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

        // Extract calamares.zip
        std::string extractCalamaresCmd = "unzip -o " + calamaresZipPath + " -d " + configDir;
        execute_command(extractCalamaresCmd, true);

        // Clean up calamares.zip
        execute_command("rm -f " + calamaresZipPath, true);

        // Step 2: Extract branding.zip to cmi/calamares
        std::string brandingZipPath = configDir + "/branding.zip";
        QFile embeddedBrandingZip(":/zip/branding.zip");
        if (!embeddedBrandingZip.exists()) {
            return false;
        }

        // Copy branding.zip to filesystem
        if (!embeddedBrandingZip.copy(QString::fromStdString(brandingZipPath))) {
            return false;
        }

        // Set proper permissions
        QFile::setPermissions(QString::fromStdString(brandingZipPath), QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

        // Extract branding.zip to calamares directory
        std::string extractBrandingCmd = "unzip -o " + brandingZipPath + " -d " + calamaresDir;
        execute_command(extractBrandingCmd, true);

        // Clean up branding.zip
        execute_command("rm -f " + brandingZipPath, true);

        // Step 3: Copy extras.zip to cmi/calamares
        std::string extrasZipPath = calamaresDir + "/extras.zip";
        QFile embeddedExtrasZip(":/zip/extras.zip");
        if (!embeddedExtrasZip.exists()) {
            return false;
        }

        // Copy extras.zip to calamares directory
        if (!embeddedExtrasZip.copy(QString::fromStdString(extrasZipPath))) {
            return false;
        }

        // Set proper permissions
        QFile::setPermissions(QString::fromStdString(extrasZipPath), QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

        return true;
    } catch (...) {
        return false;
    }
}

void update_time_thread() {
    while (time_thread_running) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char datetime[50];
        strftime(datetime, sizeof(datetime), "%d/%m/%Y %H:%M:%S", t);

        {
            std::lock_guard<std::mutex> lock(time_mutex);
            current_time_str = datetime;
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
        configFile << "cloneDir=" << config.cloneDir << "\n";
        configFile << "mkinitcpioGenerated=" << (config.mkinitcpioGenerated ? "1" : "0") << "\n";
        configFile << "grubEdited=" << (config.grubEdited ? "1" : "0") << "\n";
        configFile << "bootTextEdited=" << (config.bootTextEdited ? "1" : "0") << "\n";
        configFile << "calamaresBrandingEdited=" << (config.calamaresBrandingEdited ? "1" : "0") << "\n";
        configFile << "calamares1Edited=" << (config.calamares1Edited ? "1" : "0") << "\n";
        configFile << "calamares2Edited=" << (config.calamares2Edited ? "1" : "0") << "\n";
        configFile << "dependenciesInstalled=" << (config.dependenciesInstalled ? "1" : "0") << "\n";
        configFile.close();
    } else {
        QMessageBox::critical(nullptr, "Error", QString("Failed to save configuration to %1").arg(QString::fromStdString(configPath)));
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
                else if (key == "cloneDir") config.cloneDir = value;
                else if (key == "mkinitcpioGenerated") config.mkinitcpioGenerated = (value == "1");
                else if (key == "grubEdited") config.grubEdited = (value == "1");
                else if (key == "bootTextEdited") config.bootTextEdited = (value == "1");
                else if (key == "calamaresBrandingEdited") config.calamaresBrandingEdited = (value == "1");
                else if (key == "calamares1Edited") config.calamares1Edited = (value == "1");
                else if (key == "calamares2Edited") config.calamares2Edited = (value == "1");
                else if (key == "dependenciesInstalled") config.dependenciesInstalled = (value == "1");
            }
        }
        configFile.close();
    }
}

// Consolidated rsync command
std::string getRsyncCommand(const std::string& source, const std::string& destination) {
    return "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
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
    "--exclude=clone_system_temp "
    "--include=dev "
    "--include=proc "
    "--include=tmp "
    "--include=sys "
    "--include=run "
    "--include=dev "
    "--include=proc "
    "--include=tmp "
    "--include=sys "
    "--include=usr " +
    source + "/ " + destination + "/";
}

bool copyFilesWithRsync(const std::string& source, const std::string& destination) {
    std::string command = getRsyncCommand(source, destination);
    execute_command(command, true);
    return true;
}

bool createSquashFS(const std::string& inputDir, const std::string& outputFile) {
    std::string command = "sudo mksquashfs " + inputDir + " " + outputFile +
    " -noappend -comp xz -b 256K -Xbcj x86";

    execute_command(command, true);
    return true;
}

bool createChecksum(const std::string& filename) {
    std::string command = "sha512sum " + filename + " > " + filename + ".sha512";
    execute_command(command, true);
    return true;
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

// Consolidated ISO creation command
std::string getXorrisoCommand(const std::string& buildDir, const std::string& outputPath) {
    return "sudo xorriso -as mkisofs "
    "--modification-date=\"$(date +%Y%m%d%H%M%S00)\" "
    "--protective-msdos-label "
    "-volid \"" + config.isoTag + "\" "
    "-appid \"claudemods Linux Live/Rescue CD\" "
    "-publisher \"claudemods claudemods101@gmail.com >\" "
    "-preparer \"Prepared by user\" "
    "-r -graft-points -no-pad "
    "--sort-weight 0 / "
    "--sort-weight 1 /boot "
    "--grub2-mbr " + buildDir + "/boot/grub/i386-pc/boot_hybrid.img "
    "-partition_offset 16 "
    "-b boot/grub/i386-pc/eltorito.img "
    "-c boot.catalog "
    "-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info "
    "-eltorito-alt-boot "
    "-append_partition 2 0xef " + buildDir + "/boot/efi.img "
    "-e --interval:appended_partition_2:all:: "
    "-no-emul-boot "
    "-iso-level 3 "
    "-o \"" + outputPath + "\" " +
    buildDir;
}

bool createISO() {
    if (!config.allCheckboxesChecked()) {
        QMessageBox::critical(nullptr, "Error", "Cannot create ISO - all setup steps must be completed first!");
        return false;
    }

    if (!config.isReadyForISO()) {
        QMessageBox::critical(nullptr, "Error", "Cannot create ISO - setup is incomplete!");
        return false;
    }

    std::string expandedOutputDir = expandPath(config.outputDir);
    execute_command("mkdir -p " + expandedOutputDir, true);

    // FIXED: Build directory uses actual username
    std::string buildDir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";
    std::string outputPath = expandedOutputDir + "/" + config.isoName;

    std::string xorrisoCmd = getXorrisoCommand(buildDir, outputPath);
    execute_command(xorrisoCmd, true);

    // Change ownership of the created ISO to the current user
    std::string chownCmd = "sudo chown " + USERNAME + ":" + USERNAME + " \"" + outputPath + "\"";
    execute_command(chownCmd, true);

    QMessageBox::information(nullptr, "Success", QString("ISO created successfully at %1").arg(QString::fromStdString(outputPath)));
    return true;
}

// Worker Thread Class
class WorkerThread : public QThread {
    Q_OBJECT
public:
    explicit WorkerThread(QObject *parent = nullptr) : QThread(parent) {}

    void executeCommand(const QString &cmd, bool continueOnError = false) {
        m_command = cmd;
        m_continueOnError = continueOnError;
        start();
    }

signals:
    void commandOutput(const QString &output);
    void commandFinished(bool success);
    void progressUpdate(int value);

protected:
    void run() override {
        try {
            QProcess process;
            process.setProcessChannelMode(QProcess::MergedChannels);

            connect(&process, &QProcess::readyReadStandardOutput, [&]() {
                QString output = process.readAllStandardOutput();
                emit commandOutput(output);
            });

            // Check if command requires sudo
            bool requiresSudo = m_command.contains("sudo");

            if (requiresSudo && !PASSWORD_VALIDATED) {
                emit commandOutput("Error: Sudo password required but not available in worker thread!\n");
                emit commandFinished(false);
                return;
            }

            if (requiresSudo && PASSWORD_VALIDATED) {
                // Extract the actual command without sudo
                QString actualCmd = m_command.mid(5); // Remove "sudo "
                process.start("sudo", QStringList() << "-S" << "sh" << "-c" << actualCmd);
                process.write(SUDO_PASSWORD.c_str());
                process.write("\n");
            } else {
                process.start("/bin/bash", QStringList() << "-c" << m_command);
            }

            process.closeWriteChannel();

            if (!process.waitForStarted()) {
                emit commandOutput("Error: Failed to start command: " + m_command + "\n");
                emit commandFinished(false);
                return;
            }

            while (process.waitForReadyRead(-1)) {
                // Output is handled by the signal above
            }

            process.waitForFinished(-1);

            bool success = (process.exitStatus() == QProcess::NormalExit &&
            (process.exitCode() == 0 || m_continueOnError));

            emit commandFinished(success);
        } catch (...) {
            emit commandOutput("Error: Exception occurred in worker thread!\n");
            emit commandFinished(false);
        }
    }

private:
    QString m_command;
    bool m_continueOnError;
};

// Main Window Class
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        // Get username first
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            USERNAME = pw->pw_name;
        } else {
            QMessageBox::critical(nullptr, "Error", "Failed to get username!");
            return;
        }

        // Set build directory
        BUILD_DIR = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";

        // Create config directory
        std::string configDir = "/home/" + USERNAME + "/.config/cmi";
        execute_command("mkdir -p " + configDir, true);

        setupUI();
        loadConfig();
        startTimeThread();

        // Request sudo password FIRST on startup, then username, then check for updates
        QTimer::singleShot(100, this, &MainWindow::requestSudoOnStartup);
    }

    ~MainWindow() {
        time_thread_running = false;
        if (time_thread.joinable()) {
            time_thread.join();
        }
    }

private slots:
    void requestSudoOnStartup() {
        logOutputText("=== SYSTEM STARTUP ===\n", "cyan");
        logOutputText("Requesting sudo password for system operations...\n", "cyan");

        if (!requestSudoPassword()) {
            logOutputText("Sudo password is required for most operations. You will be prompted when needed.\n", "yellow");
            // Continue without sudo for now, but still ask for username
            QTimer::singleShot(500, this, &MainWindow::requestUsernameOnStartup);
        } else {
            logOutputText("Sudo password validated successfully!\n", "green");
            // Now ask for username after sudo validation
            QTimer::singleShot(500, this, &MainWindow::requestUsernameOnStartup);
        }
    }

    void requestUsernameOnStartup() {
        logOutputText("Requesting username for home directory paths...\n", "cyan");

        if (!requestUsername()) {
            logOutputText("Username is required for proper operation. Using system detected username: " + QString::fromStdString(USERNAME) + "\n", "yellow");
            // Continue with system-detected username
            QTimer::singleShot(500, this, &MainWindow::checkForUpdates);
        } else {
            logOutputText("Username set successfully to: " + QString::fromStdString(USERNAME) + "\n", "green");
            // Now check for updates after username is set
            QTimer::singleShot(500, this, &MainWindow::checkForUpdates);
        }
    }

    void onInstallDependencies() {
        if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
            logOutputText("Sudo password required for installing dependencies!\n", "red");
            return;
        }

        logOutputText("Installing dependencies...\n", "cyan");
        progressBar->setValue(10);

        worker->executeCommand("sudo pacman -Sy", true);

        std::string packages;
        for (const auto& pkg : DEPENDENCIES) {
            packages += pkg + " ";
        }

        std::string command = "sudo pacman -S --needed --noconfirm " + packages;
        worker->executeCommand(QString::fromStdString(command), true);

        config.dependenciesInstalled = true;
        saveConfig();
        updateConfigStatus();
    }

    void onSetCloneDir() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Clone Directory",
                                                        QDir::homePath());
        if (!dir.isEmpty()) {
            config.cloneDir = dir.toStdString() + "/clone_system_temp";
            worker->executeCommand("sudo mkdir -p " + QString::fromStdString(config.cloneDir), true);
            logOutputText("Clone directory set to: " + QString::fromStdString(config.cloneDir) + "\n", "green");
            saveConfig();
            updateConfigStatus();
        }
    }

    void onSetIsoTag() {
        bool ok;
        QString tag = QInputDialog::getText(this, "ISO Tag", "Enter ISO tag (e.g., 2025):",
                                            QLineEdit::Normal, QString::fromStdString(config.isoTag), &ok);
        if (ok && !tag.isEmpty()) {
            config.isoTag = tag.toStdString();
            logOutputText("ISO tag set to: " + tag + "\n", "green");
            saveConfig();
            updateConfigStatus();
        }
    }

    void onSetIsoName() {
        bool ok;
        QString name = QInputDialog::getText(this, "ISO Name", "Enter ISO name (e.g., claudemods.iso):",
                                             QLineEdit::Normal, QString::fromStdString(config.isoName), &ok);
        if (ok && !name.isEmpty()) {
            config.isoName = name.toStdString();
            logOutputText("ISO name set to: " + name + "\n", "green");
            saveConfig();
            updateConfigStatus();
        }
    }

    void onSetOutputDir() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory",
                                                        QDir::homePath() + "/Downloads");
        if (!dir.isEmpty()) {
            config.outputDir = dir.toStdString();
            worker->executeCommand("mkdir -p " + dir, true);
            logOutputText("Output directory set to: " + dir + "\n", "green");
            saveConfig();
            updateConfigStatus();
        }
    }

    void onSelectVmlinuz() {
        QStringList vmlinuzFiles;
        QDir bootDir("/boot");
        QStringList filters;
        filters << "vmlinuz*";
        bootDir.setNameFilters(filters);

        QStringList files = bootDir.entryList(QDir::Files);
        for (const QString &file : files) {
            vmlinuzFiles << "/boot/" + file;
        }

        if (vmlinuzFiles.isEmpty()) {
            logOutputText("No vmlinuz files found in /boot!\n", "red");
            return;
        }

        bool ok;
        QString selected = QInputDialog::getItem(this, "Select vmlinuz",
                                                 "Choose vmlinuz file:", vmlinuzFiles, 0, false, &ok);
        if (ok && !selected.isEmpty()) {
            config.vmlinuzPath = selected.toStdString();

            // FIXED: Destination path uses actual username
            std::string destPath = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img/boot/vmlinuz-x86_64";

            if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
                logOutputText("Sudo password required for copying vmlinuz!\n", "red");
                return;
            }

            worker->executeCommand("sudo cp " + selected + " " + QString::fromStdString(destPath), true);
            logOutputText("Selected vmlinuz: " + selected + "\n", "green");
            logOutputText("Copied to: " + QString::fromStdString(destPath) + "\n", "cyan");
            saveConfig();
            updateConfigStatus();
        }
    }

    void onGenerateMkinitcpio() {
        if (config.vmlinuzPath.empty()) {
            logOutputText("Please select vmlinuz first!\n", "red");
            return;
        }

        if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
            logOutputText("Sudo password required for generating mkinitcpio!\n", "red");
            return;
        }

        logOutputText("Generating initramfs using geninit.sh script...\n", "cyan");
        progressBar->setValue(30);

        // FIXED: Proper string concatenation using std::string
        std::string geninitScript = "/home/" + USERNAME + "/.config/cmi/geninit.sh";

        logOutputText("Executing script: " + QString::fromStdString(geninitScript) + "\n", "cyan");
        worker->executeCommand("sudo bash " + QString::fromStdString(geninitScript), true);

        config.mkinitcpioGenerated = true;
        saveConfig();
        updateConfigStatus();
        logOutputText("mkinitcpio generated successfully using geninit.sh script!\n", "green");
    }

    void onEditGrubCfg() {
        // FIXED: GRUB config path uses actual username
        std::string buildDir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";
        std::string grubCfgPath = buildDir + "/boot/grub/grub.cfg";

        logOutputText("Editing GRUB config: " + QString::fromStdString(grubCfgPath) + "\n", "cyan");

        QDialog editorDialog(this);
        editorDialog.setWindowTitle("Edit GRUB Configuration");
        editorDialog.setMinimumSize(600, 400);

        QVBoxLayout layout(&editorDialog);
        QTextEdit textEdit;
        QPushButton saveButton("Save");
        QPushButton cancelButton("Cancel");

        // Read current grub.cfg content
        QFile file(QString::fromStdString(grubCfgPath));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            textEdit.setPlainText(file.readAll());
            file.close();
        }

        QHBoxLayout buttonLayout;
        buttonLayout.addWidget(&saveButton);
        buttonLayout.addWidget(&cancelButton);

        layout.addWidget(&textEdit);
        layout.addLayout(&buttonLayout);

        connect(&saveButton, &QPushButton::clicked, [&]() {
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(textEdit.toPlainText().toUtf8());
                file.close();
                logOutputText("GRUB configuration saved successfully!\n", "green");
                config.grubEdited = true;
                saveConfig();
                updateConfigStatus();
            } else {
                logOutputText("Failed to save GRUB configuration!\n", "red");
            }
            editorDialog.accept();
        });

        connect(&cancelButton, &QPushButton::clicked, &editorDialog, &QDialog::reject);

        editorDialog.exec();
    }

    void onEditBootText() {
        // FIXED: Boot text path uses actual username
        std::string buildDir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";
        std::string bootTextPath = buildDir + "/boot/grub/kernels.cfg";

        logOutputText("Editing Boot Text: " + QString::fromStdString(bootTextPath) + "\n", "cyan");

        QDialog editorDialog(this);
        editorDialog.setWindowTitle("Edit Boot Text");
        editorDialog.setMinimumSize(600, 400);

        QVBoxLayout layout(&editorDialog);
        QTextEdit textEdit;
        QPushButton saveButton("Save");
        QPushButton cancelButton("Cancel");

        // Read current kernels.cfg content
        QFile file(QString::fromStdString(bootTextPath));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            textEdit.setPlainText(file.readAll());
            file.close();
        }

        QHBoxLayout buttonLayout;
        buttonLayout.addWidget(&saveButton);
        buttonLayout.addWidget(&cancelButton);

        layout.addWidget(&textEdit);
        layout.addLayout(&buttonLayout);

        connect(&saveButton, &QPushButton::clicked, [&]() {
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(textEdit.toPlainText().toUtf8());
                file.close();
                logOutputText("Boot text saved successfully!\n", "green");
                config.bootTextEdited = true;
                saveConfig();
                updateConfigStatus();
            } else {
                logOutputText("Failed to save boot text!\n", "red");
            }
            editorDialog.accept();
        });

        connect(&cancelButton, &QPushButton::clicked, &editorDialog, &QDialog::reject);

        editorDialog.exec();
    }

    void onEditCalamaresBranding() {
        QString calamaresBrandingPath = "/usr/share/calamares/branding/claudemods/branding.desc";
        logOutputText("Editing Calamares Branding: " + calamaresBrandingPath + "\n", "cyan");

        if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
            logOutputText("Sudo password required for editing Calamares branding!\n", "red");
            return;
        }

        QDialog editorDialog(this);
        editorDialog.setWindowTitle("Edit Calamares Branding");
        editorDialog.setMinimumSize(600, 400);

        QVBoxLayout layout(&editorDialog);
        QTextEdit textEdit;
        QPushButton saveButton("Save");
        QPushButton cancelButton("Cancel");

        // Read current branding.desc content
        QProcess process;
        process.start("sudo", QStringList() << "cat" << calamaresBrandingPath);
        process.write(SUDO_PASSWORD.c_str());
        process.write("\n");
        process.closeWriteChannel();
        process.waitForFinished();
        if (process.exitCode() == 0) {
            textEdit.setPlainText(process.readAllStandardOutput());
        }

        QHBoxLayout buttonLayout;
        buttonLayout.addWidget(&saveButton);
        buttonLayout.addWidget(&cancelButton);

        layout.addWidget(&textEdit);
        layout.addLayout(&buttonLayout);

        connect(&saveButton, &QPushButton::clicked, [&]() {
            // Create temporary file with new content
            QTemporaryFile tempFile;
            if (tempFile.open()) {
                tempFile.write(textEdit.toPlainText().toUtf8());
                tempFile.close();

                // Use sudo to copy the file back
                QProcess copyProcess;
                copyProcess.start("sudo", QStringList() << "cp" << tempFile.fileName() << calamaresBrandingPath);
                copyProcess.write(SUDO_PASSWORD.c_str());
                copyProcess.write("\n");
                copyProcess.closeWriteChannel();
                copyProcess.waitForFinished();

                if (copyProcess.exitCode() == 0) {
                    logOutputText("Calamares branding saved successfully!\n", "green");
                    config.calamaresBrandingEdited = true;
                    saveConfig();
                    updateConfigStatus();
                } else {
                    logOutputText("Failed to save Calamares branding!\n", "red");
                }
            }
            editorDialog.accept();
        });

        connect(&cancelButton, &QPushButton::clicked, &editorDialog, &QDialog::reject);

        editorDialog.exec();
    }

    void onEditCalamares1() {
        QString calamares1Path = "/etc/calamares/modules/initcpio.conf";
        logOutputText("Editing Calamares 1st initcpio.conf: " + calamares1Path + "\n", "cyan");

        if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
            logOutputText("Sudo password required for editing Calamares configuration!\n", "red");
            return;
        }

        QDialog editorDialog(this);
        editorDialog.setWindowTitle("Edit Calamares 1st initcpio.conf");
        editorDialog.setMinimumSize(600, 400);

        QVBoxLayout layout(&editorDialog);
        QTextEdit textEdit;
        QPushButton saveButton("Save");
        QPushButton cancelButton("Cancel");

        // Read current initcpio.conf content
        QProcess process;
        process.start("sudo", QStringList() << "cat" << calamares1Path);
        process.write(SUDO_PASSWORD.c_str());
        process.write("\n");
        process.closeWriteChannel();
        process.waitForFinished();
        if (process.exitCode() == 0) {
            textEdit.setPlainText(process.readAllStandardOutput());
        }

        QHBoxLayout buttonLayout;
        buttonLayout.addWidget(&saveButton);
        buttonLayout.addWidget(&cancelButton);

        layout.addWidget(&textEdit);
        layout.addLayout(&buttonLayout);

        connect(&saveButton, &QPushButton::clicked, [&]() {
            // Create temporary file with new content
            QTemporaryFile tempFile;
            if (tempFile.open()) {
                tempFile.write(textEdit.toPlainText().toUtf8());
                tempFile.close();

                // Use sudo to copy the file back
                QProcess copyProcess;
                copyProcess.start("sudo", QStringList() << "cp" << tempFile.fileName() << calamares1Path);
                copyProcess.write(SUDO_PASSWORD.c_str());
                copyProcess.write("\n");
                copyProcess.closeWriteChannel();
                copyProcess.waitForFinished();

                if (copyProcess.exitCode() == 0) {
                    logOutputText("Calamares 1st initcpio.conf saved successfully!\n", "green");
                    config.calamares1Edited = true;
                    saveConfig();
                    updateConfigStatus();
                } else {
                    logOutputText("Failed to save Calamares 1st initcpio.conf!\n", "red");
                }
            }
            editorDialog.accept();
        });

        connect(&cancelButton, &QPushButton::clicked, &editorDialog, &QDialog::reject);

        editorDialog.exec();
    }

    void onEditCalamares2() {
        QString calamares2Path = "/usr/share/calamares/modules/initcpio.conf";
        logOutputText("Editing Calamares 2nd initcpio.conf: " + calamares2Path + "\n", "cyan");

        if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
            logOutputText("Sudo password required for editing Calamares configuration!\n", "red");
            return;
        }

        QDialog editorDialog(this);
        editorDialog.setWindowTitle("Edit Calamares 2nd initcpio.conf");
        editorDialog.setMinimumSize(600, 400);

        QVBoxLayout layout(&editorDialog);
        QTextEdit textEdit;
        QPushButton saveButton("Save");
        QPushButton cancelButton("Cancel");

        // Read current initcpio.conf content
        QProcess process;
        process.start("sudo", QStringList() << "cat" << calamares2Path);
        process.write(SUDO_PASSWORD.c_str());
        process.write("\n");
        process.closeWriteChannel();
        process.waitForFinished();
        if (process.exitCode() == 0) {
            textEdit.setPlainText(process.readAllStandardOutput());
        }

        QHBoxLayout buttonLayout;
        buttonLayout.addWidget(&saveButton);
        buttonLayout.addWidget(&cancelButton);

        layout.addWidget(&textEdit);
        layout.addLayout(&buttonLayout);

        connect(&saveButton, &QPushButton::clicked, [&]() {
            // Create temporary file with new content
            QTemporaryFile tempFile;
            if (tempFile.open()) {
                tempFile.write(textEdit.toPlainText().toUtf8());
                tempFile.close();

                // Use sudo to copy the file back
                QProcess copyProcess;
                copyProcess.start("sudo", QStringList() << "cp" << tempFile.fileName() << calamares2Path);
                copyProcess.write(SUDO_PASSWORD.c_str());
                copyProcess.write("\n");
                copyProcess.closeWriteChannel();
                copyProcess.waitForFinished();

                if (copyProcess.exitCode() == 0) {
                    logOutputText("Calamares 2nd initcpio.conf saved successfully!\n", "green");
                    config.calamares2Edited = true;
                    saveConfig();
                    updateConfigStatus();
                } else {
                    logOutputText("Failed to save Calamares 2nd initcpio.conf!\n", "red");
                }
            }
            editorDialog.accept();
        });

        connect(&cancelButton, &QPushButton::clicked, &editorDialog, &QDialog::reject);

        editorDialog.exec();
    }

    void onCreateImage() {
        if (!config.allCheckboxesChecked()) {
            logOutputText("Cannot create image - all setup steps must be completed first!\n", "red");
            return;
        }

        if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
            logOutputText("Sudo password required for creating image!\n", "red");
            return;
        }

        showCloneOptionsMenu();
    }

    void onCreateISO() {
        if (!config.isReadyForISO()) {
            logOutputText("Cannot create ISO - setup is incomplete!\n", "red");
            return;
        }

        if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
            logOutputText("Sudo password required for creating ISO!\n", "red");
            return;
        }

        logOutputText("Starting ISO creation process...\n", "cyan");
        progressBar->setValue(50);

        std::string expandedOutputDir = expandPath(config.outputDir);
        worker->executeCommand("mkdir -p " + QString::fromStdString(expandedOutputDir), true);

        // FIXED: Build directory path uses actual username
        std::string buildDir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";
        std::string outputPath = expandedOutputDir + "/" + config.isoName;

        std::string xorrisoCmd = getXorrisoCommand(buildDir, outputPath);
        worker->executeCommand(QString::fromStdString(xorrisoCmd), true);

        // Change ownership of the created ISO to the current user
        std::string chownCmd = "sudo chown " + USERNAME + ":" + USERNAME + " \"" + outputPath + "\"";
        worker->executeCommand(QString::fromStdString(chownCmd), true);

        logOutputText("ISO created successfully at " + QString::fromStdString(outputPath) + "\n", "green");
        logOutputText("Ownership changed to current user: " + QString::fromStdString(USERNAME) + "\n", "cyan");
    }

    void onShowDiskUsage() {
        logOutputText("Disk usage:\n", "cyan");
        worker->executeCommand("df -h", true);
    }

    void onInstallToUSB() {
        if (config.outputDir.empty()) {
            logOutputText("Output directory not set!\n", "red");
            return;
        }

        if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
            logOutputText("Sudo password required for USB installation!\n", "red");
            return;
        }

        std::string expandedOutputDir = expandPath(config.outputDir);
        QDir outputDir(QString::fromStdString(expandedOutputDir));
        QStringList isoFiles = outputDir.entryList(QStringList() << "*.iso", QDir::Files);

        if (isoFiles.isEmpty()) {
            logOutputText("No ISO files found in output directory!\n", "red");
            return;
        }

        bool ok;
        QString selectedISO = QInputDialog::getItem(this, "Select ISO",
                                                    "Choose ISO file:", isoFiles, 0, false, &ok);
        if (!ok || selectedISO.isEmpty()) return;

        QString fullISOPath = QString::fromStdString(expandedOutputDir) + "/" + selectedISO;

        logOutputText("Available drives:\n", "cyan");
        worker->executeCommand("lsblk -d -o NAME,SIZE,MODEL | grep -v 'loop'", true);

        QString targetDrive = QInputDialog::getText(this, "Target Drive",
                                                    "Enter target drive (e.g., /dev/sda):");
        if (targetDrive.isEmpty()) {
            logOutputText("No drive specified!\n", "red");
            return;
        }

        QMessageBox::StandardButton confirm = QMessageBox::warning(this, "Warning",
                                                                   "This will overwrite all data on " + targetDrive + "!\nAre you sure you want to continue?",
                                                                   QMessageBox::Yes | QMessageBox::No);

        if (confirm != QMessageBox::Yes) {
            logOutputText("Operation cancelled.\n", "cyan");
            return;
        }

        logOutputText("Writing " + selectedISO + " to " + targetDrive + "...\n", "cyan");
        progressBar->setValue(75);
        worker->executeCommand("sudo dd if=" + fullISOPath + " of=" + targetDrive +
        " bs=4M status=progress oflag=sync", true);
    }

    void onRunCMIInstaller() {
        logOutputText("Running CMI Installer...\n", "cyan");
        worker->executeCommand("cmirsyncinstaller", true);
    }

    void onRunCalamares() {
        if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
            logOutputText("Sudo password required for running Calamares!\n", "red");
            return;
        }

        logOutputText("Running Calamares...\n", "cyan");
        worker->executeCommand("sudo calamares", true);
    }

    void onUpdateScript() {
        logOutputText("Updating script from GitHub...\n", "cyan");
        progressBar->setValue(90);

        // FIXED: Ensure sudo password is available for updater
        if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
            logOutputText("Sudo password required for updating script!\n", "red");
            return;
        }

        worker->executeCommand("bash -c \"$(curl -fsSL https://raw.githubusercontent.com/claudemods/claudemods-multi-iso-konsole-script/main/advancedimgscript+/installer/patch.sh)\"", true);
    }

    void onShowGuide() {
        // FIXED: Readme path uses actual username
        std::string readmePath = "/home/" + USERNAME + "/.config/cmi/readme.txt";
        worker->executeCommand("mkdir -p " + QString::fromStdString("/home/" + USERNAME + "/.config/cmi"), true);

        QDialog guideDialog(this);
        guideDialog.setWindowTitle("User Guide");
        guideDialog.setMinimumSize(800, 600);

        QVBoxLayout layout(&guideDialog);
        QTextEdit textEdit;
        textEdit.setReadOnly(true);

        // Read guide content
        QFile file(QString::fromStdString(readmePath));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            textEdit.setPlainText(file.readAll());
            file.close();
        } else {
            textEdit.setPlainText("Guide file not found. Please check if the resources were extracted properly.");
        }

        QPushButton closeButton("Close");
        layout.addWidget(&textEdit);
        layout.addWidget(&closeButton);

        connect(&closeButton, &QPushButton::clicked, &guideDialog, &QDialog::accept);

        guideDialog.exec();
    }

    void onSetupScripts() {
        // Show setup scripts dialog
        QDialog setupDialog(this);
        setupDialog.setWindowTitle("Setup Scripts");
        setupDialog.setMinimumSize(400, 500);

        QVBoxLayout *layout = new QVBoxLayout(&setupDialog);

        QStringList setupButtons = {
            "Install Dependencies", "Set Clone Directory", "Set ISO Tag", "Set ISO Name",
            "Set Output Directory", "Select vmlinuz", "Generate mkinitcpio", "Edit GRUB Config",
            "Edit Boot Text", "Edit Calamares Branding", "Edit Calamares 1st initcpio.conf",
            "Edit Calamares 2nd initcpio.conf"
        };

        for (const QString &buttonText : setupButtons) {
            QPushButton *btn = new QPushButton(buttonText);
            layout->addWidget(btn);
            connect(btn, &QPushButton::clicked, [&, buttonText]() {
                handleSetupButton(buttonText);
                setupDialog.accept();
            });
        }

        QPushButton *closeButton = new QPushButton("Close");
        layout->addWidget(closeButton);
        connect(closeButton, &QPushButton::clicked, &setupDialog, &QDialog::reject);

        setupDialog.exec();
    }

    void onCommandOutput(const QString &output) {
        logOutputText(output, "white");
    }

    void onCommandFinished(bool success) {
        if (success) {
            logOutputText("Command completed successfully!\n", "green");
            progressBar->setValue(100);
            QTimer::singleShot(2000, this, [this]() { progressBar->setValue(0); });
        } else {
            logOutputText("Command completed with errors!\n", "red");
            progressBar->setValue(0);
        }
    }

    void updateTimeDisplay() {
        QDateTime now = QDateTime::currentDateTime();
        timeLabel->setText("Current UK Time: " + now.toString("dd/MM/yyyy hh:mm:ss"));

        // Update disk usage
        QProcess process;
        process.start("df", QStringList() << "-h" << "/");
        process.waitForFinished();
        QString diskUsage = process.readAllStandardOutput();
        QStringList lines = diskUsage.split('\n');
        if (lines.size() > 1) {
            diskUsageLabel->setText("Disk Usage: " + lines[1].simplified());
        }
    }

    void checkForUpdates() {
        logOutputText("Checking for updates...\n", "cyan");

        // FIXED: Clone directory uses actual username
        std::string cloneDir = "/home/" + USERNAME + "/claudemods-multi-iso-konsole-script";

        // Try to clone the repository
        std::string cloneCmd = "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git " + cloneDir;
        int cloneResult = system(cloneCmd.c_str());

        if (cloneResult != 0) {
            logOutputText("Failed to check for updates. Continuing with current version.\n", "yellow");
            return;
        }

        // Read current version
        // FIXED: Version path uses actual username
        std::string currentVersionPath = "/home/" + USERNAME + "/.config/cmi/version.txt";
        std::string currentVersion = "";

        std::ifstream currentFile(currentVersionPath);
        if (currentFile.is_open()) {
            std::getline(currentFile, currentVersion);
            currentFile.close();
        }

        // Read new version
        std::string newVersionPath = cloneDir + "/advancedimgscript+/version/version.txt";
        std::string newVersion = "";

        std::ifstream newFile(newVersionPath);
        if (newFile.is_open()) {
            std::getline(newFile, newVersion);
            newFile.close();
        }

        // Clean up cloned directory
        std::string cleanupCmd = "rm -rf " + cloneDir;
        system(cleanupCmd.c_str());

        if (newVersion.empty()) {
            logOutputText("Could not retrieve new version information.\n", "yellow");
            return;
        }

        if (currentVersion.empty()) {
            logOutputText("No current version found. Assuming first run.\n", "yellow");

            // Extract embedded resources
            if (extractEmbeddedZip()) {
                logOutputText("Base resources extracted successfully.\n", "green");
            }

            // Extract Calamares resources
            if (extractCalamaresResources()) {
                logOutputText("Calamares resources extracted successfully.\n", "green");
            }

            // Execute extrainstalls.sh after ALL zips are finished
            logOutputText("Executing extra installations...\n", "cyan");
            // FIXED: extrainstalls.sh path uses actual username
            std::string extraInstallsPath = "/home/" + USERNAME + "/.config/cmi/extrainstalls.sh";
            execute_command("bash " + extraInstallsPath, true);

            return;
        }

        logOutputText("Current version: " + QString::fromStdString(currentVersion) + "\n", "cyan");
        logOutputText("Latest version: " + QString::fromStdString(newVersion) + "\n", "cyan");

        if (currentVersion != newVersion) {
            logOutputText("New version available!\n", "green");

            QMessageBox::StandardButton reply = QMessageBox::question(this, "Update Available",
                                                                      "A new version is available! Do you want to update?",
                                                                      QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                logOutputText("Starting update process...\n", "cyan");
                onUpdateScript();
            }
        } else {
            logOutputText("You are running the latest version.\n", "green");
        }
    }

private:
    void setupUI() {
        setWindowTitle("Advanced C++ Arch Img Iso Script+ Beta v2.03 04-10-2025");
        setMinimumSize(1200, 800);

        QWidget *centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

        // Header
        QLabel *titleLabel = new QLabel("Advanced C++ Arch Img Iso Script+ Beta v2.03 04-10-2025");
        titleLabel->setStyleSheet("QLabel { color: #ff0000; font-size: 16px; font-weight: bold; }");
        mainLayout->addWidget(titleLabel);

        timeLabel = new QLabel();
        timeLabel->setStyleSheet("QLabel { color: #00ffff; }");
        mainLayout->addWidget(timeLabel);

        diskUsageLabel = new QLabel();
        diskUsageLabel->setStyleSheet("QLabel { color: #00ff00; }");
        mainLayout->addWidget(diskUsageLabel);

        // Content area
        QHBoxLayout *contentLayout = new QHBoxLayout();

        // Left panel - buttons
        QWidget *buttonPanel = new QWidget();
        QVBoxLayout *buttonLayout = new QVBoxLayout(buttonPanel);

        // Main group - moved Setup Scripts below Guide
        QGroupBox *mainGroup = new QGroupBox("Main Menu");
        QVBoxLayout *mainGroupLayout = new QVBoxLayout(mainGroup);

        QStringList mainButtons = {
            "Guide",
            "Setup Scripts",  // Moved here below Guide
            "Create Image",
            "Create ISO",
            "Show Disk Usage",
            "Install ISO to USB",
            "CMI BTRFS/EXT4 Installer",
            "Calamares",
            "Update Script"
        };

        for (const QString &buttonText : mainButtons) {
            QPushButton *btn = new QPushButton(buttonText);
            mainGroupLayout->addWidget(btn);
            connect(btn, &QPushButton::clicked, this, [this, buttonText]() {
                handleMainButton(buttonText);
            });
        }

        buttonLayout->addWidget(mainGroup);
        buttonLayout->addStretch();

        // Right panel - log and config
        QWidget *rightPanel = new QWidget();
        QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);

        // Config status
        configGroup = new QGroupBox("Configuration Status");
        QVBoxLayout *configLayout = new QVBoxLayout(configGroup);
        configStatusLabel = new QLabel();
        configStatusLabel->setTextFormat(Qt::RichText);
        configLayout->addWidget(configStatusLabel);
        rightLayout->addWidget(configGroup);

        // Log output
        QGroupBox *logGroup = new QGroupBox("Output Log");
        QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
        logOutputWidget = new QTextEdit();
        logOutputWidget->setReadOnly(true);
        logOutputWidget->setStyleSheet("QTextEdit { background-color: black; color: white; font-family: monospace; }");
        logLayout->addWidget(logOutputWidget);
        rightLayout->addWidget(logGroup);

        contentLayout->addWidget(buttonPanel, 1);
        contentLayout->addWidget(rightPanel, 2);

        mainLayout->addLayout(contentLayout);

        // Progress bar - MOVED TO BOTTOM
        progressBar = new QProgressBar();
        progressBar->setStyleSheet("QProgressBar { border: 2px solid grey; border-radius: 5px; text-align: center; color: white; } QProgressBar::chunk { background-color: #ff0000; }");
        progressBar->setValue(0);
        mainLayout->addWidget(progressBar);

        // Worker thread
        worker = new WorkerThread(this);
        connect(worker, &WorkerThread::commandOutput, this, &MainWindow::onCommandOutput);
        connect(worker, &WorkerThread::commandFinished, this, &MainWindow::onCommandFinished);
        connect(worker, &WorkerThread::progressUpdate, progressBar, &QProgressBar::setValue);

        // Timer for clock
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &MainWindow::updateTimeDisplay);
        timer->start(1000);

        updateTimeDisplay();
        updateConfigStatus();
    }

    void handleSetupButton(const QString &buttonText) {
        static QHash<QString, void(MainWindow::*)()> buttonMap = {
            {"Install Dependencies", &MainWindow::onInstallDependencies},
            {"Set Clone Directory", &MainWindow::onSetCloneDir},
            {"Set ISO Tag", &MainWindow::onSetIsoTag},
            {"Set ISO Name", &MainWindow::onSetIsoName},
            {"Set Output Directory", &MainWindow::onSetOutputDir},
            {"Select vmlinuz", &MainWindow::onSelectVmlinuz},
            {"Generate mkinitcpio", &MainWindow::onGenerateMkinitcpio},
            {"Edit GRUB Config", &MainWindow::onEditGrubCfg},
            {"Edit Boot Text", &MainWindow::onEditBootText},
            {"Edit Calamares Branding", &MainWindow::onEditCalamaresBranding},
            {"Edit Calamares 1st initcpio.conf", &MainWindow::onEditCalamares1},
            {"Edit Calamares 2nd initcpio.conf", &MainWindow::onEditCalamares2}
        };

        if (buttonMap.contains(buttonText)) {
            (this->*buttonMap[buttonText])();
        }
    }

    void handleMainButton(const QString &buttonText) {
        static QHash<QString, void(MainWindow::*)()> buttonMap = {
            {"Guide", &MainWindow::onShowGuide},
            {"Setup Scripts", &MainWindow::onSetupScripts},  // Added this
            {"Create Image", &MainWindow::onCreateImage},
            {"Create ISO", &MainWindow::onCreateISO},
            {"Show Disk Usage", &MainWindow::onShowDiskUsage},
            {"Install ISO to USB", &MainWindow::onInstallToUSB},
            {"CMI BTRFS/EXT4 Installer", &MainWindow::onRunCMIInstaller},
            {"Calamares", &MainWindow::onRunCalamares},
            {"Update Script", &MainWindow::onUpdateScript}
        };

        if (buttonMap.contains(buttonText)) {
            (this->*buttonMap[buttonText])();
        }
    }

    void showCloneOptionsMenu() {
        QDialog dialog(this);
        dialog.setWindowTitle("Clone Options - Select Source");
        QVBoxLayout layout(&dialog);

        QStringList options = {
            "Clone Current System (as it is now)",
            "Clone Another Drive (e.g., /dev/sda2)",
            "Clone Folder or File",
            "Cancel"
        };

        for (int i = 0; i < options.size(); ++i) {
            QPushButton *btn = new QPushButton(options[i]);
            layout.addWidget(btn);
            connect(btn, &QPushButton::clicked, [&, i]() {
                if (i == 3) { // Cancel
                    dialog.reject();
                    return;
                }

                if (config.cloneDir.empty()) {
                    logOutputText("Clone directory not set! Please set it first.\n", "red");
                    dialog.reject();
                    return;
                }

                QString cloneDir = QString::fromStdString(expandPath(config.cloneDir));
                worker->executeCommand("sudo mkdir -p " + cloneDir, true);

                switch (i) {
                    case 0: cloneCurrentSystem(cloneDir); break;
                    case 1: cloneAnotherDrive(cloneDir); break;
                    case 2: cloneFolderOrFile(cloneDir); break;
                }

                dialog.accept();
            });
        }

        dialog.exec();
    }

    void cloneCurrentSystem(const QString &cloneDir) {
        logOutputText("Cloning current system to " + cloneDir + "...\n", "cyan");
        progressBar->setValue(25);

        std::string command = getRsyncCommand("/", cloneDir.toStdString());
        worker->executeCommand(QString::fromStdString(command), true);

        // Create SquashFS after cloning
        std::string outputDir = getOutputDirectory();
        std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;

        worker->executeCommand(QString::fromStdString("sudo mksquashfs " + cloneDir.toStdString() + " " + finalImgPath +
        " -noappend -comp xz -b 256K -Xbcj x86"), true);

        // Clean up
        worker->executeCommand("sudo rm -rf " + cloneDir, true);

        // Create checksum
        worker->executeCommand(QString::fromStdString("sha512sum " + finalImgPath + " > " + finalImgPath + ".sha512"), true);

        logOutputText("SquashFS image created successfully: " + QString::fromStdString(finalImgPath) + "\n", "green");
    }

    void cloneAnotherDrive(const QString &cloneDir) {
        logOutputText("Available drives:\n", "cyan");
        worker->executeCommand("lsblk -f -o NAME,FSTYPE,SIZE,MOUNTPOINT | grep -v 'loop'", true);

        bool ok;
        QString drive = QInputDialog::getText(this, "Drive to Clone",
                                              "Enter drive to clone (e.g., /dev/sda2):",
                                              QLineEdit::Normal, "", &ok);
        if (!ok || drive.isEmpty()) return;

        QString tempMountPoint = "/mnt/temp_clone_mount";

        logOutputText("Cloning " + drive + " to " + cloneDir + "...\n", "cyan");
        progressBar->setValue(35);

        // Mount and clone
        worker->executeCommand("sudo mkdir -p " + tempMountPoint, true);
        worker->executeCommand("sudo mount " + drive + " " + tempMountPoint, true);

        std::string command = getRsyncCommand(tempMountPoint.toStdString(), cloneDir.toStdString());
        worker->executeCommand(QString::fromStdString(command), true);
        worker->executeCommand("sudo umount " + tempMountPoint, true);
        worker->executeCommand("sudo rmdir " + tempMountPoint, true);

        // Create SquashFS after cloning
        std::string outputDir = getOutputDirectory();
        std::string finalImgPath = outputDir + "/" + FINAL_IMG_NAME;

        worker->executeCommand(QString::fromStdString("sudo mksquashfs " + cloneDir.toStdString() + " " + finalImgPath +
        " -noappend -comp xz -b 256K -Xbcj x86"), true);

        // Clean up
        worker->executeCommand("sudo rm -rf " + cloneDir, true);

        // Create checksum
        worker->executeCommand(QString::fromStdString("sha512sum " + finalImgPath + " > " + finalImgPath + ".sha512"), true);

        logOutputText("SquashFS image created successfully: " + QString::fromStdString(finalImgPath) + "\n", "green");
    }

    void cloneFolderOrFile(const QString &cloneDir) {
        QString sourcePath = QFileDialog::getExistingDirectory(this, "Select Folder to Clone");
        if (sourcePath.isEmpty()) return;

        QString userfilesDir = cloneDir + "/home/userfiles";
        worker->executeCommand("sudo mkdir -p " + userfilesDir, true);

        logOutputText("Cloning " + sourcePath + " to " + userfilesDir + "...\n", "cyan");
        progressBar->setValue(45);

        std::string command = "sudo rsync -aHAXSr --numeric-ids --info=progress2 " +
        sourcePath.toStdString() + " " + userfilesDir.toStdString() + "/";

        worker->executeCommand(QString::fromStdString(command), true);

        logOutputText("Folder cloned successfully to: " + userfilesDir + "\n", "green");
    }

    void logOutputText(const QString &message, const QString &color) {
        QString htmlColor;
        if (color == "red") htmlColor = "#ff0000";
        else if (color == "green") htmlColor = "#00ff00";
        else if (color == "cyan") htmlColor = "#00ffff";
        else if (color == "yellow") htmlColor = "#ffff00";
        else if (color == "white") htmlColor = "#ffffff";
        else htmlColor = "#ffffff"; // default to white

        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        QString formattedMessage = QString("<span style='color: %1'>[%2] %3</span>")
        .arg(htmlColor, timestamp, message.toHtmlEscaped());

        logOutputWidget->append(formattedMessage);

        // Auto-scroll to bottom
        QScrollBar *scrollBar = logOutputWidget->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }

    void updateConfigStatus() {
        QString status;
        status += checkboxStatus("Dependencies Installed", config.dependenciesInstalled);
        status += textStatus("ISO Tag", config.isoTag);
        status += textStatus("ISO Name", config.isoName);
        status += textStatus("Output Directory", config.outputDir);
        status += textStatus("vmlinuz Selected", config.vmlinuzPath);
        status += textStatus("Clone Directory", config.cloneDir);
        status += checkboxStatus("mkinitcpio Generated", config.mkinitcpioGenerated);
        status += checkboxStatus("GRUB Config Edited", config.grubEdited);
        status += checkboxStatus("Boot Text Edited", config.bootTextEdited);
        status += checkboxStatus("Calamares Branding Edited", config.calamaresBrandingEdited);
        status += checkboxStatus("Calamares 1st initcpio.conf Edited", config.calamares1Edited);
        status += checkboxStatus("Calamares 2nd initcpio.conf Edited", config.calamares2Edited);

        configStatusLabel->setText(status);
    }

    QString checkboxStatus(const QString &text, bool checked) {
        return QString("%1 <span style='color: %2'>[%3]</span><br>")
        .arg(text)
        .arg(checked ? "#00ff00" : "#ff0000")
        .arg(checked ? "✓" : " ");
    }

    QString textStatus(const QString &text, const std::string &value) {
        return QString("%1: <span style='color: %2'>%3</span><br>")
        .arg(text)
        .arg(value.empty() ? "#ffff00" : "#00ffff")
        .arg(value.empty() ? "Not set" : QString::fromStdString(value));
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
                    else if (key == "cloneDir") config.cloneDir = value;
                    else if (key == "mkinitcpioGenerated") config.mkinitcpioGenerated = (value == "1");
                    else if (key == "grubEdited") config.grubEdited = (value == "1");
                    else if (key == "bootTextEdited") config.bootTextEdited = (value == "1");
                    else if (key == "calamaresBrandingEdited") config.calamaresBrandingEdited = (value == "1");
                    else if (key == "calamares1Edited") config.calamares1Edited = (value == "1");
                    else if (key == "calamares2Edited") config.calamares2Edited = (value == "1");
                    else if (key == "dependenciesInstalled") config.dependenciesInstalled = (value == "1");
                }
            }
            configFile.close();
        }
    }

    void saveConfig() {
        std::string configPath = getConfigFilePath();
        std::ofstream configFile(configPath);
        if (configFile.is_open()) {
            configFile << "isoTag=" << config.isoTag << "\n";
            configFile << "isoName=" << config.isoName << "\n";
            configFile << "outputDir=" << config.outputDir << "\n";
            configFile << "vmlinuzPath=" << config.vmlinuzPath << "\n";
            configFile << "cloneDir=" << config.cloneDir << "\n";
            configFile << "mkinitcpioGenerated=" << (config.mkinitcpioGenerated ? "1" : "0") << "\n";
            configFile << "grubEdited=" << (config.grubEdited ? "1" : "0") << "\n";
            configFile << "bootTextEdited=" << (config.bootTextEdited ? "1" : "0") << "\n";
            configFile << "calamaresBrandingEdited=" << (config.calamaresBrandingEdited ? "1" : "0") << "\n";
            configFile << "calamares1Edited=" << (config.calamares1Edited ? "1" : "0") << "\n";
            configFile << "calamares2Edited=" << (config.calamares2Edited ? "1" : "0") << "\n";
            configFile << "dependenciesInstalled=" << (config.dependenciesInstalled ? "1" : "0") << "\n";
            configFile.close();
        } else {
            logOutputText("Failed to save configuration!\n", "red");
        }
    }

    void startTimeThread() {
        time_thread = std::thread([this]() {
            while (time_thread_running) {
                QMetaObject::invokeMethod(this, &MainWindow::updateTimeDisplay, Qt::QueuedConnection);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }

    // Member variables
    WorkerThread *worker;
    QProgressBar *progressBar;
    QTextEdit *logOutputWidget;
    QLabel *timeLabel;
    QLabel *diskUsageLabel;
    QLabel *configStatusLabel;
    QGroupBox *configGroup;

    std::thread time_thread;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Initialize Qt application for resource system
    QCoreApplication::setApplicationName("Advanced Arch Image ISO Script");

    MainWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
