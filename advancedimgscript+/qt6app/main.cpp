analyze this script #include <QApplication>
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

// Qt includes for resource system
#include <QFile>
#include <QResource>
#include <QDir>
#include <QCoreApplication>

// Forward declarations
void saveConfig();
void execute_command(const std::string& cmd, bool continueOnError = false);
void printCheckbox(bool checked);
std::string getUserInput(const std::string& prompt);
void clearScreen();
int getch();
int kbhit();

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
std::string BUILD_DIR = "/home/$USER/.config/cmi/build-image-arch-img";
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
        process.waitForFinished(5000); // 5 second timeout
        
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

// Enhanced execute_command with proper password handling
void execute_command(const std::string& cmd, bool continueOnError) {
    // Check if command requires sudo
    bool requiresSudo = (cmd.find("sudo") == 0);
    
    if (requiresSudo && !PASSWORD_VALIDATED) {
        if (!requestSudoPassword()) {
            if (!continueOnError) {
                std::cerr << "Sudo password required but not provided!" << std::endl;
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
        std::cerr << "Error: Failed to start command: " << cmd << std::endl;
        if (!continueOnError) exit(1);
        return;
    }

    // Read output while process is running
    QByteArray output;
    while (process.waitForReadyRead(-1)) {
        output.append(process.readAll());
    }

    process.waitForFinished(-1);

    // Print output
    std::cout << output.toStdString();

    if (process.exitStatus() != QProcess::NormalExit || (process.exitCode() != 0 && !continueOnError)) {
        std::cerr << "Error executing: " << cmd << std::endl;
        if (!continueOnError) exit(1);
    } else if (process.exitCode() != 0) {
        std::cerr << "Command failed but continuing: " << cmd << std::endl;
    }
}

// Function to extract embedded zip using Qt resource system
bool extractEmbeddedZip() {
    // Get username
    struct passwd *pw = getpwuid(getuid());
    std::string username = pw ? pw->pw_name : "";
    if (username.empty()) {
        return false;
    }

    std::string configDir = "/home/" + username + "/.config/cmi";
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
    std::string extractCmd = "unzip -o " + zipPath + " -d " + extractPath + " >/dev/null 2>&1";
    if (system(extractCmd.c_str()) != 0) {
        return false;
    }

    // Clean up zip file
    execute_command("rm -f " + zipPath, true);

    return true;
}

// Function to extract Calamares resources
bool extractCalamaresResources() {
    // Get username
    struct passwd *pw = getpwuid(getuid());
    std::string username = pw ? pw->pw_name : "";
    if (username.empty()) {
        return false;
    }

    std::string configDir = "/home/" + username + "/.config/cmi";
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
    std::string extractCalamaresCmd = "unzip -o " + calamaresZipPath + " -d " + configDir + " >/dev/null 2>&1";
    if (system(extractCalamaresCmd.c_str()) != 0) {
        return false;
    }

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
    std::string extractBrandingCmd = "unzip -o " + brandingZipPath + " -d " + calamaresDir + " >/dev/null 2>&1";
    if (system(extractBrandingCmd.c_str()) != 0) {
        return false;
    }

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

void clearScreen() {
    std::cout << "\033[2J\033[1;1H";
}

int getch() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

void printCheckbox(bool checked) {
    if (checked) {
        std::cout << "[✓]";
    } else {
        std::cout << "[ ]";
    }
}

void printBanner() {
    // Clear screen and move cursor to top
    std::cout << "\033[2J\033[1;1H";

    std::cout << R"(
░█████╗░██╗░░░░░░█████╗░██╗░░░██╗██████╗░███████╗███╗░░░███╗░█████╗░██████╗░░██████╗
██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝
██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░
██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗
╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝
░╚════╝░╚══════╝╚═╝░░░░░░╚═════╝░╚══════╝░╚══════╝╚═╝░░░░░╚═╝░╚════╝░╚═════╝░╚═════╝░
)" << std::endl;
std::cout << " Advanced C++ Arch Img Iso Script+ Beta v2.03 04-10-2025" << std::endl;

{
    std::lock_guard<std::mutex> lock(time_mutex);
    std::cout << "Current UK Time: " << current_time_str << std::endl;
}

std::cout << "Filesystem      Size  Used Avail Use% Mounted on" << std::endl;
execute_command("df -h / | tail -1");
std::cout << std::endl;
}

void printConfigStatus() {
    std::cout << "Current Configuration:" << std::endl;

    std::cout << " ";
    printCheckbox(config.dependenciesInstalled);
    std::cout << " Dependencies Installed" << std::endl;

    std::cout << " ";
    printCheckbox(!config.isoTag.empty());
    std::cout << " ISO Tag: " << (config.isoTag.empty() ? "Not set" : config.isoTag) << std::endl;

    std::cout << " ";
    printCheckbox(!config.isoName.empty());
    std::cout << " ISO Name: " << (config.isoName.empty() ? "Not set" : config.isoName) << std::endl;

    std::cout << " ";
    printCheckbox(!config.outputDir.empty());
    std::cout << " Output Directory: " << (config.outputDir.empty() ? "Not set" : config.outputDir) << std::endl;

    std::cout << " ";
    printCheckbox(!config.vmlinuzPath.empty());
    std::cout << " vmlinuz Selected: " << (config.vmlinuzPath.empty() ? "Not selected" : config.vmlinuzPath) << std::endl;

    std::cout << " ";
    printCheckbox(!config.cloneDir.empty());
    std::cout << " Clone Directory: " << (config.cloneDir.empty() ? "Not set" : config.cloneDir) << std::endl;

    std::cout << " ";
    printCheckbox(config.mkinitcpioGenerated);
    std::cout << " mkinitcpio Generated" << std::endl;

    std::cout << " ";
    printCheckbox(config.grubEdited);
    std::cout << " GRUB Config Edited" << std::endl;

    std::cout << " ";
    printCheckbox(config.bootTextEdited);
    std::cout << " Boot Text Edited" << std::endl;

    std::cout << " ";
    printCheckbox(config.calamaresBrandingEdited);
    std::cout << " Calamares Branding Edited" << std::endl;

    std::cout << " ";
    printCheckbox(config.calamares1Edited);
    std::cout << " Calamares 1st initcpio.conf Edited" << std::endl;

    std::cout << " ";
    printCheckbox(config.calamares2Edited);
    std::cout << " Calamares 2nd initcpio.conf Edited" << std::endl;
}

std::string getUserInput(const std::string& prompt) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    return input;
}

void installDependencies() {
    std::cout << "\nInstalling required dependencies...\n";

    // First update the package database
    std::cout << "Updating package database...\n";
    execute_command("sudo pacman -Sy");

    std::string packages;
    for (const auto& pkg : DEPENDENCIES) {
        packages += pkg + " ";
    }

    std::string command = "sudo pacman -S --needed --noconfirm " + packages;
    execute_command(command);

    config.dependenciesInstalled = true;
    saveConfig();
    std::cout << "\nDependencies installed successfully!\n" << std::endl;
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
        std::cerr << "Could not open /boot directory" << std::endl;
        return;
    }

    if (vmlinuzFiles.empty()) {
        std::cerr << "No vmlinuz files found in /boot!" << std::endl;
        return;
    }

    std::cout << "Available vmlinuz files:" << std::endl;
    for (size_t i = 0; i < vmlinuzFiles.size(); i++) {
        std::cout << (i+1) << ") " << vmlinuzFiles[i] << std::endl;
    }

    std::string selection = getUserInput("Select vmlinuz file (1-" + std::to_string(vmlinuzFiles.size()) + "): ");
    try {
        int choice = std::stoi(selection);
        if (choice > 0 && choice <= static_cast<int>(vmlinuzFiles.size())) {
            config.vmlinuzPath = vmlinuzFiles[choice-1];

            std::string destPath = BUILD_DIR + "/boot/vmlinuz-x86_64";
            std::string copyCmd = "sudo cp " + config.vmlinuzPath + " " + destPath;
            execute_command(copyCmd);

            std::cout << "Selected: " << config.vmlinuzPath << std::endl;
            std::cout << "Copied to: " << destPath << std::endl;
            saveConfig();
        } else {
            std::cerr << "Invalid selection!" << std::endl;
        }
    } catch (...) {
        std::cerr << "Invalid input!" << std::endl;
    }
}

