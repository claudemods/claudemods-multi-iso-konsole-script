#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
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
#include <QSplitter>
#include <QScrollBar>
#include <QTextCursor>
#include <QFontDatabase>
#include <QFont>
#include <QRegularExpression>

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
#include <fstream>

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

// Password Dialog
class PasswordDialog : public QDialog {
    Q_OBJECT
public:
    PasswordDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Authentication Required");
        setFixedSize(350, 120);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(15, 15, 15, 15);

        QLabel *label = new QLabel("Enter your sudo password:", this);
        label->setStyleSheet("font-weight: bold;");
        
        passwordEdit = new QLineEdit(this);
        passwordEdit->setEchoMode(QLineEdit::Password);
        passwordEdit->setPlaceholderText("Password");
        passwordEdit->setStyleSheet("padding: 5px;");

        QHBoxLayout *buttonLayout = new QHBoxLayout();
        QPushButton *okButton = new QPushButton("OK", this);
        QPushButton *cancelButton = new QPushButton("Cancel", this);
        okButton->setDefault(true);

        buttonLayout->addStretch();
        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);

        layout->addWidget(label);
        layout->addWidget(passwordEdit);
        layout->addLayout(buttonLayout);

        connect(okButton, &QPushButton::clicked, this, &PasswordDialog::accept);
        connect(cancelButton, &QPushButton::clicked, this, &PasswordDialog::reject);
    }

    QString password() const {
        return passwordEdit->text();
    }

private:
    QLineEdit *passwordEdit;
};

// Console Output Widget
class ConsoleOutput : public QTextEdit {
    Q_OBJECT
public:
    ConsoleOutput(QWidget *parent = nullptr) : QTextEdit(parent) {
        setReadOnly(true);
        setStyleSheet("font-family: 'Consolas', 'Monospace', monospace; font-size: 11px; background-color: #0a0a1a; color: #e0e0e0;");
        setLineWrapMode(QTextEdit::NoWrap);
        
        QFont font("Monospace");
        font.setStyleHint(QFont::TypeWriter);
        setFont(font);
        
        document()->setDocumentMargin(5);
    }

    void append(const QString &text) {
        QString singleLine = text;
        singleLine.replace('\n', ' ').replace('\r', ' ');
        moveCursor(QTextCursor::End);
        textCursor().insertText(singleLine + "\n");
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }

    void appendCommand(const QString &command) {
        QString singleLine = command;
        singleLine.replace('\n', ' ').replace('\r', ' ');
        QTextCharFormat format;
        format.setForeground(QColor("#4fc3f7"));
        format.setFontWeight(QFont::Bold);
        
        moveCursor(QTextCursor::End);
        textCursor().insertText("> " + singleLine + "\n", format);
    }

    void appendError(const QString &error) {
        QString singleLine = error;
        singleLine.replace('\n', ' ').replace('\r', ' ');
        QTextCharFormat format;
        format.setForeground(QColor("#ff5252"));
        format.setFontWeight(QFont::Bold);
        
        moveCursor(QTextCursor::End);
        textCursor().insertText(singleLine + "\n", format);
    }

    void appendSuccess(const QString &message) {
        QString singleLine = message;
        singleLine.replace('\n', ' ').replace('\r', ' ');
        QTextCharFormat format;
        format.setForeground(QColor("#69f0ae"));
        format.setFontWeight(QFont::Bold);
        
        moveCursor(QTextCursor::End);
        textCursor().insertText(singleLine + "\n", format);
    }

    void appendWarning(const QString &warning) {
        QString singleLine = warning;
        singleLine.replace('\n', ' ').replace('\r', ' ');
        QTextCharFormat format;
        format.setForeground(QColor("#ffd740"));
        format.setFontWeight(QFont::Bold);
        
        moveCursor(QTextCursor::End);
        textCursor().insertText(singleLine + "\n", format);
    }

