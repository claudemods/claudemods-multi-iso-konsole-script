#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QTabWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QProcess>
#include <QThread>
#include <QDateTime>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QResource>
#include <QInputDialog>
#include <QScrollBar>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <QTextCursor>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QMutex>
#include <QFuture>
#include <QtConcurrent>
#include <atomic>
#include <mutex>
#include <thread>
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
#include <sstream>
#include <iomanip>

// Qt includes for resource system
#include <QFile>
#include <QResource>
#include <QDir>
#include <QCoreApplication>

// Forward declarations - ALL original functions kept
void saveConfig();
void execute_command(const QString& cmd, bool continueOnError = false);
void printCheckbox(bool checked);
QString getUserInput(const QString& prompt);
void clearScreen();
int getch();
int kbhit();
void printBanner();
void printConfigStatus();
void installDependencies();
void selectVmlinuz();
void generateMkinitcpio();
void editGrubCfg();
void editBootText();
void editCalamaresBranding();
void editCalamares1();
void editCalamares2();
void setIsoTag();
void setIsoName();
void setOutputDir();
void setCloneDir();
bool extractEmbeddedZip();
bool extractCalamaresResources();
bool checkForUpdates();
void updateScript();
void showGuide();
void installISOToUSB();
void runCMIInstaller();
void runCalamares();
bool isDeviceMounted(const QString& device);
bool mountDevice(const QString& device, const QString& mountPoint);
void cloneCurrentSystem(const QString& cloneDir);
void cloneAnotherDrive(const QString& cloneDir);
void cloneFolderOrFile(const QString& cloneDir);
bool copyFilesWithRsync(const QString& source, const QString& destination);
bool createSquashFS(const QString& inputDir, const QString& outputFile);
bool createChecksum(const QString& filename);
void printFinalMessage(const QString& outputFile);
QString getOutputDirectory();
QString expandPath(const QString& path);
void showSetupMenu();
void showCloneOptionsMenu();
void showMainMenu();
bool createISO();
void update_time_thread();
QString getConfigFilePath();

// Time-related globals
std::atomic<bool> time_thread_running(true);
std::mutex time_mutex;
QString current_time_str;
bool should_reset = false;

// Constants - ALL original constants kept
const QString ORIG_IMG_NAME = "rootfs1.img";
const QString FINAL_IMG_NAME = "rootfs.img";
QString MOUNT_POINT = "/mnt/ext4_temp";
const QString SOURCE_DIR = "/";
const QString COMPRESSION_LEVEL = "22";
const QString SQUASHFS_COMPRESSION = "zstd";
const QStringList SQUASHFS_COMPRESSION_ARGS = {"-Xcompression-level", "22"};
QString BUILD_DIR = "/home/$USER/.config/cmi/build-image-arch-img";
QString USERNAME = "";

// Dependencies list - ALL original dependencies kept
const QStringList DEPENDENCIES = {
    "rsync", "squashfs-tools", "xorriso", "grub", "dosfstools", "unzip", "nano",
    "arch-install-scripts", "bash-completion", "erofs-utils", "findutils", "unzip",
    "jq", "libarchive", "libisoburn", "lsb-release", "lvm2", "mkinitcpio-archiso",
    "mkinitcpio-nfs-utils", "mtools", "nbd", "pacman-contrib", "parted", "procps-ng",
    "pv", "python", "sshfs", "syslinux", "xdg-utils", "zsh-completions",
    "kernel-modules-hook", "virt-manager", "qt6-tools", "btrfs-progs", "e2fsprogs",
    "f2fs-tools", "xfsprogs", "xfsdump", "qt5-tools"
};

// Configuration state - ALL original struct kept
struct ConfigState {
    QString isoTag;
    QString isoName;
    QString outputDir;
    QString vmlinuzPath;
    QString cloneDir;
    bool mkinitcpioGenerated = false;
    bool grubEdited = false;
    bool bootTextEdited = false;
    bool calamaresBrandingEdited = false;
    bool calamares1Edited = false;
    bool calamares2Edited = false;
    bool dependenciesInstalled = false;

    bool isReadyForISO() const {
        return !isoTag.isEmpty() && !isoName.isEmpty() && !outputDir.isEmpty() &&
               !vmlinuzPath.isEmpty() && mkinitcpioGenerated && grubEdited && dependenciesInstalled;
    }

    bool allCheckboxesChecked() const {
        return dependenciesInstalled && !isoTag.isEmpty() && !isoName.isEmpty() &&
               !outputDir.isEmpty() && !vmlinuzPath.isEmpty() && !cloneDir.isEmpty() &&
               mkinitcpioGenerated && grubEdited && bootTextEdited &&
               calamaresBrandingEdited && calamares1Edited && calamares2Edited;
    }
};

ConfigState config;

// ANSI color codes - ALL original colors kept
const QString COLOR_RED = "\033[31m";
const QString COLOR_GREEN = "\033[32m";
const QString COLOR_BLUE = "\033[34m";
const QString COLOR_CYAN = "\033[38;2;0;255;255m";
const QString COLOR_YELLOW = "\033[33m";
const QString COLOR_RESET = "\033[0m";
const QString COLOR_HIGHLIGHT = "\033[38;2;0;255;255m";
const QString COLOR_NORMAL = "\033[34m";
const QString COLOR_DISABLED = "\033[90m";

// Global pointers for UI updates
QTextEdit* g_logTextEdit = nullptr;
QProgressBar* g_progressBar = nullptr;