void generateMkinitcpio() {
    if (config.vmlinuzPath.empty()) {
        std::cerr << "Please select vmlinuz first!" << std::endl;
        return;
    }

    if (BUILD_DIR.empty()) {
        std::cerr << "Build directory not set!" << std::endl;
        return;
    }

    std::cout << "Generating initramfs..." << std::endl;
    execute_command("cd " + BUILD_DIR + " && sudo mkinitcpio -c mkinitcpio.conf -g " + BUILD_DIR + "/boot/initramfs-x86_64.img");

    config.mkinitcpioGenerated = true;
    saveConfig();
    std::cout << "mkinitcpio generated successfully!" << std::endl;
}

void editGrubCfg() {
    if (BUILD_DIR.empty()) {
        std::cerr << "Build directory not set!" << std::endl;
        return;
    }

    std::string grubCfgPath = BUILD_DIR + "/boot/grub/grub.cfg";
    std::cout << "Editing GRUB config: " << grubCfgPath << std::endl;

    // Set kate to use instead of nano
    std::string kateCommand = "kate " + grubCfgPath;
    execute_command(kateCommand);

    config.grubEdited = true;
    saveConfig();
    std::cout << "GRUB config edited!" << std::endl;
}

void editBootText() {
    if (BUILD_DIR.empty()) {
        std::cerr << "Build directory not set!" << std::endl;
        return;
    }

    std::string bootTextPath = BUILD_DIR + "/boot/grub/kernels.cfg";
    std::cout << "Editing Boot Text: " << bootTextPath << std::endl;

    // Set kate to use instead of nano
    std::string kateCommand = "kate " + bootTextPath;
    execute_command(kateCommand);

    config.bootTextEdited = true;
    saveConfig();
    std::cout << "Boot Text edited!" << std::endl;
}