    void clear() {
        QTextEdit::clear();
        append("Console cleared. Ready for new commands.");
    }
};

// Main Application Window
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
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

        if (!askPassword()) {
            exit(0);
        }

        setupUI();
        setupMenuBar();
        setupStatusBar();
        setupConnections();

        setWindowTitle("ClaudeMods Image Builder");
        resize(1200, 800);

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
        QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
        mainLayout->setContentsMargins(5, 5, 5, 5);
        mainLayout->setSpacing(5);

        QSplitter *splitter = new QSplitter(Qt::Horizontal, centralWidget);
        splitter->setHandleWidth(5);
        splitter->setStyleSheet("QSplitter::handle { background: #1a1a2e; }");

        // Left panel
        QWidget *leftPanel = new QWidget(splitter);
        QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
        leftLayout->setContentsMargins(10, 10, 10, 10);
        leftLayout->setSpacing(15);

        bannerLabel = new QLabel(leftPanel);
        bannerLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        bannerLabel->setAlignment(Qt::AlignCenter);
        bannerLabel->setStyleSheet("font-family: monospace; color: #4fc3f7; font-size: 10px;");
        leftLayout->addWidget(bannerLabel);

        updateBanner();

        timeLabel = new QLabel(leftPanel);
        timeLabel->setAlignment(Qt::AlignCenter);
        timeLabel->setStyleSheet("color: #69f0ae; font-weight: bold;");
        leftLayout->addWidget(timeLabel);

        diskInfoLabel = new QLabel(leftPanel);
        diskInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        diskInfoLabel->setAlignment(Qt::AlignCenter);
        diskInfoLabel->setStyleSheet("font-family: monospace; color: #ffd740; font-size: 10px;");
        leftLayout->addWidget(diskInfoLabel);

        updateDiskInfo();

        configGroup = new QGroupBox("Current Configuration", leftPanel);
        configGroup->setStyleSheet("QGroupBox { border: 1px solid #1a1a2e; border-radius: 5px; margin-top: 10px; }"
                                  "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #4fc3f7; }");
        QVBoxLayout *configLayout = new QVBoxLayout(configGroup);
        configLayout->setSpacing(5);

        dependenciesCheck = new QCheckBox("Dependencies Installed", configGroup);
        isoTagLabel = new QLabel("ISO Tag: Not set", configGroup);
        isoNameLabel = new QLabel("ISO Name: Not set", configGroup);
        outputDirLabel = new QLabel("Output Directory: Not set", configGroup);
        vmlinuzLabel = new QLabel("vmlinuz Selected: Not selected", configGroup);
        mkinitcpioCheck = new QCheckBox("mkinitcpio Generated", configGroup);
        grubCheck = new QCheckBox("GRUB Config Edited", configGroup);

        QString labelStyle = "color: #e0e0e0; font-size: 11px;";
        QString checkBoxStyle = "color: #e0e0e0; font-size: 11px; spacing: 3px;";
        
        dependenciesCheck->setStyleSheet(checkBoxStyle);
        isoTagLabel->setStyleSheet(labelStyle);
        isoNameLabel->setStyleSheet(labelStyle);
        outputDirLabel->setStyleSheet(labelStyle);
        vmlinuzLabel->setStyleSheet(labelStyle);
        mkinitcpioCheck->setStyleSheet(checkBoxStyle);
        grubCheck->setStyleSheet(checkBoxStyle);

        configLayout->addWidget(dependenciesCheck);
        configLayout->addWidget(isoTagLabel);
        configLayout->addWidget(isoNameLabel);
        configLayout->addWidget(outputDirLabel);
        configLayout->addWidget(vmlinuzLabel);
        configLayout->addWidget(mkinitcpioCheck);
        configLayout->addWidget(grubCheck);

        leftLayout->addWidget(configGroup);

        QGridLayout *buttonLayout = new QGridLayout();
        buttonLayout->setSpacing(5);
        buttonLayout->setContentsMargins(0, 10, 0, 10);

        QString buttonStyle = "QPushButton { background-color: #1a1a2e; color: #e0e0e0; border: 1px solid #4fc3f7; border-radius: 4px; padding: 6px; font-size: 11px; }"
                            "QPushButton:hover { background-color: #4fc3f7; color: #0a0a1a; }"
                            "QPushButton:pressed { background-color: #0288d1; }";

        QPushButton *guideButton = createStyledButton("Guide", buttonStyle, leftPanel);
        QPushButton *setupButton = createStyledButton("Setup", buttonStyle, leftPanel);
        QPushButton *createImageButton = createStyledButton("Create Image", buttonStyle, leftPanel);
        QPushButton *createIsoButton = createStyledButton("Create ISO", buttonStyle, leftPanel);
        QPushButton *diskUsageButton = createStyledButton("Disk Usage", buttonStyle, leftPanel);
        QPushButton *installUsbButton = createStyledButton("Install to USB", buttonStyle, leftPanel);
        QPushButton *installerButton = createStyledButton("CMI Installer", buttonStyle, leftPanel);
        QPushButton *updateButton = createStyledButton("Update Script", buttonStyle, leftPanel);
        QPushButton *exitButton = createStyledButton("Exit", buttonStyle, leftPanel);
        exitButton->setStyleSheet(buttonStyle + "QPushButton { border-color: #ff5252; } QPushButton:hover { background-color: #ff5252; }");

        buttonLayout->addWidget(guideButton, 0, 0);
        buttonLayout->addWidget(setupButton, 0, 1);
        buttonLayout->addWidget(createImageButton, 1, 0);
        buttonLayout->addWidget(createIsoButton, 1, 1);
        buttonLayout->addWidget(diskUsageButton, 2, 0);
        buttonLayout->addWidget(installUsbButton, 2, 1);
        buttonLayout->addWidget(installerButton, 3, 0);
        buttonLayout->addWidget(updateButton, 3, 1);
        buttonLayout->addWidget(exitButton, 4, 0, 1, 2);

        leftLayout->addLayout(buttonLayout);
        leftLayout->addStretch();

        // Right panel
        QWidget *rightPanel = new QWidget(splitter);
        QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(0, 0, 0, 0);

        console = new ConsoleOutput(rightPanel);
        console->setMinimumWidth(600);

        rightLayout->addWidget(console);

        splitter->addWidget(leftPanel);
        splitter->addWidget(rightPanel);

        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 3);

        mainLayout->addWidget(splitter);
        setCentralWidget(centralWidget);

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

    QPushButton* createStyledButton(const QString& text, const QString& style, QWidget* parent) {
        QPushButton* button = new QPushButton(text, parent);
        button->setStyleSheet(style);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    }

    void setupMenuBar() {
        QMenu *fileMenu = menuBar()->addMenu("File");
        fileMenu->setStyleSheet("QMenu { background-color: #0a0a1a; border: 1px solid #4fc3f7; }"
                              "QMenu::item { background-color: transparent; color: #e0e0e0; padding: 5px 20px; }"
                              "QMenu::item:selected { background-color: #4fc3f7; color: #0a0a1a; }");
        
        QAction *exitAction = fileMenu->addAction("Exit");
        exitAction->setShortcut(QKeySequence::Quit);
        connect(exitAction, &QAction::triggered, this, &QApplication::quit);

        QMenu *toolsMenu = menuBar()->addMenu("Tools");
        toolsMenu->setStyleSheet(fileMenu->styleSheet());
        
        QAction *configAction = toolsMenu->addAction("Show Config", this, &MainWindow::showConfigStatus);
        QAction *clearAction = toolsMenu->addAction("Clear Console", console, &ConsoleOutput::clear);
        
        configAction->setShortcut(QKeySequence("Ctrl+C"));
        clearAction->setShortcut(QKeySequence("Ctrl+L"));
    }

    void setupStatusBar() {
        statusBar()->setStyleSheet("background-color: #1a1a2e; color: #e0e0e0; font-size: 11px;");
        statusBar()->showMessage("Ready");
    }

    void setupConnections() {
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
        console->appendCommand(QString::fromStdString(cmd));

        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start("bash", QStringList() << "-c" << QString::fromStdString(cmd));

        if (cmd.find("sudo") != std::string::npos && !sudoPassword.isEmpty()) {
            process.write((sudoPassword + "\n").toUtf8());
            process.closeWriteChannel();
        }

        connect(&process, &QProcess::readyReadStandardOutput, [&]() {
            QString output = process.readAllStandardOutput();
            console->append(output.trimmed());
        });

        while (!process.waitForFinished(100)) {
            QCoreApplication::processEvents();
        }

        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            QString error = process.readAllStandardError();
            if (!error.isEmpty()) {
                console->appendError(error);
            }
            
            if (!continueOnError) {
                console->appendError("Error executing command!");
                QMessageBox::critical(this, "Error", QString("Failed to execute: %1").arg(QString::fromStdString(cmd)));
                return;
            } else {
                console->appendWarning("Command failed but continuing...");
            }
        } else {
            console->appendSuccess("Command completed successfully");
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
            console->appendError("Failed to save configuration!");
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
        PasswordDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            sudoPassword = dialog.password();

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
        QDialog dialog(this);
        dialog.setWindowTitle("ISO Creation Setup");
        dialog.resize(400, 300);
        dialog.setStyleSheet("background-color: #0a0a1a; color: #e0e0e0;");

        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        layout->setSpacing(5);
        layout->setContentsMargins(0, 10, 0, 10);

        QString buttonStyle = "QPushButton { background-color: #1a1a2e; color: #e0e0e0; border: 1px solid #4fc3f7; border-radius: 4px; padding: 6px; font-size: 11px; }"
                            "QPushButton:hover { background-color: #4fc3f7; color: #0a0a1a; }";

        QPushButton *depsButton = new QPushButton("1. Install Dependencies", &dialog);
        depsButton->setStyleSheet(buttonStyle);
        connect(depsButton, &QPushButton::clicked, [this, &dialog]() {
            installDependencies();
            dialog.close();
        });

        QPushButton *isoTagButton = new QPushButton("2. Set ISO Tag", &dialog);
        isoTagButton->setStyleSheet(buttonStyle);
        connect(isoTagButton, &QPushButton::clicked, [this, &dialog]() {
            bool ok;
            QString text = QInputDialog::getText(&dialog, "Set ISO Tag",
                                             "Enter ISO tag (e.g., 2025):", QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty()) {
                config.isoTag = text.toStdString();
                saveConfig();
            }
        });

        QPushButton *isoNameButton = new QPushButton("3. Set ISO Name", &dialog);
        isoNameButton->setStyleSheet(buttonStyle);
        connect(isoNameButton, &QPushButton::clicked, [this, &dialog]() {
            bool ok;
            QString text = QInputDialog::getText(&dialog, "Set ISO Name",
                                             "Enter ISO name (e.g., claudemods.iso):", QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty()) {
                config.isoName = text.toStdString();
                saveConfig();
            }
        });

        QPushButton *outputDirButton = new QPushButton("4. Set Output Directory", &dialog);
        outputDirButton->setStyleSheet(buttonStyle);
        connect(outputDirButton, &QPushButton::clicked, [this, &dialog]() {
            QString defaultPath = QString::fromStdString("/home/" + USERNAME + "/Downloads");
            QString initialPath = config.outputDir.empty() ? defaultPath : QString::fromStdString(config.outputDir);
            QString dir = QFileDialog::getExistingDirectory(&dialog, "Select Output Directory", initialPath);
            if (!dir.isEmpty()) {
                config.outputDir = dir.toStdString();
                saveConfig();
            }
        });

        QPushButton *vmlinuzButton = new QPushButton("5. Select vmlinuz", &dialog);
        vmlinuzButton->setStyleSheet(buttonStyle);
        connect(vmlinuzButton, &QPushButton::clicked, this, &MainWindow::selectVmlinuz);

        QPushButton *mkinitcpioButton = new QPushButton("6. Generate mkinitcpio", &dialog);
        mkinitcpioButton->setStyleSheet(buttonStyle);
        connect(mkinitcpioButton, &QPushButton::clicked, [this, &dialog]() {
            generateMkinitcpio();
            dialog.close();
        });

        QPushButton *grubButton = new QPushButton("7. Edit GRUB Config", &dialog);
        grubButton->setStyleSheet(buttonStyle);
        connect(grubButton, &QPushButton::clicked, [this, &dialog]() {
            editGrubCfg();
            dialog.close();
        });

        QPushButton *closeButton = new QPushButton("Close", &dialog);
        closeButton->setStyleSheet(buttonStyle);
        connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::close);

        layout->addWidget(depsButton);
        layout->addWidget(isoTagButton);
        layout->addWidget(isoNameButton);
        layout->addWidget(outputDirButton);
        layout->addWidget(vmlinuzButton);
        layout->addWidget(mkinitcpioButton);
        layout->addWidget(grubButton);
        layout->addStretch();
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
        console->appendSuccess("\nDependencies installed successfully!\n");
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
            console->appendError("Could not open /boot directory");
            return;
        }

        if (vmlinuzFiles.empty()) {
            console->appendError("No vmlinuz files found in /boot!");
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

            console->appendSuccess(QString("Selected: %1").arg(item));
            console->appendSuccess(QString("Copied to: %1").arg(QString::fromStdString(destPath)));
            saveConfig();
        }
    }

    void generateMkinitcpio() {
        if (config.vmlinuzPath.empty()) {
            console->appendError("Please select vmlinuz first!");
            return;
        }

        if (BUILD_DIR.empty()) {
            console->appendError("Build directory not set!");
            return;
        }

        console->append("Generating initramfs...");
        execute_command("cd " + BUILD_DIR + " && sudo mkinitcpio -c mkinitcpio.conf -g " + BUILD_DIR + "/boot/initramfs-x86_64.img");

        config.mkinitcpioGenerated = true;
        saveConfig();
        console->appendSuccess("mkinitcpio generated successfully!");
    }

    void editGrubCfg() {
        if (BUILD_DIR.empty()) {
            console->appendError("Build directory not set!");
            return;
        }

        std::string grubCfgPath = BUILD_DIR + "/boot/grub/grub.cfg";
        console->append(QString("Editing GRUB config: %1").arg(QString::fromStdString(grubCfgPath)));

        std::string nanoCommand = "sudo env TERM=xterm-256color nano -Y cyanish " + grubCfgPath;
        execute_command(nanoCommand);

        config.grubEdited = true;
        saveConfig();
        console->appendSuccess("GRUB config edited!");
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

        bool ok;
        double sizeGB = QInputDialog::getDouble(this, "Image Size",
                                            "Enter the image size in GB (e.g., 1.2 for 1.2GB):", 1.0, 0.1, 100.0, 1, &ok);
        if (!ok) return;

        QStringList fsOptions = {"btrfs", "ext4"};
        QString fsType = QInputDialog::getItem(this, "Filesystem Type",
                                           "Choose filesystem type:", fsOptions, 0, false, &ok);
        if (!ok) return;

        if (!createImageFile(std::to_string(sizeGB), outputImgPath) ||
            !formatFilesystem(fsType.toStdString(), outputImgPath) ||
            !mountFilesystem(fsType.toStdString(), outputImgPath, MOUNT_POINT) ||
            !copyFilesWithRsync(SOURCE_DIR, MOUNT_POINT, fsType.toStdString())) {
            return;
            }

            unmountAndCleanup(MOUNT_POINT);

        if (fsType == "ext4") {
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
                console->appendError("Invalid size: must be greater than 0");
                return false;
            }

            size_t sizeBytes = static_cast<size_t>(sizeGB * 1073741824);

            console->append(QString("Creating image file of size %1GB (%2 bytes)...")
            .arg(sizeGB).arg(sizeBytes));

            std::string command = "sudo fallocate -l " + std::to_string(sizeBytes) + " " + filename;
            execute_command(command, true);

            return true;
        } catch (const std::exception& e) {
            console->appendError(QString("Error creating image file: %1")
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
            console->appendSuccess(QString("Compressed BTRFS image created successfully: %1")
            .arg(QString::fromStdString(outputFile)));
        } else {
            console->appendSuccess(QString("Compressed ext4 image created successfully: %1")
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
            console->appendError("Cannot create ISO - setup is incomplete!");
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
        console->appendSuccess(QString("ISO created successfully at %1/%2")
        .arg(QString::fromStdString(expandedOutputDir))
        .arg(QString::fromStdString(config.isoName)));
    }

    void showDiskUsage() {
        execute_command("df -h");
    }

    void installISOToUSB() {
        if (config.outputDir.empty()) {
            console->appendError("Output directory not set!");
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
            console->appendError(QString("Could not open output directory: %1")
            .arg(QString::fromStdString(expandedOutputDir)));
            return;
        }

        if (isoFiles.empty()) {
            console->appendError("No ISO files found in output directory!");
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
            console->appendError("No drive specified!");
            return;
        }

        QMessageBox::StandardButton confirm = QMessageBox::warning(this, "Warning",
                                                               QString("WARNING: This will overwrite all data on %1!").arg(targetDrive),
                                                               QMessageBox::Ok | QMessageBox::Cancel);
        if (confirm != QMessageBox::Ok) {
            console->appendWarning("Operation cancelled.");
            return;
        }

        console->append(QString("\nWriting %1 to %2...")
        .arg(QString::fromStdString(selectedISO))
        .arg(targetDrive));

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

        if (ddProcess.exitStatus() == QProcess::NormalExit && ddProcess.exitCode() == 0) {
            console->appendSuccess("\nISO successfully written to USB drive!");
        } else {
            console->appendError("\nFailed to write ISO to USB drive!");
        }
    }

    void runCMIInstaller() {
        execute_command("cmirsyncinstaller", true);
    }

    void updateScript() {
        console->append("\nUpdating script from GitHub...");
        execute_command("bash -c \"$(curl -fsSL https://raw.githubusercontent.com/claudemods/claudemods-multi-iso-konsole-script/main/advancedimgscript/installer/patch.sh)\"");
        console->appendSuccess("\nScript updated successfully!");
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
    ConsoleOutput *console;

    std::thread timeThread;
    std::atomic<bool> timeThreadRunning{true};
    std::mutex timeMutex;
    std::string currentTimeStr;

    QString sudoPassword;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setStyle("Fusion");

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0, 0, 51));
    palette.setColor(QPalette::WindowText, QColor(0, 255, 255));
    palette.setColor(QPalette::Base, QColor(0, 0, 25));
    palette.setColor(QPalette::AlternateBase, QColor(0, 0, 51));
    palette.setColor(QPalette::ToolTipBase, Qt::black);
    palette.setColor(QPalette::ToolTipText, QColor(0, 255, 255));
    palette.setColor(QPalette::Text, QColor(0, 255, 255));
    palette.setColor(QPalette::Button, QColor(0, 0, 102));
    palette.setColor(QPalette::ButtonText, QColor(0, 255, 255));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(0, 200, 255));
    palette.setColor(QPalette::Highlight, QColor(0, 100, 200));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}

#include "main.moc"