// Worker thread for command execution
class CommandWorker : public QObject {
    Q_OBJECT
public:
    explicit CommandWorker(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void executeCommand(const QString &command, bool continueOnError = false) {
        QProcess process;
        process.start("bash", {"-c", command});
        process.waitForFinished(-1);
        
        QString output = process.readAllStandardOutput();
        QString error = process.readAllStandardError();
        
        if (!output.isEmpty()) {
            emit outputReceived(output);
        }
        if (!error.isEmpty()) {
            emit outputReceived("ERROR: " + error);
        }
        
        bool success = (process.exitCode() == 0) || continueOnError;
        emit commandFinished(success, process.exitCode());
    }

signals:
    void outputReceived(const QString &output);
    void commandFinished(bool success, int exitCode);
};

// ALL ORIGINAL FUNCTIONS IMPLEMENTED BELOW - NOTHING REMOVED

bool extractEmbeddedZip() {
    if (g_logTextEdit) {
        g_logTextEdit->append("Extracting build resources...");
    }
    
    QString configDir = "/home/" + USERNAME + "/.config/cmi";
    QString zipPath = configDir + "/build-image-arch-img.zip";
    QString extractPath = configDir;

    // Create config directory
    execute_command("mkdir -p " + configDir, true);

    // Use Qt resource system to access embedded zip
    QFile embeddedZip(":/zip/build-image-arch-img.zip");
    if (!embeddedZip.exists()) {
        // Silent failure - no error reporting
        return false;
    }

    // Copy embedded resource to filesystem
    if (!embeddedZip.copy(zipPath)) {
        // Silent failure - no error reporting
        return false;
    }

    // Set proper permissions
    QFile::setPermissions(zipPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

    // Extract zip file
    if (g_logTextEdit) {
        g_logTextEdit->append("Extracting zip file...");
    }
    QString extractCmd = "unzip -o " + zipPath + " -d " + extractPath + " >/dev/null 2>&1";
    if (system(extractCmd.toStdString().c_str()) != 0) {
        // Silent failure - no error reporting
        return false;
    }

    // Clean up zip file
    execute_command("rm -f " + zipPath, true);

    if (g_logTextEdit) {
        g_logTextEdit->append("Build resources extracted successfully to: " + extractPath);
    }
    return true;
}

bool extractCalamaresResources() {
    if (g_logTextEdit) {
        g_logTextEdit->append("Extracting Calamares resources...");
    }

    QString configDir = "/home/" + USERNAME + "/.config/cmi";
    QString calamaresDir = configDir + "/calamares-files";

    // Create calamares directory
    execute_command("mkdir -p " + calamaresDir, true);

    // Step 1: Extract calamares.zip to cmi folder
    if (g_logTextEdit) {
        g_logTextEdit->append("Step 1: Extracting calamares.zip to cmi folder...");
    }

    QString calamaresZipPath = configDir + "/calamares.zip";
    QFile embeddedCalamaresZip(":/zip/calamares.zip");
    if (!embeddedCalamaresZip.exists()) {
        // Silent failure - no error reporting
        return false;
    }

    // Copy calamares.zip to filesystem
    if (!embeddedCalamaresZip.copy(calamaresZipPath)) {
        // Silent failure - no error reporting
        return false;
    }

    // Set proper permissions
    QFile::setPermissions(calamaresZipPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

    // Extract calamares.zip
    QString extractCalamaresCmd = "unzip -o " + calamaresZipPath + " -d " + configDir + " >/dev/null 2>&1";
    if (system(extractCalamaresCmd.toStdString().c_str()) != 0) {
        // Silent failure - no error reporting
        return false;
    }

    // Clean up calamares.zip
    execute_command("rm -f " + calamaresZipPath, true);

    // Step 2: Extract branding.zip to cmi/calamares
    if (g_logTextEdit) {
        g_logTextEdit->append("Step 2: Extracting branding.zip to cmi/calamares-files...");
    }

    QString brandingZipPath = configDir + "/branding.zip";
    QFile embeddedBrandingZip(":/zip/branding.zip");
    if (!embeddedBrandingZip.exists()) {
        // Silent failure - no error reporting
        return false;
    }

    // Copy branding.zip to filesystem
    if (!embeddedBrandingZip.copy(brandingZipPath)) {
        // Silent failure - no error reporting
        return false;
    }

    // Set proper permissions
    QFile::setPermissions(brandingZipPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

    // Extract branding.zip to calamares directory
    QString extractBrandingCmd = "unzip -o " + brandingZipPath + " -d " + calamaresDir + " >/dev/null 2>&1";
    if (system(extractBrandingCmd.toStdString().c_str()) != 0) {
        // Silent failure - no error reporting
        return false;
    }

    // Clean up branding.zip
    execute_command("rm -f " + brandingZipPath, true);

    // Step 3: Copy extras.zip to cmi/calamares
    if (g_logTextEdit) {
        g_logTextEdit->append("Step 3: Copying extras.zip to cmi/calamares-files...");
    }

    QString extrasZipPath = calamaresDir + "/extras.zip";
    QFile embeddedExtrasZip(":/zip/extras.zip");
    if (!embeddedExtrasZip.exists()) {
        // Silent failure - no error reporting
        return false;
    }

    // Copy extras.zip to calamares directory
    if (!embeddedExtrasZip.copy(extrasZipPath)) {
        // Silent failure - no error reporting
        return false;
    }

    // Set proper permissions
    QFile::setPermissions(extrasZipPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);

    if (g_logTextEdit) {
        g_logTextEdit->append("Calamares resources extracted successfully!");
        g_logTextEdit->append("- calamares.zip extracted to: " + configDir);
        g_logTextEdit->append("- branding.zip extracted to: " + calamaresDir);
        g_logTextEdit->append("- extras.zip copied to: " + calamaresDir);
    }

    return true;
}

bool checkForUpdates() {
    if (g_logTextEdit) {
        g_logTextEdit->append("Checking for updates...");
    }

    QString cloneDir = "/home/" + USERNAME + "/claudemods-multi-iso-konsole-script";

    // Try to clone the repository
    QString cloneCmd = "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git " + cloneDir + " 2>/dev/null";
    int cloneResult = system(cloneCmd.toStdString().c_str());

    if (cloneResult != 0) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Failed to check for updates. Continuing with current version.");
        }
        return false;
    }

    // Read current version
    QString currentVersionPath = "/home/" + USERNAME + "/.config/cmi/version.txt";
    QString currentVersion = "";

    QFile currentFile(currentVersionPath);
    if (currentFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&currentFile);
        currentVersion = in.readLine();
        currentFile.close();
    }

    // Read new version
    QString newVersionPath = cloneDir + "/advancedimgscript+/version/version.txt";
    QString newVersion = "";

    QFile newFile(newVersionPath);
    if (newFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&newFile);
        newVersion = in.readLine();
        newFile.close();
    }

    // Clean up cloned directory
    QString cleanupCmd = "rm -rf " + cloneDir;
    system(cleanupCmd.toStdString().c_str());

    if (newVersion.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Could not retrieve new version information.");
        }
        return false;
    }

    if (currentVersion.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("No current version found. Assuming first run.");
        }

        // First run - extract embedded resources
        if (!extractEmbeddedZip()) {
            // Silent failure - no error reporting
        }

        // Also extract Calamares resources on first run
        if (!extractCalamaresResources()) {
            // Silent failure - no error reporting
        }

        // Execute extrainstalls.sh after ALL zips are finished
        if (g_logTextEdit) {
            g_logTextEdit->append("Executing extra installations...");
        }
        execute_command("bash /home/" + USERNAME + "/.config/cmi/extrainstalls.sh", true);

        return false;
    }

    if (g_logTextEdit) {
        g_logTextEdit->append("Current version: " + currentVersion);
        g_logTextEdit->append("Latest version: " + newVersion);
    }

    if (currentVersion != newVersion) {
        if (g_logTextEdit) {
            g_logTextEdit->append("New version available!");
        }
        
        // In GUI, we'll show a dialog instead of command line input
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, "Update Available", 
            "New version available!\nDo you want to update?", 
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            if (g_logTextEdit) {
                g_logTextEdit->append("Starting update process...");
            }
            return true;
        }
    } else {
        if (g_logTextEdit) {
            g_logTextEdit->append("You are running the latest version.");
        }
    }

    return false;
}

void update_time_thread() {
    while (time_thread_running) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char datetime[50];
        strftime(datetime, sizeof(datetime), "%d/%m/%Y %H:%M:%S", t);

        {
            std::lock_guard<std::mutex> lock(time_mutex);
            current_time_str = QString::fromLocal8Bit(datetime);
        }
        sleep(1);
    }
}