void editCalamaresBranding() {
    std::string calamaresBrandingPath = "/usr/share/calamares/branding/claudemods/branding.desc";
    std::cout << "Editing Calamares Branding: " << calamaresBrandingPath << std::endl;

    // Set kate to use instead of nano
    std::string kateCommand = "sudo kate " + calamaresBrandingPath;
    execute_command(kateCommand);

    config.calamaresBrandingEdited = true;
    saveConfig();
    std::cout << "Calamares Branding edited!" << std::endl;
}

void editCalamares1() {
    std::string calamares1Path = "/etc/calamares/modules/initcpio.conf";
    std::cout << "Editing Calamares 1st initcpio.conf: " << calamares1Path << std::endl;

    // Set kate to use instead of nano
    std::string kateCommand = "sudo kate " + calamares1Path;
    execute_command(kateCommand);

    config.calamares1Edited = true;
    saveConfig();
    std::cout << "Calamares 1st initcpio.conf edited!" << std::endl;
}

void editCalamares2() {
    std::string calamares2Path = "/usr/share/calamares/modules/initcpio.conf";
    std::cout << "Editing Calamares 2nd initcpio.conf: " << calamares2Path << std::endl;

    // Set kate to use instead of nano
    std::string kateCommand = "sudo kate " + calamares2Path;
    execute_command(kateCommand);

    config.calamares2Edited = true;
    saveConfig();
    std::cout << "Calamares 2nd initcpio.conf edited!" << std::endl;
}

void setIsoTag() {
    config.isoTag = getUserInput("Enter ISO tag (e.g., 2025): ");
    saveConfig();
}

void setIsoName() {
    config.isoName = getUserInput("Enter ISO name (e.g., claudemods.iso): ");
    saveConfig();
}