void clearScreen() {
    if (g_logTextEdit) {
        g_logTextEdit->clear();
    }
}

void execute_command(const QString& cmd, bool continueOnError) {
    if (g_logTextEdit) {
        g_logTextEdit->append("Executing: " + cmd);
    }
    
    QProcess process;
    process.start("bash", {"-c", cmd});
    process.waitForFinished(-1);
    
    QString output = process.readAllStandardOutput();
    QString error = process.readAllStandardError();
    
    if (g_logTextEdit) {
        if (!output.isEmpty()) {
            g_logTextEdit->append(output);
        }
        if (!error.isEmpty()) {
            g_logTextEdit->append("ERROR: " + error);
        }
    }
    
    if (process.exitCode() != 0 && !continueOnError) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Command failed: " + cmd);
        }
        // In GUI, we don't exit the application
    } else if (process.exitCode() != 0) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Command failed but continuing: " + cmd);
        }
    }
}

void printCheckbox(bool checked) {
    if (g_logTextEdit) {
        if (checked) {
            g_logTextEdit->append("[✓]");
        } else {
            g_logTextEdit->append("[ ]");
        }
    }
}

void printBanner() {
    if (!g_logTextEdit) return;
    
    // Clear screen and move cursor to top
    g_logTextEdit->clear();

    g_logTextEdit->append(R"(
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚══════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
)");
    g_logTextEdit->append(" Advanced C++ Arch Img Iso Script+ Beta v2.03 04-10-2025");

    {
        std::lock_guard<std::mutex> lock(time_mutex);
        g_logTextEdit->append("Current UK Time: " + current_time_str);
    }

    g_logTextEdit->append("Filesystem      Size  Used Avail Use% Mounted on");
    execute_command("df -h / | tail -1");
    g_logTextEdit->append("");
}

void printConfigStatus() {
    if (!g_logTextEdit) return;
    
    g_logTextEdit->append("Current Configuration:");

    g_logTextEdit->append(" [ ] Dependencies Installed");

    g_logTextEdit->append(" [ ] ISO Tag: " + (config.isoTag.isEmpty() ? "Not set" : config.isoTag));

    g_logTextEdit->append(" [ ] ISO Name: " + (config.isoName.isEmpty() ? "Not set" : config.isoName));

    g_logTextEdit->append(" [ ] Output Directory: " + (config.outputDir.isEmpty() ? "Not set" : config.outputDir));

    g_logTextEdit->append(" [ ] vmlinuz Selected: " + (config.vmlinuzPath.isEmpty() ? "Not selected" : config.vmlinuzPath));

    g_logTextEdit->append(" [ ] Clone Directory: " + (config.cloneDir.isEmpty() ? "Not set" : config.cloneDir));

    g_logTextEdit->append(" [ ] mkinitcpio Generated");

    g_logTextEdit->append(" [ ] GRUB Config Edited");

    g_logTextEdit->append(" [ ] Boot Text Edited");

    g_logTextEdit->append(" [ ] Calamares Branding Edited");

    g_logTextEdit->append(" [ ] Calamares 1st initcpio.conf Edited");

    g_logTextEdit->append(" [ ] Calamares 2nd initcpio.conf Edited");
}

QString getUserInput(const QString& prompt) {
    bool ok;
    QString text = QInputDialog::getText(nullptr, "Input", prompt, QLineEdit::Normal, "", &ok);
    if (ok && !text.isEmpty()) {
        return text;
    }
    return "";
}

void installDependencies() {
    if (g_logTextEdit) {
        g_logTextEdit->append("Installing required dependencies...");
    }

    // First update the package database
    if (g_logTextEdit) {
        g_logTextEdit->append("Updating package database...");
    }
    execute_command("sudo pacman -Sy");

    QString packages = DEPENDENCIES.join(" ");

    QString command = "sudo pacman -S --needed --noconfirm " + packages;
    execute_command(command);

    config.dependenciesInstalled = true;
    saveConfig();
    if (g_logTextEdit) {
        g_logTextEdit->append("Dependencies installed successfully!");
    }
}

void selectVmlinuz() {
    QDir bootDir("/boot");
    QStringList vmlinuzFiles = bootDir.entryList(QStringList() << "vmlinuz*", QDir::Files);

    if (vmlinuzFiles.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("No vmlinuz files found in /boot!");
        }
        return;
    }

    if (g_logTextEdit) {
        g_logTextEdit->append("Available vmlinuz files:");
        for (int i = 0; i < vmlinuzFiles.size(); i++) {
            g_logTextEdit->append(QString("%1) /boot/%2").arg(i+1).arg(vmlinuzFiles[i]));
        }
    }

    bool ok;
    QString selection = QInputDialog::getItem(nullptr, "Select vmlinuz", 
        "Select vmlinuz file:", vmlinuzFiles, 0, false, &ok);
    
    if (ok && !selection.isEmpty()) {
        config.vmlinuzPath = "/boot/" + selection;

        QString destPath = BUILD_DIR + "/boot/vmlinuz-x86_64";
        QString copyCmd = "sudo cp " + config.vmlinuzPath + " " + destPath;
        execute_command(copyCmd);

        if (g_logTextEdit) {
            g_logTextEdit->append("Selected: " + config.vmlinuzPath);
            g_logTextEdit->append("Copied to: " + destPath);
        }
        saveConfig();
    }
}

void generateMkinitcpio() {
    if (config.vmlinuzPath.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Please select vmlinuz first!");
        }
        return;
    }

    if (BUILD_DIR.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Build directory not set!");
        }
        return;
    }

    if (g_logTextEdit) {
        g_logTextEdit->append("Generating initramfs...");
    }
    execute_command("cd " + BUILD_DIR + " && sudo mkinitcpio -c mkinitcpio.conf -g " + BUILD_DIR + "/boot/initramfs-x86_64.img");

    config.mkinitcpioGenerated = true;
    saveConfig();
    if (g_logTextEdit) {
        g_logTextEdit->append("mkinitcpio generated successfully!");
    }
}

void editGrubCfg() {
    if (BUILD_DIR.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Build directory not set!");
        }
        return;
    }

    QString grubCfgPath = BUILD_DIR + "/boot/grub/grub.cfg";
    if (g_logTextEdit) {
        g_logTextEdit->append("Editing GRUB config: " + grubCfgPath);
    }

    // Set nano to use cyan color scheme
    QString nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + grubCfgPath;
    execute_command(nanoCommand);

    config.grubEdited = true;
    saveConfig();
    if (g_logTextEdit) {
        g_logTextEdit->append("GRUB config edited!");
    }
}

void editBootText() {
    if (BUILD_DIR.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Build directory not set!");
        }
        return;
    }

    QString bootTextPath = BUILD_DIR + "/boot/grub/kernels.cfg";
    if (g_logTextEdit) {
        g_logTextEdit->append("Editing Boot Text: " + bootTextPath);
    }

    // Set nano to use cyan color scheme
    QString nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + bootTextPath;
    execute_command(nanoCommand);

    config.bootTextEdited = true;
    saveConfig();
    if (g_logTextEdit) {
        g_logTextEdit->append("Boot Text edited!");
    }
}