void setOutputDir() {
    std::string defaultDir = "/home/" + USERNAME + "/Downloads";
    std::cout << "Current output directory: " << (config.outputDir.empty() ? "Not set" : config.outputDir) << std::endl;
    std::cout << "Default directory: " << defaultDir << std::endl;
    config.outputDir = getUserInput("Enter output directory (e.g., " + defaultDir + " or $USER/Downloads): ");

    size_t user_pos;
    if ((user_pos = config.outputDir.find("$USER")) != std::string::npos) {
        config.outputDir.replace(user_pos, 5, USERNAME);
    }

    if (config.outputDir.empty()) {
        config.outputDir = defaultDir;
    }

    execute_command("mkdir -p " + config.outputDir, true);

    saveConfig();
}

void setCloneDir() {
    std::string defaultDir = "/home/" + USERNAME;
    std::cout << "Current clone directory: " << (config.cloneDir.empty() ? "Not set" : config.cloneDir) << std::endl;
    std::cout << "Default directory: " << defaultDir << std::endl;

    // Get the parent directory from user
    std::string parentDir = getUserInput("Enter parent directory for clone_system_temp folder (e.g., " + defaultDir + " or $USER): ");

    size_t user_pos;
    if ((user_pos = parentDir.find("$USER")) != std::string::npos) {
        parentDir.replace(user_pos, 5, USERNAME);
    }

    if (parentDir.empty()) {
        parentDir = defaultDir;
    }

    // Always use clone_system_temp as the folder name
    config.cloneDir = parentDir + "/clone_system_temp";

    execute_command(" sudo mkdir -p " + config.cloneDir, true);

    saveConfig();
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
        std::cerr << "Failed to save configuration to " << configPath << std::endl;
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

bool validateSizeInput(const std::string& input) {
    try {
        double size = std::stod(input);
        return size > 0.1;  // Minimum 0.1GB
    } catch (...) {
        return false;
    }
}

bool copyFilesWithRsync(const std::string& source, const std::string& destination) {
    std::cout << "Copying files..." << std::endl;

    std::string command = "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
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

bool createSquashFS(const std::string& inputDir, const std::string& outputFile) {
    std::cout << "Creating SquashFS image, this may take some time..." << std::endl;

    // Use exact same mksquashfs arguments as in the bash script
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

void printFinalMessage(const std::string& outputFile) {
    std::cout << std::endl;
    std::cout << "SquashFS image created successfully: " << outputFile << std::endl;
    std::cout << "Checksum file: " << outputFile + ".sha512" << std::endl;
    std::cout << "Size: ";
    execute_command("sudo du -h " + outputFile + " | cut -f1", true);
    std::cout << std::endl;
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

bool createISO() {
    if (!config.allCheckboxesChecked()) {
        std::cerr << "Cannot create ISO - all setup steps must be completed first!" << std::endl;
        std::cout << "\nPress any key to continue...";
        getch();
        return false;
    }

    if (!config.isReadyForISO()) {
        std::cerr << "Cannot create ISO - setup is incomplete!" << std::endl;
        return false;
    }

    std::cout << "\nStarting ISO creation process...\n";

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

    // Change ownership of the created ISO to the current user
    std::string isoPath = expandedOutputDir + "/" + config.isoName;
    std::string chownCmd = "sudo chown " + USERNAME + ":" + USERNAME + " \"" + isoPath + "\"";
    execute_command(chownCmd, true);

    std::cout << "ISO created successfully at " << isoPath << std::endl;
    std::cout << "Ownership changed to current user: " << USERNAME << std::endl;

    return true;
}

void showGuide() {
    std::string readmePath = "/home/" + USERNAME + "/.config/cmi/readme.txt";
    execute_command("mkdir -p /home/" + USERNAME + "/.config/cmi", true);

    execute_command("kate " + readmePath, true);
}

void installISOToUSB() {
    if (config.outputDir.empty()) {
        std::cerr << "Output directory not set!" << std::endl;
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
        std::cerr << "Could not open output directory: " << expandedOutputDir << std::endl;
        return;
    }

    if (isoFiles.empty()) {
        std::cerr << "No ISO files found in output directory!" << std::endl;
        return;
    }

    std::cout << "Available ISO files:" << std::endl;
    for (size_t i = 0; i < isoFiles.size(); i++) {
        std::cout << (i+1) << ") " << isoFiles[i] << std::endl;
    }

    std::string selection = getUserInput("Select ISO file (1-" + std::to_string(isoFiles.size()) + "): ");
    int choice;
    try {
        choice = std::stoi(selection);
        if (choice < 1 || choice > static_cast<int>(isoFiles.size())) {
            std::cerr << "Invalid selection!" << std::endl;
            return;
        }
    } catch (...) {
        std::cerr << "Invalid input!" << std::endl;
        return;
    }

    std::string selectedISO = expandedOutputDir + "/" + isoFiles[choice-1];

    std::cout << "\nAvailable drives:" << std::endl;
    execute_command("lsblk -d -o NAME,SIZE,MODEL | grep -v 'loop'", true);

    std::string targetDrive = getUserInput("Enter target drive (e.g., /dev/sda): ");
    if (targetDrive.empty()) {
        std::cerr << "No drive specified!" << std::endl;
        return;
    }

    std::cout << "\nWARNING: This will overwrite all data on " << targetDrive << "!" << std::endl;
    std::string confirm = getUserInput("Are you sure you want to continue? (y/N): ");
    if (confirm != "y" && confirm != "Y") {
        std::cout << "Operation cancelled." << std::endl;
        return;
    }

    std::cout << "\nWriting " << selectedISO << " to " << targetDrive << "..." << std::endl;
    std::string ddCommand = "sudo dd if=" + selectedISO + " of=" + targetDrive + " bs=4M status=progress oflag=sync";
    execute_command(ddCommand, true);

    std::cout << "\nISO successfully written to USB drive!" << std::endl;
    std::cout << "Press any key to continue...";
    getch();
}

void runCMIInstaller() {
    execute_command("cmirsyncinstaller", true);
    std::cout << "\nPress any key to continue...";
    getch();
}

void runCalamares() {
    execute_command("sudo calamares", true);
    std::cout << "\nPress any key to continue...";
    getch();
}

void updateScript() {
    std::cout << "\nUpdating script from GitHub..." << std::endl;
    execute_command("bash -c \"$(curl -fsSL https://raw.githubusercontent.com/claudemods/claudemods-multi-iso-konsole-script/main/advancedimgscript+/installer/patch.sh )\"");
    std::cout << "\nScript updated successfully!" << std::endl;
    std::cout << "Press any key to continue...";
    getch();
}

// New function to check if a device is mounted
bool isDeviceMounted(const std::string& device) {
    std::string command = "mount | grep " + device + " > /dev/null 2>&1";
    return system(command.c_str()) == 0;
}

// New function to mount a device
bool mountDevice(const std::string& device, const std::string& mountPoint) {
    std::cout << "Mounting " << device << " to " << mountPoint << "..." << std::endl;
    execute_command("sudo mkdir -p " + mountPoint, true);
    std::string mountCmd = "sudo mount " + device + " " + mountPoint;
    return system(mountCmd.c_str()) == 0;
}

// New function to clone current system
void cloneCurrentSystem(const std::string& cloneDir) {
    if (!config.allCheckboxesChecked()) {
        std::cerr << "Cannot create image - all setup steps must be completed first!" << std::endl;
        std::cout << "\nPress any key to continue...";
        getch();
        return;
    }

    std::cout << "Cloning current system to " << cloneDir << "..." << std::endl;

    if (!copyFilesWithRsync(SOURCE_DIR, cloneDir)) {
        std::cerr << "Failed to clone current system!" << std::endl;
        return;
    }

    std::cout << "Current system cloned successfully!" << std::endl;
}

// New function to clone another drive
void cloneAnotherDrive(const std::string& cloneDir) {
    if (!config.allCheckboxesChecked()) {
        std::cerr << "Cannot create image - all setup steps must be completed first!" << std::endl;
        std::cout << "\nPress any key to continue...";
        getch();
        return;
    }

    std::cout << "\nAvailable drives:" << std::endl;
    execute_command("lsblk -f -o NAME,FSTYPE,SIZE,MOUNTPOINT | grep -v 'loop'", true);

    std::string drive = getUserInput("Enter drive to clone (e.g., /dev/sda2): ");
    if (drive.empty()) {
        std::cerr << "No drive specified!" << std::endl;
        return;
    }

    // Check if drive exists
    std::string checkCmd = "ls " + drive + " > /dev/null 2>&1";
    if (system(checkCmd.c_str()) != 0) {
        std::cerr << "Drive " << drive + " does not exist!" << std::endl;
        return;
    }

    std::string tempMountPoint = "/mnt/temp_clone_mount";

    // Mount the drive if not already mounted
    if (!isDeviceMounted(drive)) {
        if (!mountDevice(drive, tempMountPoint)) {
            std::cerr << "Failed to mount " << drive << "!" << std::endl;
            return;
        }
    } else {
        // Get existing mount point
        std::string mountCmd = "mount | grep " + drive + " | awk '{print $3}'";
        FILE* fp = popen(mountCmd.c_str(), "r");
        if (fp) {
            char mountPath[256];
            if (fgets(mountPath, sizeof(mountPath), fp)) {
                tempMountPoint = mountPath;
                // Remove newline
                tempMountPoint.erase(tempMountPoint.find_last_not_of("\n") + 1);
            }
            pclose(fp);
        }
    }

    std::cout << "Cloning " << drive << " from " << tempMountPoint << " to " << cloneDir << "..." << std::endl;

    if (!copyFilesWithRsync(tempMountPoint, cloneDir)) {
        std::cerr << "Failed to clone drive " << drive << "!" << std::endl;
    } else {
        std::cout << "Drive " << drive << " cloned successfully!" << std::endl;
    }

    // Unmount if we mounted it
    if (!isDeviceMounted(drive) || system(("mount | grep " + drive + " | grep " + tempMountPoint).c_str()) == 0) {
        execute_command("sudo umount " + tempMountPoint, true);
        execute_command("sudo rmdir " + tempMountPoint, true);
    }
}

// New function to clone folder or file using rsync
void cloneFolderOrFile(const std::string& cloneDir) {
    if (!config.allCheckboxesChecked()) {
        std::cerr << "Cannot create image - all setup steps must be completed first!" << std::endl;
        std::cout << "\nPress any key to continue...";
        getch();
        return;
    }

    std::cout << "\nClone Folder or File" << std::endl;
    std::cout << "Enter the path to a folder or file you want to clone." << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  - Folder: /home/" << USERNAME << "/Documents" << std::endl;
    std::cout << "  - File: /home/" << USERNAME << "/file.txt" << std::endl;

    std::string sourcePath = getUserInput("Enter folder or file path to clone: ");
    if (sourcePath.empty()) {
        std::cerr << "No path specified!" << std::endl;
        return;
    }

    // Check if source exists
    std::string checkCmd = "sudo test -e " + sourcePath + " > /dev/null 2>&1";
    if (system(checkCmd.c_str()) != 0) {
        std::cerr << "Source path does not exist: " << sourcePath << std::endl;
        return;
    }

    // Create userfiles directory in clone_system_temp
    std::string userfilesDir = cloneDir + "/home/userfiles";
    execute_command("sudo mkdir -p " + userfilesDir, true);

    std::cout << "Cloning " << sourcePath << " to " << userfilesDir << "..." << std::endl;

    // Use rsync to copy the folder or file
    std::string rsyncCmd = "sudo rsync -aHAXSr --numeric-ids --info=progress2 " +
    sourcePath + " " + userfilesDir + "/";

    execute_command(rsyncCmd, true);

    std::cout << "Successfully cloned " << sourcePath << " to " << userfilesDir << "!" << std::endl;

    // Show what was copied
    std::string listCmd = "sudo ls -la " + userfilesDir + " | head -20";
    std::cout << "Contents of userfiles directory:" << std::endl;
    execute_command(listCmd, true);
}

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
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);

        connect(&process, &QProcess::readyReadStandardOutput, [&]() {
            QString output = process.readAllStandardOutput();
            emit commandOutput(output);
        });

        // Check if command requires sudo
        bool requiresSudo = m_command.contains("sudo");
        
        if (requiresSudo && !PASSWORD_VALIDATED) {
            // We can't request password in worker thread, emit error
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
    }

private:
    QString m_command;
    bool m_continueOnError;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        // Get username first
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            USERNAME = pw->pw_name;
        } else {
            std::cerr << "Failed to get username!" << std::endl;
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

        // Request sudo password on startup
        QTimer::singleShot(500, this, &MainWindow::requestSudoOnStartup);
    }

    ~MainWindow() {
        time_thread_running = false;
        if (time_thread.joinable()) {
            time_thread.join();
        }
    }

private slots:
    void requestSudoOnStartup() {
        if (!requestSudoPassword()) {
            logOutputText("Sudo password is required for most operations. You will be prompted when needed.\n", "yellow");
        } else {
            logOutputText("Sudo password validated successfully!\n", "green");
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
            QString destPath = QString::fromStdString(BUILD_DIR) + "/boot/vmlinuz-x86_64";
            
            if (!PASSWORD_VALIDATED && !requestSudoPassword()) {
                logOutputText("Sudo password required for copying vmlinuz!\n", "red");
                return;
            }
            
            worker->executeCommand("sudo cp " + selected + " " + destPath, true);
            logOutputText("Selected vmlinuz: " + selected + "\n", "green");
            logOutputText("Copied to: " + destPath + "\n", "cyan");
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

        logOutputText("Generating initramfs...\n", "cyan");
        progressBar->setValue(30);
        worker->executeCommand("cd " + QString::fromStdString(BUILD_DIR) + " && sudo mkinitcpio -c mkinitcpio.conf -g " +
        QString::fromStdString(BUILD_DIR) + "/boot/initramfs-x86_64.img", true);

        config.mkinitcpioGenerated = true;
        saveConfig();
        updateConfigStatus();
    }

    void onEditGrubCfg() {
        QString grubCfgPath = QString::fromStdString(BUILD_DIR) + "/boot/grub/grub.cfg";
        logOutputText("Editing GRUB config: " + grubCfgPath + "\n", "cyan");

        QDialog editorDialog(this);
        editorDialog.setWindowTitle("Edit GRUB Configuration");
        editorDialog.setMinimumSize(600, 400);

        QVBoxLayout layout(&editorDialog);
        QTextEdit textEdit;
        QPushButton saveButton("Save");
        QPushButton cancelButton("Cancel");

        // Read current grub.cfg content
        QFile file(grubCfgPath);
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
        QString bootTextPath = QString::fromStdString(BUILD_DIR) + "/boot/grub/kernels.cfg";
        logOutputText("Editing Boot Text: " + bootTextPath + "\n", "cyan");

        QDialog editorDialog(this);
        editorDialog.setWindowTitle("Edit Boot Text");
        editorDialog.setMinimumSize(600, 400);

        QVBoxLayout layout(&editorDialog);
        QTextEdit textEdit;
        QPushButton saveButton("Save");
        QPushButton cancelButton("Cancel");

        // Read current kernels.cfg content
        QFile file(bootTextPath);
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

        worker->executeCommand(QString::fromStdString(xorrisoCmd), true);

        // Change ownership of the created ISO to the current user
        std::string isoPath = expandedOutputDir + "/" + config.isoName;
        std::string chownCmd = "sudo chown " + USERNAME + ":" + USERNAME + " \"" + isoPath + "\"";
        worker->executeCommand(QString::fromStdString(chownCmd), true);

        logOutputText("ISO created successfully at " + QString::fromStdString(isoPath) + "\n", "green");
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
        worker->executeCommand("bash -c \"$(curl -fsSL https://raw.githubusercontent.com/claudemods/claudemods-multi-iso-konsole-script/main/advancedimgscript+/installer/patch.sh)\"", true);
    }

    void onShowGuide() {
        QString readmePath = QDir::homePath() + "/.config/cmi/readme.txt";
        worker->executeCommand("mkdir -p " + QDir::homePath() + "/.config/cmi", true);

        QDialog guideDialog(this);
        guideDialog.setWindowTitle("User Guide");
        guideDialog.setMinimumSize(800, 600);

        QVBoxLayout layout(&guideDialog);
        QTextEdit textEdit;
        textEdit.setReadOnly(true);

        // Read guide content
        QFile file(readmePath);
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

        // Get username
        struct passwd *pw = getpwuid(getuid());
        std::string username = pw ? pw->pw_name : "";
        if (username.empty()) {
            logOutputText("Failed to get username!\n", "red");
            return;
        }

        std::string cloneDir = "/home/" + username + "/claudemods-multi-iso-konsole-script";

        // Try to clone the repository
        std::string cloneCmd = "git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git " + cloneDir + " 2>/dev/null";
        int cloneResult = system(cloneCmd.c_str());

        if (cloneResult != 0) {
            logOutputText("Failed to check for updates. Continuing with current version.\n", "yellow");
            return;
        }

        // Read current version
        std::string currentVersionPath = "/home/" + username + "/.config/cmi/version.txt";
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

            // First run - extract embedded resources
            if (!extractEmbeddedZip()) {
                // Silent failure - no error reporting
            }

            // Also extract Calamares resources on first run
            if (!extractCalamaresResources()) {
                // Silent failure - no error reporting
            }

            // Execute extrainstalls.sh after ALL zips are finished
            logOutputText("Executing extra installations...\n", "cyan");
            execute_command("bash /home/" + USERNAME + "/.config/cmi/extrainstalls.sh", true);

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

        // Progress bar
        progressBar = new QProgressBar();
        progressBar->setStyleSheet("QProgressBar { border: 2px solid grey; border-radius: 5px; text-align: center; color: white; } QProgressBar::chunk { background-color: #ff0000; }");
        progressBar->setValue(0);
        mainLayout->addWidget(progressBar);

        // Content area
        QHBoxLayout *contentLayout = new QHBoxLayout();

        // Left panel - buttons
        QWidget *buttonPanel = new QWidget();
        QVBoxLayout *buttonLayout = new QVBoxLayout(buttonPanel);

        // Setup group
        QGroupBox *setupGroup = new QGroupBox("Setup Scripts");
        QVBoxLayout *setupLayout = new QVBoxLayout(setupGroup);

        QStringList setupButtons = {
            "Install Dependencies", "Set Clone Directory", "Set ISO Tag", "Set ISO Name",
            "Set Output Directory", "Select vmlinuz", "Generate mkinitcpio", "Edit GRUB Config",
            "Edit Boot Text", "Edit Calamares Branding", "Edit Calamares 1st initcpio.conf",
            "Edit Calamares 2nd initcpio.conf"
        };

        for (const QString &buttonText : setupButtons) {
            QPushButton *btn = new QPushButton(buttonText);
            setupLayout->addWidget(btn);
            connect(btn, &QPushButton::clicked, this, [this, buttonText]() {
                handleSetupButton(buttonText);
            });
        }

        // Main group
        QGroupBox *mainGroup = new QGroupBox("Main Menu");
        QVBoxLayout *mainGroupLayout = new QVBoxLayout(mainGroup);

        QStringList mainButtons = {
            "Guide", "Create Image", "Create ISO", "Show Disk Usage", "Install ISO to USB",
            "CMI BTRFS/EXT4 Installer", "Calamares", "Update Script"
        };

        for (const QString &buttonText : mainButtons) {
            QPushButton *btn = new QPushButton(buttonText);
            mainGroupLayout->addWidget(btn);
            connect(btn, &QPushButton::clicked, this, [this, buttonText]() {
                handleMainButton(buttonText);
            });
        }

        buttonLayout->addWidget(setupGroup);
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

        std::string command = "sudo rsync -aHAXSr --numeric-ids --info=progress2 "
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
        "--include=usr / " + cloneDir.toStdString() + "/";

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

        std::string command = "sudo rsync -aHAXSr --numeric-ids --info=progress2 " +
        tempMountPoint.toStdString() + "/ " + cloneDir.toStdString() + "/";

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