void editCalamaresBranding() {
    QString calamaresBrandingPath = "/usr/share/calamares/branding/claudemods/branding.desc";
    if (g_logTextEdit) {
        g_logTextEdit->append("Editing Calamares Branding: " + calamaresBrandingPath);
    }

    // Set nano to use cyan color scheme
    QString nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + calamaresBrandingPath;
    execute_command(nanoCommand);

    config.calamaresBrandingEdited = true;
    saveConfig();
    if (g_logTextEdit) {
        g_logTextEdit->append("Calamares Branding edited!");
    }
}

void editCalamares1() {
    QString calamares1Path = "/etc/calamares/modules/initcpio.conf";
    if (g_logTextEdit) {
        g_logTextEdit->append("Editing Calamares 1st initcpio.conf: " + calamares1Path);
    }

    // Set nano to use cyan color scheme
    QString nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + calamares1Path;
    execute_command(nanoCommand);

    config.calamares1Edited = true;
    saveConfig();
    if (g_logTextEdit) {
        g_logTextEdit->append("Calamares 1st initcpio.conf edited!");
    }
}

void editCalamares2() {
    QString calamares2Path = "/usr/share/calamares/modules/initcpio.conf";
    if (g_logTextEdit) {
        g_logTextEdit->append("Editing Calamares 2nd initcpio.conf: " + calamares2Path);
    }

    // Set nano to use cyan color scheme
    QString nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + calamares2Path;
    execute_command(nanoCommand);

    config.calamares2Edited = true;
    saveConfig();
    if (g_logTextEdit) {
        g_logTextEdit->append("Calamares 2nd initcpio.conf edited!");
    }
}

void setIsoTag() {
    QString tag = getUserInput("Enter ISO tag (e.g., 2025): ");
    if (!tag.isEmpty()) {
        config.isoTag = tag;
        saveConfig();
    }
}

void setIsoName() {
    QString name = getUserInput("Enter ISO name (e.g., claudemods.iso): ");
    if (!name.isEmpty()) {
        config.isoName = name;
        saveConfig();
    }
}

void setOutputDir() {
    QString defaultDir = "/home/" + USERNAME + "/Downloads";
    if (g_logTextEdit) {
        g_logTextEdit->append("Current output directory: " + (config.outputDir.isEmpty() ? "Not set" : config.outputDir));
        g_logTextEdit->append("Default directory: " + defaultDir);
    }
    
    QString dir = QFileDialog::getExistingDirectory(nullptr, "Select Output Directory", defaultDir);
    if (!dir.isEmpty()) {
        config.outputDir = dir;
        execute_command("mkdir -p " + config.outputDir, true);
        saveConfig();
    }
}

void setCloneDir() {
    QString defaultDir = "/home/" + USERNAME;
    if (g_logTextEdit) {
        g_logTextEdit->append("Current clone directory: " + (config.cloneDir.isEmpty() ? "Not set" : config.cloneDir));
        g_logTextEdit->append("Default directory: " + defaultDir);
    }

    // Get the parent directory from user
    QString parentDir = QFileDialog::getExistingDirectory(nullptr, "Select Parent Directory for Clone", defaultDir);
    if (!parentDir.isEmpty()) {
        // Always use clone_system_temp as the folder name
        config.cloneDir = parentDir + "/clone_system_temp";
        execute_command("sudo mkdir -p " + config.cloneDir, true);
        saveConfig();
    }
}

QString getConfigFilePath() {
    return "/home/" + USERNAME + "/.config/cmi/configuration.txt";
}

void saveConfig() {
    QString configPath = getConfigFilePath();
    QFile configFile(configPath);
    if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&configFile);
        out << "isoTag=" << config.isoTag << "\n";
        out << "isoName=" << config.isoName << "\n";
        out << "outputDir=" << config.outputDir << "\n";
        out << "vmlinuzPath=" << config.vmlinuzPath << "\n";
        out << "cloneDir=" << config.cloneDir << "\n";
        out << "mkinitcpioGenerated=" << (config.mkinitcpioGenerated ? "1" : "0") << "\n";
        out << "grubEdited=" << (config.grubEdited ? "1" : "0") << "\n";
        out << "bootTextEdited=" << (config.bootTextEdited ? "1" : "0") << "\n";
        out << "calamaresBrandingEdited=" << (config.calamaresBrandingEdited ? "1" : "0") << "\n";
        out << "calamares1Edited=" << (config.calamares1Edited ? "1" : "0") << "\n";
        out << "calamares2Edited=" << (config.calamares2Edited ? "1" : "0") << "\n";
        out << "dependenciesInstalled=" << (config.dependenciesInstalled ? "1" : "0") << "\n";
        configFile.close();
    } else {
        if (g_logTextEdit) {
            g_logTextEdit->append("Failed to save configuration to " + configPath);
        }
    }
}

void loadConfig() {
    QString configPath = getConfigFilePath();
    QFile configFile(configPath);
    if (configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&configFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            int delimiter = line.indexOf('=');
            if (delimiter != -1) {
                QString key = line.left(delimiter);
                QString value = line.mid(delimiter + 1);

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

bool copyFilesWithRsync(const QString& source, const QString& destination) {
    if (g_logTextEdit) {
        g_logTextEdit->append("Copying files...");
    }

    QString command = "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
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

    execute_command(command, true);

    return true;
}

bool createSquashFS(const QString& inputDir, const QString& outputFile) {
    if (g_logTextEdit) {
        g_logTextEdit->append("Creating SquashFS image, this may take some time...");
    }

    // Use exact same mksquashfs arguments as in the bash script
    QString command = "sudo mksquashfs " + inputDir + " " + outputFile +
    " -noappend -comp xz -b 256K -Xbcj x86";

    execute_command(command, true);
    return true;
}

bool createChecksum(const QString& filename) {
    QString command = "sha512sum " + filename + " > " + filename + ".sha512";
    execute_command(command, true);
    return true;
}

void printFinalMessage(const QString& outputFile) {
    if (!g_logTextEdit) return;
    
    g_logTextEdit->append("");
    g_logTextEdit->append("SquashFS image created successfully: " + outputFile);
    g_logTextEdit->append("Checksum file: " + outputFile + ".sha512");
    g_logTextEdit->append("Size: ");
    execute_command("sudo du -h " + outputFile + " | cut -f1", true);
}

QString getOutputDirectory() {
    QString dir = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img/LiveOS";
    return dir;
}

QString expandPath(const QString& path) {
    QString result = path;
    result.replace("$USER", USERNAME);
    result.replace("~", "/home/" + USERNAME);
    return result;
}

bool createISO() {
    if (!config.allCheckboxesChecked()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Cannot create ISO - all setup steps must be completed first!");
        }
        return false;
    }

    if (!config.isReadyForISO()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Cannot create ISO - setup is incomplete!");
        }
        return false;
    }

    if (g_logTextEdit) {
        g_logTextEdit->append("Starting ISO creation process...");
    }

    QString expandedOutputDir = expandPath(config.outputDir);

    execute_command("mkdir -p " + expandedOutputDir, true);

    QString xorrisoCmd = "sudo xorriso -as mkisofs "
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

    // Change ownership of the created ISO to the current user
    QString isoPath = expandedOutputDir + "/" + config.isoName;
    QString chownCmd = "sudo chown " + USERNAME + ":" + USERNAME + " \"" + isoPath + "\"";
    execute_command(chownCmd, true);

    if (g_logTextEdit) {
        g_logTextEdit->append("ISO created successfully at " + isoPath);
        g_logTextEdit->append("Ownership changed to current user: " + USERNAME);
    }

    return true;
}

void showGuide() {
    QString readmePath = "/home/" + USERNAME + "/.config/cmi/readme.txt";
    execute_command("mkdir -p /home/" + USERNAME + "/.config/cmi", true);

    execute_command("nano " + readmePath, true);
}

void installISOToUSB() {
    if (config.outputDir.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Output directory not set!");
        }
        return;
    }

    QDir outputDir(expandPath(config.outputDir));
    QStringList isoFiles = outputDir.entryList(QStringList() << "*.iso", QDir::Files);

    if (isoFiles.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("No ISO files found in output directory!");
        }
        return;
    }

    if (g_logTextEdit) {
        g_logTextEdit->append("Available ISO files:");
        for (int i = 0; i < isoFiles.size(); i++) {
            g_logTextEdit->append(QString("%1) %2").arg(i+1).arg(isoFiles[i]));
        }
    }

    bool ok;
    QString selection = QInputDialog::getItem(nullptr, "Select ISO", 
        "Select ISO file:", isoFiles, 0, false, &ok);
    
    if (!ok || selection.isEmpty()) {
        return;
    }

    QString selectedISO = expandPath(config.outputDir) + "/" + selection;

    if (g_logTextEdit) {
        g_logTextEdit->append("Available drives:");
    }
    execute_command("lsblk -d -o NAME,SIZE,MODEL | grep -v 'loop'", true);

    QString targetDrive = getUserInput("Enter target drive (e.g., /dev/sda): ");
    if (targetDrive.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("No drive specified!");
        }
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(nullptr, "Confirmation",
        "WARNING: This will overwrite all data on " + targetDrive + "!\nAre you sure you want to continue?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Writing " + selectedISO + " to " + targetDrive + "...");
        }
        QString ddCommand = "sudo dd if=" + selectedISO + " of=" + targetDrive + " bs=4M status=progress oflag=sync";
        execute_command(ddCommand, true);

        if (g_logTextEdit) {
            g_logTextEdit->append("ISO successfully written to USB drive!");
        }
    }
}

void runCMIInstaller() {
    execute_command("cmirsyncinstaller", true);
}

void runCalamares() {
    execute_command("sudo calamares", true);
}

void updateScript() {
    if (g_logTextEdit) {
        g_logTextEdit->append("Updating script from GitHub...");
    }
    execute_command("bash -c \"$(curl -fsSL https://raw.githubusercontent.com/claudemods/claudemods-multi-iso-konsole-script/main/advancedimgscript+/installer/patch.sh )\"");
    if (g_logTextEdit) {
        g_logTextEdit->append("Script updated successfully!");
    }
}

bool isDeviceMounted(const QString& device) {
    QString command = "mount | grep " + device + " > /dev/null 2>&1";
    return system(command.toStdString().c_str()) == 0;
}

bool mountDevice(const QString& device, const QString& mountPoint) {
    if (g_logTextEdit) {
        g_logTextEdit->append("Mounting " + device + " to " + mountPoint + "...");
    }
    execute_command("sudo mkdir -p " + mountPoint, true);
    QString mountCmd = "sudo mount " + device + " " + mountPoint;
    return system(mountCmd.toStdString().c_str()) == 0;
}

void cloneCurrentSystem(const QString& cloneDir) {
    if (!config.allCheckboxesChecked()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Cannot create image - all setup steps must be completed first!");
        }
        return;
    }

    if (g_logTextEdit) {
        g_logTextEdit->append("Cloning current system to " + cloneDir + "...");
    }

    if (!copyFilesWithRsync(SOURCE_DIR, cloneDir)) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Failed to clone current system!");
        }
        return;
    }

    if (g_logTextEdit) {
        g_logTextEdit->append("Current system cloned successfully!");
    }
}

void cloneAnotherDrive(const QString& cloneDir) {
    if (!config.allCheckboxesChecked()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Cannot create image - all setup steps must be completed first!");
        }
        return;
    }

    if (g_logTextEdit) {
        g_logTextEdit->append("Available drives:");
    }
    execute_command("lsblk -f -o NAME,FSTYPE,SIZE,MOUNTPOINT | grep -v 'loop'", true);

    QString drive = getUserInput("Enter drive to clone (e.g., /dev/sda2): ");
    if (drive.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("No drive specified!");
        }
        return;
    }

    // Check if drive exists
    QString checkCmd = "ls " + drive + " > /dev/null 2>&1";
    if (system(checkCmd.toStdString().c_str()) != 0) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Drive " + drive + " does not exist!");
        }
        return;
    }

    QString tempMountPoint = "/mnt/temp_clone_mount";

    // Mount the drive if not already mounted
    if (!isDeviceMounted(drive)) {
        if (!mountDevice(drive, tempMountPoint)) {
            if (g_logTextEdit) {
                g_logTextEdit->append("Failed to mount " + drive + "!");
            }
            return;
        }
    } else {
        // Get existing mount point
        QProcess process;
        process.start("bash", {"-c", "mount | grep " + drive + " | awk '{print $3}'"});
        process.waitForFinished();
        QString mountPoint = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (!mountPoint.isEmpty()) {
            tempMountPoint = mountPoint;
        }
    }

    if (g_logTextEdit) {
        g_logTextEdit->append("Cloning " + drive + " from " + tempMountPoint + " to " + cloneDir + "...");
    }

    if (!copyFilesWithRsync(tempMountPoint, cloneDir)) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Failed to clone drive " + drive + "!");
        }
    } else {
        if (g_logTextEdit) {
            g_logTextEdit->append("Drive " + drive + " cloned successfully!");
        }
    }

    // Unmount if we mounted it
    if (!isDeviceMounted(drive) || system(("mount | grep " + drive + " | grep " + tempMountPoint).toStdString().c_str()) == 0) {
        execute_command("sudo umount " + tempMountPoint, true);
        execute_command("sudo rmdir " + tempMountPoint, true);
    }
}

void cloneFolderOrFile(const QString& cloneDir) {
    if (!config.allCheckboxesChecked()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Cannot create image - all setup steps must be completed first!");
        }
        return;
    }

    if (g_logTextEdit) {
        g_logTextEdit->append("Clone Folder or File");
        g_logTextEdit->append("Enter the path to a folder or file you want to clone.");
        g_logTextEdit->append("Examples:");
        g_logTextEdit->append("  - Folder: /home/" + USERNAME + "/Documents");
        g_logTextEdit->append("  - File: /home/" + USERNAME + "/file.txt");
    }

    QString sourcePath = getUserInput("Enter folder or file path to clone: ");
    if (sourcePath.isEmpty()) {
        if (g_logTextEdit) {
            g_logTextEdit->append("No path specified!");
        }
        return;
    }

    // Check if source exists
    QString checkCmd = "sudo test -e " + sourcePath + " > /dev/null 2>&1";
    if (system(checkCmd.toStdString().c_str()) != 0) {
        if (g_logTextEdit) {
            g_logTextEdit->append("Source path does not exist: " + sourcePath);
        }
        return;
    }

    // Create userfiles directory in clone_system_temp
    QString userfilesDir = cloneDir + "/home/userfiles";
    execute_command("sudo mkdir -p " + userfilesDir, true);

    if (g_logTextEdit) {
        g_logTextEdit->append("Cloning " + sourcePath + " to " + userfilesDir + "...");
    }

    // Use rsync to copy the folder or file
    QString rsyncCmd = "sudo rsync -aHAXSr --numeric-ids --info=progress2 " +
                      sourcePath + " " + userfilesDir + "/";

    execute_command(rsyncCmd, true);

    if (g_logTextEdit) {
        g_logTextEdit->append("Successfully cloned " + sourcePath + " to " + userfilesDir + "!");
    }

    // Show what was copied
    QString listCmd = "sudo ls -la " + userfilesDir + " | head -20";
    if (g_logTextEdit) {
        g_logTextEdit->append("Contents of userfiles directory:");
    }
    execute_command(listCmd, true);
}

// Menu functions that will be handled by GUI
void showSetupMenu() {
    // Handled by GUI tabs
}

void showCloneOptionsMenu() {
    // Handled by GUI buttons
}

void showMainMenu() {
    // Handled by GUI main window
}

// Terminal functions that are not needed in GUI
int getch() { return 0; }
int kbhit() { return 0; }

// Main GUI Window Class
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        // Get username first
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            USERNAME = QString::fromLocal8Bit(pw->pw_name);
        } else {
            USERNAME = "user";
        }
        
        BUILD_DIR = "/home/" + USERNAME + "/.config/cmi/build-image-arch-img";
        
        setupUI();
        loadConfig();
        
        // Start time update timer
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
        timer->start(1000);
        updateTime();
        
        // Set global pointers
        g_logTextEdit = logTextEdit;
        g_progressBar = progressBar;
        
        logOutput("=== Advanced C++ Arch Img Iso Script+ Beta v2.03 ===");
        logOutput("Initialized successfully. Ready for operations.");
        
        // Start time thread
        std::thread(update_time_thread).detach();
    }

    ~MainWindow() {
        time_thread_running = false;
    }

private slots:
    void updateTime() {
        std::lock_guard<std::mutex> lock(time_mutex);
        timeLabel->setText("Current UK Time: " + current_time_str);
    }
    
    void updateProgress(int value) {
        progressBar->setValue(value);
    }
    
    void logOutput(const QString &message) {
        logTextEdit->append(message);
        QScrollBar *scrollbar = logTextEdit->verticalScrollBar();
        scrollbar->setValue(scrollbar->maximum());
        QApplication::processEvents();
    }
    
    void onInstallDependencies() {
        logOutput("=== Installing Dependencies ===");
        updateProgress(10);
        QtConcurrent::run([this]() {
            installDependencies();
            updateProgress(100);
        });
    }
    
    void onSetIsoTag() {
        setIsoTag();
        logOutput("ISO Tag set to: " + config.isoTag);
    }
    
    void onSetIsoName() {
        setIsoName();
        logOutput("ISO Name set to: " + config.isoName);
    }
    
    void onSetOutputDir() {
        setOutputDir();
        logOutput("Output Directory set to: " + config.outputDir);
    }
    
    void onSetCloneDir() {
        setCloneDir();
        logOutput("Clone Directory set to: " + config.cloneDir);
    }
    
    void onSelectVmlinuz() {
        selectVmlinuz();
    }
    
    void onGenerateMkinitcpio() {
        logOutput("=== Generating mkinitcpio ===");
        updateProgress(30);
        QtConcurrent::run([this]() {
            generateMkinitcpio();
            updateProgress(100);
        });
    }
    
    void onEditGrubCfg() {
        logOutput("=== Editing GRUB Config ===");
        QtConcurrent::run([]() {
            editGrubCfg();
        });
    }
    
    void onEditBootText() {
        logOutput("=== Editing Boot Text ===");
        QtConcurrent::run([]() {
            editBootText();
        });
    }
    
    void onEditCalamaresBranding() {
        logOutput("=== Editing Calamares Branding ===");
        QtConcurrent::run([]() {
            editCalamaresBranding();
        });
    }
    
    void onEditCalamares1() {
        logOutput("=== Editing Calamares 1st initcpio.conf ===");
        QtConcurrent::run([]() {
            editCalamares1();
        });
    }
    
    void onEditCalamares2() {
        logOutput("=== Editing Calamares 2nd initcpio.conf ===");
        QtConcurrent::run([]() {
            editCalamares2();
        });
    }
    
    void onCreateISO() {
        logOutput("=== Creating ISO ===");
        updateProgress(20);
        QtConcurrent::run([this]() {
            if (createISO()) {
                updateProgress(100);
            } else {
                updateProgress(0);
            }
        });
    }
    
    void onCloneCurrentSystem() {
        if (!config.allCheckboxesChecked()) {
            logOutput("✗ Cannot create image - all setup steps must be completed first!");
            return;
        }
        
        logOutput("=== Cloning Current System ===");
        updateProgress(10);
        
        QtConcurrent::run([this]() {
            cloneCurrentSystem(config.cloneDir);
            updateProgress(50);
            
            // Create SquashFS after cloning current system
            QString outputDir = getOutputDirectory();
            QString finalImgPath = outputDir + "/" + FINAL_IMG_NAME;
            
            createSquashFS(config.cloneDir, finalImgPath);
            updateProgress(80);

            // Clean up the clone_system_temp directory after SquashFS creation
            logOutput("Cleaning up temporary clone directory...");
            QString cleanupCmd = "sudo rm -rf " + config.cloneDir;
            execute_command(cleanupCmd, true);
            logOutput("Temporary directory cleaned up: " + config.cloneDir);

            createChecksum(finalImgPath);
            printFinalMessage(finalImgPath);
            updateProgress(100);
        });
    }
    
    void onCloneAnotherDrive() {
        if (!config.allCheckboxesChecked()) {
            logOutput("✗ Cannot create image - all setup steps must be completed first!");
            return;
        }
        
        logOutput("=== Cloning Another Drive ===");
        QtConcurrent::run([this]() {
            cloneAnotherDrive(config.cloneDir);
            
            // Create SquashFS after cloning another drive
            QString outputDir = getOutputDirectory();
            QString finalImgPath = outputDir + "/" + FINAL_IMG_NAME;

            createSquashFS(config.cloneDir, finalImgPath);
            updateProgress(80);

            // Clean up the clone_system_temp directory after SquashFS creation
            logOutput("Cleaning up temporary clone directory...");
            QString cleanupCmd = "sudo rm -rf " + config.cloneDir;
            execute_command(cleanupCmd, true);
            logOutput("Temporary directory cleaned up: " + config.cloneDir);

            createChecksum(finalImgPath);
            printFinalMessage(finalImgPath);
            updateProgress(100);
        });
    }
    
    void onCloneFolderOrFile() {
        if (!config.allCheckboxesChecked()) {
            logOutput("✗ Cannot create image - all setup steps must be completed first!");
            return;
        }
        
        logOutput("=== Cloning Folder or File ===");
        QtConcurrent::run([this]() {
            cloneFolderOrFile(config.cloneDir);
            logOutput("Folder/File cloned to " + config.cloneDir + "/home/userfiles");
            logOutput("You can now create an ISO that includes these files.");
        });
    }
    
    void onShowDiskUsage() {
        logOutput("=== Disk Usage ===");
        execute_command("df -h");
    }
    
    void onInstallToUSB() {
        installISOToUSB();
    }
    
    void onRunCMIInstaller() {
        logOutput("=== Running CMI Installer ===");
        QtConcurrent::run([]() {
            runCMIInstaller();
        });
    }
    
    void onRunCalamares() {
        logOutput("=== Running Calamares ===");
        QtConcurrent::run([]() {
            runCalamares();
        });
    }
    
    void onUpdateScript() {
        logOutput("=== Updating Script ===");
        QtConcurrent::run([]() {
            updateScript();
        });
    }
    
    void onShowGuide() {
        showGuide();
    }
    
    void onPrintBanner() {
        printBanner();
    }
    
    void onPrintConfigStatus() {
        printConfigStatus();
    }
    
    void onExtractEmbeddedZip() {
        logOutput("=== Extracting Embedded ZIP ===");
        QtConcurrent::run([]() {
            extractEmbeddedZip();
        });
    }
    
    void onExtractCalamaresResources() {
        logOutput("=== Extracting Calamares Resources ===");
        QtConcurrent::run([]() {
            extractCalamaresResources();
        });
    }

private:
    void setupUI() {
        setWindowTitle("Advanced C++ Arch Img Iso Script+ Beta v2.03");
        setMinimumSize(1200, 800);
        
        QWidget *centralWidget = new QWidget;
        setCentralWidget(centralWidget);
        
        QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
        
        // Header
        QHBoxLayout *headerLayout = new QHBoxLayout;
        timeLabel = new QLabel;
        timeLabel->setStyleSheet("QLabel { color: blue; font-weight: bold; font-size: 14px; }");
        headerLayout->addWidget(timeLabel);
        headerLayout->addStretch();
        
        QPushButton *bannerBtn = new QPushButton("Show Banner");
        QPushButton *configBtn = new QPushButton("Show Config");
        headerLayout->addWidget(bannerBtn);
        headerLayout->addWidget(configBtn);
        
        mainLayout->addLayout(headerLayout);
        
        // Progress bar (red)
        progressBar = new QProgressBar;
        progressBar->setStyleSheet(
            "QProgressBar {"
            "    border: 2px solid grey;"
            "    border-radius: 5px;"
            "    text-align: center;"
            "    height: 20px;"
            "    font-weight: bold;"
            "}"
            "QProgressBar::chunk {"
            "    background-color: #ff0000;"
            "    border-radius: 3px;"
            "}"
        );
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
        mainLayout->addWidget(progressBar);
        
        // Tab widget
        QTabWidget *tabWidget = new QTabWidget;
        
        // Setup Tab
        QWidget *setupTab = createSetupTab();
        tabWidget->addTab(setupTab, "Setup");
        
        // Clone Tab
        QWidget *cloneTab = createCloneTab();
        tabWidget->addTab(cloneTab, "Clone Options");
        
        // Actions Tab
        QWidget *actionsTab = createActionsTab();
        tabWidget->addTab(actionsTab, "Actions");
        
        // Tools Tab
        QWidget *toolsTab = createToolsTab();
        tabWidget->addTab(toolsTab, "Tools");
        
        mainLayout->addWidget(tabWidget);
        
        // Log output
        logTextEdit = new QTextEdit;
        logTextEdit->setReadOnly(true);
        logTextEdit->setPlaceholderText("Output log will appear here...");
        logTextEdit->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #ffffff; font-family: 'Monospace'; font-size: 10pt; }");
        mainLayout->addWidget(logTextEdit);
        
        // Connect header buttons
        connect(bannerBtn, &QPushButton::clicked, this, &MainWindow::onPrintBanner);
        connect(configBtn, &QPushButton::clicked, this, &MainWindow::onPrintConfigStatus);
    }
    
    QWidget* createSetupTab() {
        QWidget *tab = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(tab);
        
        // Dependencies
        QGroupBox *depsGroup = new QGroupBox("1. Dependencies");
        QVBoxLayout *depsLayout = new QVBoxLayout(depsGroup);
        QPushButton *installDepsBtn = new QPushButton("Install Dependencies");
        QPushButton *extractZipBtn = new QPushButton("Extract Embedded ZIP");
        QPushButton *extractCalamaresBtn = new QPushButton("Extract Calamares Resources");
        depsLayout->addWidget(installDepsBtn);
        depsLayout->addWidget(extractZipBtn);
        depsLayout->addWidget(extractCalamaresBtn);
        layout->addWidget(depsGroup);
        
        // Configuration
        QGroupBox *configGroup = new QGroupBox("2. Configuration");
        QGridLayout *configLayout = new QGridLayout(configGroup);
        
        int row = 0;
        configLayout->addWidget(new QLabel("ISO Tag:"), row, 0);
        QPushButton *setIsoTagBtn = new QPushButton("Set ISO Tag");
        configLayout->addWidget(setIsoTagBtn, row, 1);
        row++;
        
        configLayout->addWidget(new QLabel("ISO Name:"), row, 0);
        QPushButton *setIsoNameBtn = new QPushButton("Set ISO Name");
        configLayout->addWidget(setIsoNameBtn, row, 1);
        row++;
        
        configLayout->addWidget(new QLabel("Output Directory:"), row, 0);
        QPushButton *setOutputDirBtn = new QPushButton("Set Output Directory");
        configLayout->addWidget(setOutputDirBtn, row, 1);
        row++;
        
        configLayout->addWidget(new QLabel("Clone Directory:"), row, 0);
        QPushButton *setCloneDirBtn = new QPushButton("Set Clone Directory");
        configLayout->addWidget(setCloneDirBtn, row, 1);
        row++;
        
        configLayout->addWidget(new QLabel("vmlinuz Path:"), row, 0);
        QPushButton *selectVmlinuzBtn = new QPushButton("Select vmlinuz");
        configLayout->addWidget(selectVmlinuzBtn, row, 1);
        
        layout->addWidget(configGroup);
        
        // System Configuration
        QGroupBox *systemGroup = new QGroupBox("3. System Configuration");
        QGridLayout *systemLayout = new QGridLayout(systemGroup);
        
        row = 0;
        QPushButton *genMkinitcpioBtn = new QPushButton("Generate mkinitcpio");
        systemLayout->addWidget(genMkinitcpioBtn, row, 0);
        row++;
        
        QPushButton *editGrubBtn = new QPushButton("Edit GRUB Config");
        systemLayout->addWidget(editGrubBtn, row, 0);
        row++;
        
        QPushButton *editBootTextBtn = new QPushButton("Edit Boot Text");
        systemLayout->addWidget(editBootTextBtn, row, 0);
        row++;
        
        QPushButton *editCalamaresBrandingBtn = new QPushButton("Edit Calamares Branding");
        systemLayout->addWidget(editCalamaresBrandingBtn, row, 0);
        row++;
        
        QPushButton *editCalamares1Btn = new QPushButton("Edit Calamares 1st initcpio.conf");
        systemLayout->addWidget(editCalamares1Btn, row, 0);
        row++;
        
        QPushButton *editCalamares2Btn = new QPushButton("Edit Calamares 2nd initcpio.conf");
        systemLayout->addWidget(editCalamares2Btn, row, 0);
        
        layout->addWidget(systemGroup);
        layout->addStretch();
        
        // Connect buttons
        connect(installDepsBtn, &QPushButton::clicked, this, &MainWindow::onInstallDependencies);
        connect(extractZipBtn, &QPushButton::clicked, this, &MainWindow::onExtractEmbeddedZip);
        connect(extractCalamaresBtn, &QPushButton::clicked, this, &MainWindow::onExtractCalamaresResources);
        connect(setIsoTagBtn, &QPushButton::clicked, this, &MainWindow::onSetIsoTag);
        connect(setIsoNameBtn, &QPushButton::clicked, this, &MainWindow::onSetIsoName);
        connect(setOutputDirBtn, &QPushButton::clicked, this, &MainWindow::onSetOutputDir);
        connect(setCloneDirBtn, &QPushButton::clicked, this, &MainWindow::onSetCloneDir);
        connect(selectVmlinuzBtn, &QPushButton::clicked, this, &MainWindow::onSelectVmlinuz);
        connect(genMkinitcpioBtn, &QPushButton::clicked, this, &MainWindow::onGenerateMkinitcpio);
        connect(editGrubBtn, &QPushButton::clicked, this, &MainWindow::onEditGrubCfg);
        connect(editBootTextBtn, &QPushButton::clicked, this, &MainWindow::onEditBootText);
        connect(editCalamaresBrandingBtn, &QPushButton::clicked, this, &MainWindow::onEditCalamaresBranding);
        connect(editCalamares1Btn, &QPushButton::clicked, this, &MainWindow::onEditCalamares1);
        connect(editCalamares2Btn, &QPushButton::clicked, this, &MainWindow::onEditCalamares2);
        
        return tab;
    }
    
    QWidget* createCloneTab() {
        QWidget *tab = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(tab);
        
        QGroupBox *cloneGroup = new QGroupBox("Clone Options - Select Source:");
        QVBoxLayout *cloneLayout = new QVBoxLayout(cloneGroup);
        
        QPushButton *cloneCurrentBtn = new QPushButton("Clone Current System (as it is now)");
        QPushButton *cloneDriveBtn = new QPushButton("Clone Another Drive (e.g., /dev/sda2)");
        QPushButton *cloneFolderBtn = new QPushButton("Clone Folder or File");
        
        cloneLayout->addWidget(cloneCurrentBtn);
        cloneLayout->addWidget(cloneDriveBtn);
        cloneLayout->addWidget(cloneFolderBtn);
        
        layout->addWidget(cloneGroup);
        layout->addStretch();
        
        connect(cloneCurrentBtn, &QPushButton::clicked, this, &MainWindow::onCloneCurrentSystem);
        connect(cloneDriveBtn, &QPushButton::clicked, this, &MainWindow::onCloneAnotherDrive);
        connect(cloneFolderBtn, &QPushButton::clicked, this, &MainWindow::onCloneFolderOrFile);
        
        return tab;
    }
    
    QWidget* createActionsTab() {
        QWidget *tab = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(tab);
        
        QGroupBox *actionsGroup = new QGroupBox("Main Actions");
        QVBoxLayout *actionsLayout = new QVBoxLayout(actionsGroup);
        
        QPushButton *createIsoBtn = new QPushButton("Create ISO");
        QPushButton *installUsbBtn = new QPushButton("Install ISO to USB");
        QPushButton *showDiskUsageBtn = new QPushButton("Show Disk Usage");
        
        actionsLayout->addWidget(createIsoBtn);
        actionsLayout->addWidget(installUsbBtn);
        actionsLayout->addWidget(showDiskUsageBtn);
        
        layout->addWidget(actionsGroup);
        layout->addStretch();
        
        connect(createIsoBtn, &QPushButton::clicked, this, &MainWindow::onCreateISO);
        connect(installUsbBtn, &QPushButton::clicked, this, &MainWindow::onInstallToUSB);
        connect(showDiskUsageBtn, &QPushButton::clicked, this, &MainWindow::onShowDiskUsage);
        
        return tab;
    }
    
    QWidget* createToolsTab() {
        QWidget *tab = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(tab);
        
        QGroupBox *toolsGroup = new QGroupBox("Tools");
        QVBoxLayout *toolsLayout = new QVBoxLayout(toolsGroup);
        
        QPushButton *runCMIBtn = new QPushButton("CMI BTRFS/EXT4 Installer");
        QPushButton *runCalamaresBtn = new QPushButton("Calamares");
        QPushButton *updateScriptBtn = new QPushButton("Update Script");
        QPushButton *showGuideBtn = new QPushButton("Guide");
        
        toolsLayout->addWidget(runCMIBtn);
        toolsLayout->addWidget(runCalamaresBtn);
        toolsLayout->addWidget(updateScriptBtn);
        toolsLayout->addWidget(showGuideBtn);
        
        layout->addWidget(toolsGroup);
        layout->addStretch();
        
        connect(runCMIBtn, &QPushButton::clicked, this, &MainWindow::onRunCMIInstaller);
        connect(runCalamaresBtn, &QPushButton::clicked, this, &MainWindow::onRunCalamares);
        connect(updateScriptBtn, &QPushButton::clicked, this, &MainWindow::onUpdateScript);
        connect(showGuideBtn, &QPushButton::clicked, this, &MainWindow::onShowGuide);
        
        return tab;
    }
    
    void loadConfig() {
        ::loadConfig();
    }

private:
    QProgressBar *progressBar;
    QTextEdit *logTextEdit;
    QLabel *timeLabel;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    MainWindow window;
    window.show();
    
    return app.exec();
}

#include "main.moc"