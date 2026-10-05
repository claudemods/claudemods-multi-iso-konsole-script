#include "mainwindow.h"

#include "console.h"
#include "dialogs.h"
#include "sudomanager.h"
#include "theme.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

namespace {

const QString kFinalImgName = QStringLiteral("rootfs.img");

QString humanSize(qint64 size)
{
    if (size >= 1073741824)
        return QString::number(size / 1073741824.0, 'f', 2) + QStringLiteral(" GB");
    if (size >= 1048576)
        return QString::number(size / 1048576.0, 'f', 2) + QStringLiteral(" MB");
    if (size >= 1024)
        return QString::number(size / 1024.0, 'f', 2) + QStringLiteral(" KB");
    return QString::number(qMax<qint64>(size, 0)) + QStringLiteral(" B");
}

QString squashfsExcludes()
{
    return QStringLiteral(
        "-e etc/udev/rules.d/70-persistent-cd.rules "
        "-e etc/udev/rules.d/70-persistent-net.rules "
        "-e etc/mtab "
        "-e etc/fstab "
        "-e dev/* "
        "-e proc/* "
        "-e sys/* "
        "-e tmp/* "
        "-e run/* "
        "-e mnt/* "
        "-e media/* "
        "-e lost+found "
        "-e clone_system_temp");
}

// The helpers below use the exact commands from cloner.h.

// Cloner::mountSystemToCloneDir
bool mountSystemToCloneDir(TaskContext& ctx, const QString& cloneDir)
{
    ctx.log(QStringLiteral("Mounting system to: ") + cloneDir);
    ctx.execute(QStringLiteral("sudo mkdir -p ") + cloneDir, true);
    if (ctx.run(QStringLiteral("sudo mount --bind / ") + cloneDir) != 0) {
        ctx.log(QStringLiteral("Failed to bind mount!"), LogLevel::Error);
        return false;
    }
    ctx.log(QStringLiteral("System mounted successfully to: ") + cloneDir, LogLevel::Success);
    return true;
}

void unmountCloneDir(TaskContext& ctx, const QString& cloneDir)
{
    ctx.log(QStringLiteral("Unmounting bind mount..."));
    ctx.execute(QStringLiteral("sudo umount ") + cloneDir, true);
}

// Cloner::createChecksum
void createChecksum(TaskContext& ctx, const QString& filename)
{
    ctx.execute(QStringLiteral("sudo sha512sum ") + filename + QStringLiteral(" > ") + filename +
                QStringLiteral(".sha512"), true);
}

// Cloner::printFinalMessage
void printFinalMessage(TaskContext& ctx, const QString& outputFile)
{
    ctx.log(QStringLiteral("SquashFS image created successfully: ") + outputFile);
    ctx.log(QStringLiteral("Checksum file: ") + outputFile + QStringLiteral(".sha512"));
    ctx.log(QStringLiteral("Size: "));
    ctx.execute(QStringLiteral("sudo du -h ") + outputFile + QStringLiteral(" | cut -f1"), true);
}

QString findTerminal(QStringList* argsBeforeCommand)
{
    const QList<QPair<QString, QStringList>> terminals = {
        {QStringLiteral("konsole"), {QStringLiteral("-e")}},
        {QStringLiteral("kitty"), {}},
        {QStringLiteral("alacritty"), {QStringLiteral("-e")}},
        {QStringLiteral("gnome-terminal"), {QStringLiteral("--")}},
        {QStringLiteral("xfce4-terminal"), {QStringLiteral("-x")}},
        {QStringLiteral("foot"), {}},
        {QStringLiteral("wezterm"), {QStringLiteral("start"), QStringLiteral("--")}},
        {QStringLiteral("xterm"), {QStringLiteral("-e")}},
    };
    for (const auto& [name, args] : terminals) {
        const QString path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty()) {
            *argsBeforeCommand = args;
            return path;
        }
    }
    return {};
}

} // namespace

// =================================================================== Setup

MainWindow::MainWindow(SudoManager* sudo, QWidget* parent)
    : QMainWindow(parent)
    , m_sudo(sudo)
{
    setWindowTitle(Theme::VersionText);

    // Fit small screens (handhelds): smaller banner and a window no larger than the screen.
    const QRect available = screen()->availableGeometry();
    const bool compact = available.height() < 1000;
    resize(qMin(1480, available.width()), qMin(940, available.height()));

    auto* central = new QWidget;
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(compact ? 6 : 12, compact ? 6 : 12, compact ? 6 : 12, compact ? 6 : 12);
    root->setSpacing(compact ? 6 : 10);
    root->addWidget(makeBanner(compact ? 4 : 8));

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);
    m_statusPanel = buildStatusPanel();
    splitter->addWidget(m_statusPanel);
    splitter->addWidget(buildMenus());
    splitter->addWidget(buildConsolePanel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({360, 430, 690});
    root->addWidget(splitter, 1);
    setCentralWidget(central);

    // Same as the original: load ~/.config/cmi/configuration.txt at startup.
    loadConfig(m_config);
    refreshStatus();

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
    m_clockTimer->start(1000);
    updateClock();

    m_diskTimer = new QTimer(this);
    connect(m_diskTimer, &QTimer::timeout, this, &MainWindow::refreshDiskUsage);
    m_diskTimer->start(10000);
    refreshDiskUsage();

    appendLog(QStringLiteral("Welcome to ") + Theme::VersionText, LogLevel::Success);

    const QString configDir = Paths::configDir();
    const QString liveOs = Paths::liveOsDir();
    // Created as the user first so configuration.txt (written by the app) stays
    // writable even though the commands below run as root.
    QDir().mkpath(configDir);
    startTask(QStringLiteral("Initializing"), [configDir, liveOs](TaskContext& ctx) {
        ctx.execute(QStringLiteral("mkdir -p ") + configDir, true);
        // Silent, like the original execute_command_silent().
        ctx.capture(QStringLiteral("sudo mkdir -p ") + liveOs);
    }, {}, false);
}

QWidget* MainWindow::buildStatusPanel()
{
    auto* frame = new QFrame;
    frame->setObjectName(QStringLiteral("panel"));
    frame->setMinimumWidth(320);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    m_clockLabel = new QLabel;
    m_clockLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(Theme::Green));
    layout->addWidget(m_clockLabel);

    m_diskLabel = new QLabel;
    m_diskLabel->setFont(Theme::monoFont(9));
    m_diskLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Green));
    m_diskLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_diskLabel);

    auto* title = new QLabel(QStringLiteral("Current Configuration:"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(title);

    m_statusLabel = new QLabel;
    m_statusLabel->setTextFormat(Qt::RichText);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* scroll = new QScrollArea;
    scroll->setWidget(m_statusLabel);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    layout->addWidget(scroll, 1);
    return frame;
}

QWidget* MainWindow::buildMenus()
{
    m_menus = new QStackedWidget;
    m_menus->setMinimumWidth(380);

    m_mainPage = buildMenuPage(QStringLiteral("cmiadvanced main menu:"), {
        {QStringLiteral("Guide"), [this] { showGuide(); }},
        {QStringLiteral("Setup Scripts"), [this] { openSetupMenu(); }},
        {QStringLiteral("Create System Images"), [this] { openCloneMenu(); }},
        {QStringLiteral("Generate Bootable Isos"), [this] { createISO(); }},
        {QStringLiteral("Check Disk Usage"), [this] { checkDiskUsage(); }},
        {QStringLiteral("Install ISO To USB"), [this] { installISOToUSB(); }},
        {QStringLiteral("CmiAdvancedInstaller (custom ext4/btrfs squashfs/erofs installer)"), [this] { launchInstaller(); }},
        {QStringLiteral("Launch Calamares"), [this] { runCalamares(); }},
        {QStringLiteral("Update Script"), [this] { updateScript(); }},
        {QStringLiteral("Exit"), [this] { close(); }},
    });

    m_setupPage = buildMenuPage(QStringLiteral("Setup Menu:"), {
        {QStringLiteral("Extract Needed Files"), [this] { extractNeededFiles(); }},
        {QStringLiteral("Set Clone Directory"), [this] { setCloneDir(); }},
        {QStringLiteral("Set ISO Tag"), [this] { setIsoTag(); }},
        {QStringLiteral("Set ISO Name"), [this] { setIsoName(); }},
        {QStringLiteral("Set Output Directory"), [this] { setOutputDir(); }},
        {QStringLiteral("Select vmlinuz"), [this] { selectVmlinuz(); }},
        {QStringLiteral("mkinitcpio Config"), [this] { copyMkinitcpioConfig(); }},
        {QStringLiteral("Generate mkinitcpio"), [this] { generateMkinitcpio(); }},
        {QStringLiteral("Edit GRUB Config"), [this] {
            editSystemFile(QStringLiteral("GRUB config"), Paths::buildDir() + QStringLiteral("/boot/grub/grub.cfg"),
                           &ConfigState::grubEdited);
        }},
        {QStringLiteral("Edit Boot Text"), [this] {
            editSystemFile(QStringLiteral("Boot Text"), Paths::buildDir() + QStringLiteral("/boot/grub/kernels.cfg"),
                           &ConfigState::bootTextEdited);
        }},
        {QStringLiteral("Edit Calamares Branding"), [this] {
            editSystemFile(QStringLiteral("Calamares Branding"),
                           QStringLiteral("/usr/share/calamares/branding/claudemods/branding.desc"),
                           &ConfigState::calamaresBrandingEdited);
        }},
        {QStringLiteral("Edit Calamares 1st initcpio.conf"), [this] {
            editSystemFile(QStringLiteral("Calamares 1st initcpio.conf"),
                           QStringLiteral("/etc/calamares/modules/initcpio.conf"), &ConfigState::calamares1Edited);
        }},
        {QStringLiteral("Edit Calamares 2nd initcpio.conf"), [this] {
            editSystemFile(QStringLiteral("Calamares 2nd initcpio.conf"),
                           QStringLiteral("/usr/share/calamares/modules/initcpio.conf"), &ConfigState::calamares2Edited);
        }},
        {QStringLiteral("Back to Main Menu"), [this] { m_menus->setCurrentWidget(m_mainPage); }},
    });

    m_clonePage = buildMenuPage(QStringLiteral("Clone Options - Select Source:"), {
        {QStringLiteral("--- Squashfs Slow Compression Options ---"), {}, true},
        {QStringLiteral("Clone Current System (zstd compression)"), [this] { cloneSquashfs(false); }},
        {QStringLiteral("Clone Current System (xz compression)"), [this] { cloneSquashfs(true); }},
        {QStringLiteral("--- Erofs Fast Options ---"), {}, true},
        {QStringLiteral("Clone Current System (Lz4hc compression)"), [this] { cloneErofs(false); }},
        {QStringLiteral("Clone Current System (Lzma compression)"), [this] { cloneErofs(true); }},
        {QStringLiteral("Back to Main Menu"), [this] { m_menus->setCurrentWidget(m_mainPage); }},
    });

    m_menus->addWidget(m_mainPage);
    m_menus->addWidget(m_setupPage);
    m_menus->addWidget(m_clonePage);
    return m_menus;
}

QWidget* MainWindow::buildMenuPage(const QString& title, const QList<MenuEntry>& entries)
{
    auto* frame = new QFrame;
    frame->setObjectName(QStringLiteral("panel"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(6);

    auto* heading = new QLabel(title);
    heading->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(heading);

    auto* rule = new QFrame;
    rule->setFrameShape(QFrame::HLine);
    rule->setStyleSheet(QStringLiteral("color: #24488f;"));
    layout->addWidget(rule);

    for (const MenuEntry& entry : entries) {
        if (entry.header) {
            auto* label = new QLabel(entry.text);
            label->setObjectName(QStringLiteral("menuHeader"));
            layout->addWidget(label);
            continue;
        }
        auto* button = new QPushButton(QStringLiteral("➤  ") + entry.text);
        button->setObjectName(QStringLiteral("menuButton"));
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(button->sizeHint().height());
        connect(button, &QPushButton::clicked, this, entry.action);
        layout->addWidget(button);
    }
    layout->addStretch();

    // Scroll instead of squashing the buttons when the screen is too short.
    auto* scroll = new QScrollArea;
    scroll->setWidget(frame);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return scroll;
}

QWidget* MainWindow::buildConsolePanel()
{
    auto* frame = new QFrame;
    frame->setObjectName(QStringLiteral("panel"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("Output"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    header->addWidget(title);
    header->addStretch();
    m_expandButton = new QPushButton(QStringLiteral("Expand"));
    m_expandButton->setToolTip(QStringLiteral("Give the output panel the whole window"));
    connect(m_expandButton, &QPushButton::clicked, this, [this] { setLogExpanded(!m_logExpanded); });
    header->addWidget(m_expandButton);
    auto* clear = new QPushButton(QStringLiteral("Clear"));
    header->addWidget(clear);
    layout->addLayout(header);

    m_console = new Console;
    m_console->setStyleSheet(QStringLiteral("QPlainTextEdit { background-color: #050c1d; }"));
    connect(clear, &QPushButton::clicked, m_console, &QPlainTextEdit::clear);
    layout->addWidget(m_console, 1);

    m_taskLabel = new QLabel(QStringLiteral("Ready."));
    m_taskLabel->setObjectName(QStringLiteral("taskLabel"));
    layout->addWidget(m_taskLabel);

    m_progress = new QProgressBar;
    m_progress->setVisible(false);
    layout->addWidget(m_progress);
    return frame;
}

// =================================================================== Status

void MainWindow::updateClock()
{
    m_clockLabel->setText(QStringLiteral("Current UK Time: ") +
                          QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")));
}

void MainWindow::refreshDiskUsage()
{
    if (m_dfProcess)
        return;
    m_dfProcess = new QProcess(this);
    connect(m_dfProcess, &QProcess::finished, this, [this] {
        const QString out = QString::fromLocal8Bit(m_dfProcess->readAllStandardOutput()).trimmed();
        if (!out.isEmpty())
            m_diskLabel->setText(out);
        m_dfProcess->deleteLater();
        m_dfProcess = nullptr;
    });
    connect(m_dfProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            m_dfProcess->deleteLater();
            m_dfProcess = nullptr;
        }
    });
    m_dfProcess->start(QStringLiteral("df"), {QStringLiteral("-h"), QStringLiteral("/")});
}

void MainWindow::refreshStatus()
{
    QString html = QStringLiteral("<table cellspacing='0' cellpadding='3'>");
    auto row = [&html](bool ok, const QString& text) {
        const QString color = ok ? Theme::Green : Theme::Red;
        html += QStringLiteral("<tr><td style='color:%1; font-family:monospace;'>%2</td>"
                               "<td style='color:%1;'>%3</td></tr>")
                    .arg(color, ok ? QStringLiteral("[✓]") : QStringLiteral("[&nbsp;&nbsp;]"), text.toHtmlEscaped());
    };
    auto value = [](const QString& v, const QString& empty) { return v.isEmpty() ? empty : v; };
    const ConfigState& c = m_config;

    row(c.filesExtracted, QStringLiteral("Needed Files"));
    row(!c.isoTag.isEmpty(), QStringLiteral("ISO Tag: ") + value(c.isoTag, QStringLiteral("Not set")));
    row(!c.isoName.isEmpty(), QStringLiteral("ISO Name: ") + value(c.isoName, QStringLiteral("Not set")));
    row(!c.outputDir.isEmpty(), QStringLiteral("Output Directory: ") + value(c.outputDir, QStringLiteral("Not set")));
    row(!c.cloneDir.isEmpty(), QStringLiteral("Clone Directory: ") + value(c.cloneDir, QStringLiteral("Not set")));
    row(!c.vmlinuzPath.isEmpty(), QStringLiteral("vmlinuz: ") + value(c.vmlinuzPath, QStringLiteral("Not selected")));
    row(c.mkinitcpioConfigCopied, QStringLiteral("mkinitcpio Config"));
    row(c.mkinitcpioGenerated, QStringLiteral("mkinitcpio"));
    row(c.grubEdited, QStringLiteral("GRUB Config"));
    row(c.bootTextEdited, QStringLiteral("Boot Text"));
    row(c.calamaresBrandingEdited, QStringLiteral("Calamares Branding"));
    row(c.calamares1Edited, QStringLiteral("Calamares 1st initcpio.conf"));
    row(c.calamares2Edited, QStringLiteral("Calamares 2nd initcpio.conf"));
    html += QStringLiteral("</table>");

    const bool ready = c.allCheckboxesChecked();
    html += QStringLiteral("<p style='color:%1; font-weight:bold;'>%2</p>")
                .arg(ready ? Theme::Green : Theme::Yellow,
                     ready ? QStringLiteral("All setup steps complete - ready to build.")
                           : QStringLiteral("Complete every step in Setup Scripts to build images."));
    m_statusLabel->setText(html);
}

void MainWindow::persist()
{
    if (!saveConfig(m_config))
        appendLog(QStringLiteral("Failed to save configuration to ") + Paths::configFile(), LogLevel::Error);
    refreshStatus();
}

// =================================================================== Output

void MainWindow::appendOutput(const QString& text)
{
    m_console->appendStream(text, QColor(QStringLiteral("#b8d4ff")));
}

void MainWindow::appendLog(const QString& text, LogLevel level)
{
    QString color;
    QString line = text;
    switch (level) {
    case LogLevel::Command: color = Theme::Accent; line = QStringLiteral("$ ") + text; break;
    case LogLevel::Output: color = QStringLiteral("#b8d4ff"); break;
    case LogLevel::Info: color = Theme::Cyan; break;
    case LogLevel::Success: color = Theme::Green; break;
    case LogLevel::Warning: color = Theme::Yellow; break;
    case LogLevel::Error: color = Theme::Red; break;
    }
    m_console->appendLine(line, QColor(color));
}

void MainWindow::setTaskProgress(int percent, const QString& format)
{
    m_progress->setVisible(true);
    if (percent < 0) {
        m_progress->setRange(0, 0);
        return;
    }
    m_progress->setRange(0, 100);
    m_progress->setValue(qBound(0, percent, 100));
    m_progress->setFormat(format.isEmpty() ? QStringLiteral("%p%") : format);
}

void MainWindow::showError(const QString& title, const QString& message)
{
    appendLog(message, LogLevel::Error);
    QMessageBox::warning(this, title, message);
}

// =================================================================== Tasks

void MainWindow::setBusy(bool busy, const QString& title)
{
    m_menus->setEnabled(!busy);
    if (busy) {
        m_taskLabel->setText(QStringLiteral("Running: ") + title + QStringLiteral(" ..."));
        m_progress->setRange(0, 0);
        m_progress->setVisible(true);
    } else {
        m_taskLabel->setText(QStringLiteral("Ready."));
        m_progress->setVisible(false);
    }
}

void MainWindow::setLogExpanded(bool expanded)
{
    m_logExpanded = expanded;
    m_statusPanel->setVisible(!expanded);
    m_menus->setVisible(!expanded);
    m_expandButton->setText(expanded ? QStringLiteral("Restore") : QStringLiteral("Expand"));
}

void MainWindow::startTask(const QString& title, TaskBody body, std::function<void()> onDone, bool expandLog)
{
    if (m_task) {
        appendLog(QStringLiteral("Another operation is still running. Please wait for it to finish."), LogLevel::Warning);
        return;
    }
    setBusy(true, title);
    const bool autoExpanded = expandLog && !m_logExpanded;
    if (autoExpanded)
        setLogExpanded(true);

    const QProcessEnvironment env = m_sudo->environment();
    const QString prelude = m_sudo->shellPrelude();
    m_task = QThread::create([this, body = std::move(body), env, prelude] {
        TaskContext ctx(this, env, prelude);
        body(ctx);
    });
    connect(m_task, &QThread::finished, this, [this, autoExpanded, onDone = std::move(onDone)] {
        m_task->deleteLater();
        m_task = nullptr;
        setBusy(false);
        // The output stays in the (normal size) output panel afterwards.
        if (autoExpanded)
            setLogExpanded(false);
        refreshStatus();
        refreshDiskUsage();
        if (onDone)
            onDone();
    });
    m_task->start();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_task) {
        QMessageBox::warning(this, QStringLiteral("Operation in progress"),
                             QStringLiteral("An operation is still running. Please wait for it to finish before exiting."));
        event->ignore();
        return;
    }
    event->accept();
}

// =================================================================== Main menu

void MainWindow::showGuide()
{
    QDir().mkpath(Paths::configDir());
    const QString readmePath = Paths::configDir() + QStringLiteral("/readme.txt");

    QString path = readmePath;
    QFile file(readmePath);
    if (!file.exists()) {
        file.setFileName(QStringLiteral(":/guide/readme.txt"));
        path = QStringLiteral("built-in guide");
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showError(QStringLiteral("Guide"), QStringLiteral("No guide found. Put readme.txt at: ") + readmePath);
        return;
    }
    EditorDialog dlg(QStringLiteral("Guide"), path, QString::fromUtf8(file.readAll()), true, this);
    dlg.exec();
}

void MainWindow::openSetupMenu()
{
    checkMkinitcpioConfig();
    m_menus->setCurrentWidget(m_setupPage);
}

void MainWindow::openCloneMenu()
{
    if (!m_config.allCheckboxesChecked()) {
        showError(QStringLiteral("Create System Images"),
                  QStringLiteral("Cannot create image - all setup steps must be completed first!\n"
                                 "Please complete all checkboxes in the Setup Scripts menu."));
        return;
    }
    m_menus->setCurrentWidget(m_clonePage);
}

void MainWindow::checkDiskUsage()
{
    startTask(QStringLiteral("Check Disk Usage"), [](TaskContext& ctx) { ctx.execute(QStringLiteral("df -h")); });
}

void MainWindow::createISO()
{
    if (!m_config.allCheckboxesChecked()) {
        showError(QStringLiteral("Generate Bootable Isos"),
                  QStringLiteral("Cannot create ISO - all setup steps must be completed first!"));
        return;
    }
    if (!m_config.isReadyForISO()) {
        showError(QStringLiteral("Generate Bootable Isos"), QStringLiteral("Cannot create ISO - setup is incomplete!"));
        return;
    }

    const QString outputDir = expandPath(m_config.outputDir);
    const QString isoTag = m_config.isoTag;
    const QString isoName = m_config.isoName;
    const QString buildDir = Paths::buildDir();
    const QString user = Paths::username();

    startTask(QStringLiteral("Generate Bootable Iso"), [=](TaskContext& ctx) {
        ctx.log(QStringLiteral("Starting ISO creation process..."));
        ctx.execute(QStringLiteral("mkdir -p ") + outputDir, true);

        const QString xorrisoCmd =
            QStringLiteral("sudo xorriso -as mkisofs "
                           "--modification-date=\"$(date +%Y%m%d%H%M%S00)\" "
                           "--protective-msdos-label "
                           "-volid \"") + isoTag + QStringLiteral("\" "
                           "-appid \"claudemods Linux Live/Rescue CD\" "
                           "-publisher \"claudemods claudemods101@gmail.com >\" "
                           "-preparer \"Prepared by user\" "
                           "-r -graft-points -no-pad "
                           "--sort-weight 0 / "
                           "--sort-weight 1 /boot "
                           "--grub2-mbr ") + buildDir + QStringLiteral("/boot/grub/i386-pc/boot_hybrid.img "
                           "-partition_offset 16 "
                           "-b boot/grub/i386-pc/eltorito.img "
                           "-c boot.catalog "
                           "-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info "
                           "-eltorito-alt-boot "
                           "-append_partition 2 0xef ") + buildDir + QStringLiteral("/boot/efi.img "
                           "-e --interval:appended_partition_2:all:: "
                           "-no-emul-boot "
                           "-iso-level 3 "
                           "-o \"") + outputDir + QStringLiteral("/") + isoName + QStringLiteral("\" ") + buildDir;

        ctx.execute(xorrisoCmd, true);

        const QString isoPath = outputDir + QStringLiteral("/") + isoName;
        ctx.execute(QStringLiteral("sudo chown ") + user + QStringLiteral(":") + user + QStringLiteral(" \"") + isoPath +
                    QStringLiteral("\""), true);

        ctx.log(QStringLiteral("ISO created successfully at ") + isoPath);
        ctx.log(QStringLiteral("Ownership changed to current user: ") + user, LogLevel::Success);
    });
}

void MainWindow::installISOToUSB()
{
    if (m_config.outputDir.isEmpty()) {
        showError(QStringLiteral("Install ISO To USB"), QStringLiteral("Output directory not set!"));
        return;
    }

    const QString outputDir = expandPath(m_config.outputDir);
    QDir dir(outputDir);
    if (!dir.exists()) {
        showError(QStringLiteral("Install ISO To USB"), QStringLiteral("Could not open output directory: ") + outputDir);
        return;
    }

    QStringList isoFiles;
    for (const QString& name : dir.entryList(QDir::Files, QDir::Name)) {
        if (name.contains(QStringLiteral(".iso")))
            isoFiles << name;
    }
    if (isoFiles.isEmpty()) {
        showError(QStringLiteral("Install ISO To USB"), QStringLiteral("No ISO files found in output directory!"));
        return;
    }

    QStringList drives;
    QProcess lsblk;
    lsblk.start(QStringLiteral("lsblk"), {QStringLiteral("-d"), QStringLiteral("-n"), QStringLiteral("-o"),
                                          QStringLiteral("NAME,SIZE,MODEL")});
    if (lsblk.waitForFinished(5000)) {
        const QStringList lines = QString::fromLocal8Bit(lsblk.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            if (!line.contains(QStringLiteral("loop")))
                drives << line.trimmed();
        }
    }

    UsbDialog dlg(isoFiles, drives, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString targetDrive = dlg.drive();
    if (targetDrive.isEmpty()) {
        showError(QStringLiteral("Install ISO To USB"), QStringLiteral("No drive specified!"));
        return;
    }
    const QString selectedISO = outputDir + QStringLiteral("/") + dlg.isoFile();

    const auto answer = QMessageBox::warning(
        this, QStringLiteral("Confirm"),
        QStringLiteral("WARNING: This will overwrite all data on %1!\n\nWrite %2 to %1?").arg(targetDrive, selectedISO),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        appendLog(QStringLiteral("Operation cancelled."), LogLevel::Info);
        return;
    }

    startTask(QStringLiteral("Install ISO To USB"), [=](TaskContext& ctx) {
        ctx.log(QStringLiteral("Writing ") + selectedISO + QStringLiteral(" to ") + targetDrive + QStringLiteral("..."));
        ctx.execute(QStringLiteral("sudo dd if=") + selectedISO + QStringLiteral(" of=") + targetDrive +
                    QStringLiteral(" bs=4M status=progress oflag=sync"), true);
        ctx.log(QStringLiteral("ISO successfully written to USB drive!"), LogLevel::Success);
    });
}

void MainWindow::launchInstaller()
{
    const QString installer = Paths::configDir() + QStringLiteral("/cmiadvancedinstaller");
    if (!QFileInfo::exists(installer)) {
        showError(QStringLiteral("CmiAdvancedInstaller"), QStringLiteral("Installer not found: ") + installer);
        return;
    }

    QStringList args;
    const QString terminal = findTerminal(&args);
    if (terminal.isEmpty()) {
        showError(QStringLiteral("CmiAdvancedInstaller"),
                  QStringLiteral("No terminal emulator found (tried konsole, kitty, alacritty, gnome-terminal, "
                                 "xfce4-terminal, foot, wezterm, xterm)."));
        return;
    }

    // The installer is an interactive terminal program. Authenticate sudo in
    // that terminal with the stored password first so it is not asked again.
    const QString script = m_sudo->shellPrelude() +
                           QStringLiteral("command sudo -A -v 2>/dev/null || command sudo -v; ") +
                           installer +
                           QStringLiteral("; echo; read -n 1 -s -r -p 'Press any key to continue...'");
    args << QStringLiteral("bash") << QStringLiteral("-c") << script;

    QProcess proc;
    proc.setProgram(terminal);
    proc.setArguments(args);
    proc.setProcessEnvironment(m_sudo->environment());
    proc.setWorkingDirectory(QDir::homePath());
    if (proc.startDetached())
        appendLog(QStringLiteral("Launched CmiAdvancedInstaller in ") + QFileInfo(terminal).fileName(), LogLevel::Success);
    else
        showError(QStringLiteral("CmiAdvancedInstaller"), QStringLiteral("Failed to start ") + terminal);
}

void MainWindow::runCalamares()
{
    startTask(QStringLiteral("Calamares"), [](TaskContext& ctx) { ctx.execute(QStringLiteral("sudo calamares"), true); });
}

void MainWindow::updateScript()
{
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Update Script"),
        QStringLiteral("Download and run the latest installer from GitHub?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes)
        return;

    startTask(QStringLiteral("Update Script"), [](TaskContext& ctx) {
        ctx.log(QStringLiteral("Updating script from GitHub..."));
        if (!ctx.execute(QStringLiteral(
                "bash -c \"$(curl -fsSL https://raw.githubusercontent.com/claudemods/claudemods-multi-iso-konsole-script/"
                "refs/heads/main/v3.0.x-cmiadvanced/v3.0-cmiadvanced/release-branch/installer/patch.sh)\"")))
            return;
        ctx.log(QStringLiteral("Script updated successfully!"), LogLevel::Success);
    });
}

// =================================================================== Setup menu

void MainWindow::extractNeededFiles()
{
    const QString configDir = Paths::configDir();
    const QString calamaresTargetDir = configDir + QStringLiteral("/calamares-files");

    startTask(QStringLiteral("Extract Needed Files"), [this, configDir, calamaresTargetDir](TaskContext& ctx) {
        ctx.log(QStringLiteral("Extracting needed files..."));
        ctx.log(QStringLiteral("Extracting embedded zip resources..."));
        QDir().mkpath(configDir);

        // ResourceManager::extractEmbeddedZip: write each zip, unzip it, remove it.
        struct Zip { QString name; QString extractCmd; };
        const QList<Zip> zips = {
            {QStringLiteral("build-image-arch-img.zip"),
             QStringLiteral("cd ") + configDir + QStringLiteral(" && unzip -o build-image-arch-img.zip >/dev/null 2>&1")},
            {QStringLiteral("calamares-files.zip"),
             QStringLiteral("cd ") + configDir + QStringLiteral(" && unzip -o calamares-files.zip >/dev/null 2>&1")},
            {QStringLiteral("claudemods.zip"),
             QStringLiteral("cd ") + configDir + QStringLiteral(" && unzip -o claudemods.zip -d ") + calamaresTargetDir +
                 QStringLiteral(" >/dev/null 2>&1")},
        };

        for (const Zip& zip : zips) {
            const QString target = configDir + QStringLiteral("/") + zip.name;
            QFile::remove(target);
            if (!QFile::copy(QStringLiteral(":/zips/") + zip.name, target)) {
                ctx.log(QStringLiteral("Failed to write ") + target, LogLevel::Error);
                return;
            }
            QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
            if (ctx.run(zip.extractCmd) != 0) {
                ctx.log(QStringLiteral("Failed to extract ") + zip.name, LogLevel::Error);
                return;
            }
            ctx.run(QStringLiteral("rm -f ") + target);
        }

        ctx.log(QStringLiteral("Embedded zip resources extracted successfully!"), LogLevel::Success);
        ctx.gui([this] {
            m_config.filesExtracted = true;
            persist();
        });
        ctx.log(QStringLiteral("All needed files extracted successfully!"), LogLevel::Success);

        ctx.log(QStringLiteral("Running post-extraction setup script..."));
        ctx.execute(QStringLiteral("bash ") + configDir + QStringLiteral("/extrainstalls.sh"), true);
        ctx.log(QStringLiteral("Post-extraction setup completed!"), LogLevel::Success);
    });
}

void MainWindow::setCloneDir()
{
    const QString defaultDir = QStringLiteral("/home/") + Paths::username();
    bool ok = false;
    QString parentDir = QInputDialog::getText(
        this, QStringLiteral("Set Clone Directory"),
        QStringLiteral("Current clone directory: %1\nDefault directory: %2\n\n"
                       "Enter parent directory for clone_system_temp folder (e.g., %2 or $USER):")
            .arg(m_config.cloneDir.isEmpty() ? QStringLiteral("Not set") : m_config.cloneDir, defaultDir),
        QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;

    const qsizetype userPos = parentDir.indexOf(QStringLiteral("$USER"));
    if (userPos >= 0)
        parentDir.replace(userPos, 5, Paths::username());
    if (parentDir.isEmpty())
        parentDir = defaultDir;

    m_config.cloneDir = parentDir + QStringLiteral("/clone_system_temp");
    persist();
    appendLog(QStringLiteral("Clone directory set to: ") + m_config.cloneDir, LogLevel::Success);

    const QString cloneDir = m_config.cloneDir;
    startTask(QStringLiteral("Create clone directory"), [cloneDir](TaskContext& ctx) {
        ctx.execute(QStringLiteral(" sudo mkdir -p ") + cloneDir, true);
    });
}

void MainWindow::setIsoTag()
{
    bool ok = false;
    const QString tag = QInputDialog::getText(this, QStringLiteral("Set ISO Tag"),
                                              QStringLiteral("Enter ISO tag (e.g., default is 2026):"),
                                              QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;
    m_config.isoTag = tag;
    persist();
    appendLog(QStringLiteral("ISO tag set to: ") + m_config.isoTag, LogLevel::Success);
}

void MainWindow::setIsoName()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Set ISO Name"),
                                               QStringLiteral("Enter ISO name (e.g., claudemods.iso):"),
                                               QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;
    m_config.isoName = name;
    persist();
    appendLog(QStringLiteral("ISO name set to: ") + m_config.isoName, LogLevel::Success);
}

void MainWindow::setOutputDir()
{
    const QString defaultDir = QStringLiteral("/home/") + Paths::username() + QStringLiteral("/Downloads");
    bool ok = false;
    QString dir = QInputDialog::getText(
        this, QStringLiteral("Set Output Directory"),
        QStringLiteral("Current output directory: %1\nDefault directory: %2\n\n"
                       "Enter output directory (e.g., %2 or $USER/Downloads):")
            .arg(m_config.outputDir.isEmpty() ? QStringLiteral("Not set") : m_config.outputDir, defaultDir),
        QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;

    const qsizetype userPos = dir.indexOf(QStringLiteral("$USER"));
    if (userPos >= 0)
        dir.replace(userPos, 5, Paths::username());
    if (dir.isEmpty())
        dir = defaultDir;

    m_config.outputDir = dir;
    persist();
    appendLog(QStringLiteral("Output directory set to: ") + dir, LogLevel::Success);

    startTask(QStringLiteral("Create output directory"), [dir](TaskContext& ctx) {
        ctx.execute(QStringLiteral("mkdir -p ") + dir, true);
    });
}

void MainWindow::selectVmlinuz()
{
    QDir boot(QStringLiteral("/boot"));
    if (!boot.exists() || !boot.isReadable()) {
        showError(QStringLiteral("Select vmlinuz"), QStringLiteral("Could not open /boot directory"));
        return;
    }

    QStringList files;
    for (const QString& name : boot.entryList({QStringLiteral("vmlinuz*")}, QDir::Files | QDir::System, QDir::Name))
        files << QStringLiteral("/boot/") + name;
    if (files.isEmpty()) {
        showError(QStringLiteral("Select vmlinuz"), QStringLiteral("No vmlinuz files found in /boot!"));
        return;
    }

    bool ok = false;
    const int current = qMax(0, files.indexOf(m_config.vmlinuzPath));
    const QString choice = QInputDialog::getItem(this, QStringLiteral("Select vmlinuz"),
                                                 QStringLiteral("Available vmlinuz files:"), files, current, false, &ok);
    if (!ok || choice.isEmpty())
        return;

    const QString destPath = Paths::buildDir() + QStringLiteral("/boot/vmlinuz-x86_64");
    startTask(QStringLiteral("Select vmlinuz"), [this, choice, destPath](TaskContext& ctx) {
        if (!ctx.execute(QStringLiteral("sudo cp ") + choice + QStringLiteral(" ") + destPath))
            return;
        ctx.log(QStringLiteral("Selected: ") + choice);
        ctx.log(QStringLiteral("Copied to: ") + destPath);
        ctx.gui([this, choice] {
            m_config.vmlinuzPath = choice;
            persist();
        });
    });
}

void MainWindow::checkMkinitcpioConfig()
{
    const QString destFile = QStringLiteral("/usr/lib/initcpio/udev/11-dm-initramfs.rules");
    appendLog(QStringLiteral("Automatically checking if mkinitcpio config exists..."), LogLevel::Info);
    appendLog(QStringLiteral("Checking: ") + destFile, LogLevel::Info);

    if (QFileInfo::exists(destFile)) {
        appendLog(QStringLiteral("TRUE - File already exists!"), LogLevel::Success);
        m_config.mkinitcpioConfigCopied = true;
    } else {
        appendLog(QStringLiteral("FALSE - File does not exist!"), LogLevel::Error);
        m_config.mkinitcpioConfigCopied = false;
    }
    persist();
}

void MainWindow::copyMkinitcpioConfig()
{
    const QString sourceFile = Paths::buildDir() + QStringLiteral("/11-dm-initramfs.rules");
    const QString destFile = QStringLiteral("/usr/lib/initcpio/udev/11-dm-initramfs.rules");

    startTask(QStringLiteral("mkinitcpio Config"), [this, sourceFile, destFile](TaskContext& ctx) {
        ctx.log(QStringLiteral("Copying mkinitcpio config..."));
        if (!ctx.execute(QStringLiteral("sudo cp ") + sourceFile + QStringLiteral(" ") + destFile))
            return;
        ctx.gui([this] {
            m_config.mkinitcpioConfigCopied = true;
            persist();
        });
        ctx.log(QStringLiteral("mkinitcpio config copied successfully!"), LogLevel::Success);
        ctx.log(QStringLiteral("Copied to: ") + destFile);
    });
}

void MainWindow::generateMkinitcpio()
{
    if (m_config.vmlinuzPath.isEmpty()) {
        showError(QStringLiteral("Generate mkinitcpio"), QStringLiteral("Please select vmlinuz first!"));
        return;
    }

    const QString buildDir = Paths::buildDir();
    startTask(QStringLiteral("Generate mkinitcpio"), [this, buildDir](TaskContext& ctx) {
        ctx.log(QStringLiteral("Generating initramfs..."));
        if (!ctx.execute(QStringLiteral("cd ") + buildDir + QStringLiteral(" && sudo mkinitcpio -c mkinitcpio.conf -g ") +
                         buildDir + QStringLiteral("/boot/initramfs-x86_64.img")))
            return;
        ctx.gui([this] {
            m_config.mkinitcpioGenerated = true;
            persist();
        });
        ctx.log(QStringLiteral("mkinitcpio generated successfully!"), LogLevel::Success);
    });
}

void MainWindow::editSystemFile(const QString& label, const QString& path, bool ConfigState::*flag)
{
    appendLog(QStringLiteral("Editing ") + label + QStringLiteral(": ") + path, LogLevel::Info);

    auto content = std::make_shared<QByteArray>();
    auto readCode = std::make_shared<int>(0);

    auto markEdited = [this, label, flag] {
        m_config.*flag = true;
        persist();
        appendLog(label + QStringLiteral(" edited!"), LogLevel::Success);
    };

    startTask(QStringLiteral("Reading ") + path, [path, content, readCode](TaskContext& ctx) {
        *content = ctx.capture(QStringLiteral("sudo cat ") + shellQuote(path), readCode.get());
    }, [this, label, path, content, readCode, markEdited] {
        if (*readCode != 0)
            appendLog(QStringLiteral("Could not read ") + path + QStringLiteral(" - it will be created when you save."),
                      LogLevel::Warning);

        EditorDialog dlg(QStringLiteral("Edit ") + label, path, QString::fromUtf8(*content), false, this);
        if (dlg.exec() != QDialog::Accepted) {
            markEdited();
            return;
        }

        const QByteArray data = dlg.text().toUtf8();
        startTask(QStringLiteral("Saving ") + path, [path, data](TaskContext& ctx) {
            const int rc = ctx.runWithInput(QStringLiteral("sudo tee ") + shellQuote(path) + QStringLiteral(" > /dev/null"),
                                            data);
            if (rc == 0)
                ctx.log(QStringLiteral("Saved ") + path, LogLevel::Success);
            else
                ctx.log(QStringLiteral("Failed to save ") + path, LogLevel::Error);
        }, markEdited, false);
    }, false);
}

// =================================================================== Clone menu

bool MainWindow::clonePrecheck()
{
    if (m_config.cloneDir.isEmpty()) {
        showError(QStringLiteral("Clone"), QStringLiteral("Clone directory not set! Please set it in Setup Scripts menu."));
        return false;
    }
    if (!m_config.allCheckboxesChecked()) {
        showError(QStringLiteral("Clone"), QStringLiteral("Cannot create image - all setup steps must be completed first!"));
        return false;
    }
    return true;
}

void MainWindow::cloneSquashfs(bool xz)
{
    if (!clonePrecheck())
        return;

    QString level = QStringLiteral("22");
    if (!xz) {
        bool ok = false;
        const int value = QInputDialog::getInt(this, QStringLiteral("zstd compression"),
                                               QStringLiteral("Enter zstd compression level (1-22, default: 22):"),
                                               22, 1, 22, 1, &ok);
        if (!ok)
            return;
        level = QString::number(value);
    }

    const QString cloneDir = expandPath(m_config.cloneDir);
    const QString finalImgPath = Paths::liveOsDir() + QStringLiteral("/") + kFinalImgName;

    startTask(xz ? QStringLiteral("Clone Current System (xz)") : QStringLiteral("Clone Current System (zstd)"),
              [=](TaskContext& ctx) {
        // showCloneOptionsMenu
        ctx.execute(QStringLiteral("sudo mkdir -p ") + cloneDir, true);

        if (!mountSystemToCloneDir(ctx, cloneDir)) {
            ctx.log(QStringLiteral("Failed to mount system!"), LogLevel::Error);
            return;
        }

        // Cloner::createSquashFS / createSquashFS_xz - waits until mksquashfs has finished.
        QString cmd = QStringLiteral("sudo mksquashfs ") + cloneDir + QStringLiteral(" ") + finalImgPath;
        if (xz)
            cmd += QStringLiteral(" -noappend -comp xz -b 256K -Xbcj x86 ");
        else
            cmd += QStringLiteral(" -noappend -comp zstd -Xcompression-level ") + level + QStringLiteral(" -b 256K ");
        cmd += squashfsExcludes();
        ctx.execute(cmd, true);

        unmountCloneDir(ctx, cloneDir);
        createChecksum(ctx, finalImgPath);
        printFinalMessage(ctx, finalImgPath);
        if (xz)
            ctx.log(QStringLiteral("Current system cloned successfully using xz compression!"), LogLevel::Success);
        else
            ctx.log(QStringLiteral("Current system cloned successfully using zstd compression (level ") + level +
                    QStringLiteral(")!"), LogLevel::Success);
    });
}

void MainWindow::cloneErofs(bool lzma)
{
    if (!clonePrecheck())
        return;

    bool ok = false;
    const int value = lzma
        ? QInputDialog::getInt(this, QStringLiteral("lzma compression"),
                               QStringLiteral("Enter lzma compression level (1-109, default: 109):"), 109, 1, 109, 1, &ok)
        : QInputDialog::getInt(this, QStringLiteral("lz4hc compression"),
                               QStringLiteral("Enter lz4hc compression level (1-12, default: 12):"), 12, 1, 12, 1, &ok);
    if (!ok)
        return;
    const QString level = QString::number(value);

    const QString cloneDir = expandPath(m_config.cloneDir);
    const QString outputDir = Paths::liveOsDir();
    const QString outputFile = outputDir + QStringLiteral("/") + kFinalImgName;
    const QString logFile = outputDir + QStringLiteral("/log.txt");

    startTask(lzma ? QStringLiteral("Clone Current System (erofs lzma)") : QStringLiteral("Clone Current System (erofs lz4hc)"),
              [=](TaskContext& ctx) {
        // showCloneOptionsMenu
        ctx.execute(QStringLiteral("sudo mkdir -p ") + cloneDir, true);

        if (!mountSystemToCloneDir(ctx, cloneDir)) {
            ctx.log(QStringLiteral("Failed to mount system!"), LogLevel::Error);
            return;
        }

        // Cloner::createErofsImage - same commands as the original.
        ctx.log(QStringLiteral("Creating EROFS image..."));
        ctx.execute(QStringLiteral("mkdir -p ") + outputDir, true);
        ctx.execute(QStringLiteral("rm -f ") + logFile, true);

        const QString compressionArgs = lzma
            ? QStringLiteral("-zlzma,level=") + level + QStringLiteral(",dictsize=8388608")
            : QStringLiteral("-zlz4hc,level=") + level + QStringLiteral(",");

        const QString c = cloneDir.mid(1);
        QString exclusions;
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral(" ");
        exclusions += QStringLiteral("--exclude-path=") + outputDir.mid(1) + QStringLiteral(" ");
        exclusions += QStringLiteral("--exclude-path=") + outputFile.mid(1) + QStringLiteral("/rootfs.img ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/etc/udev/rules.d/70-persistent-cd.rules ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/etc/udev/rules.d/70-persistent-net.rules ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/etc/mtab ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/etc/fstab ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/dev/* ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/proc/* ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/sys/* ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/tmp/* ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/run/* ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/mnt/* ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/lost+found ");
        exclusions += QStringLiteral("--exclude-path=") + c + QStringLiteral("/clone_system_temp");

        const QString cmd = QStringLiteral("sudo mkfs.erofs -d9 ") + compressionArgs + QStringLiteral(" -C1048576 ") +
                            exclusions + QStringLiteral(" ") + outputFile + QStringLiteral(" ") + cloneDir +
                            QStringLiteral(" > ") + logFile + QStringLiteral(" 2>&1 &");
        ctx.system(cmd);

        // Same progress logic as the original: 1% per 10 s up to 60%, hold at 60%
        // until "Filesystem UUID" appears in log.txt, then wait 10 s. In addition,
        // never continue while mkfs.erofs is still running.
        auto erofsRunning = [&ctx] {
            int code = 1;
            ctx.capture(QStringLiteral("pgrep -x mkfs.erofs"), &code);
            return code == 0;
        };
        int secondsElapsed = 0;
        bool uuidDetected = false;
        int uuidWaitCounter = 0;
        while (true) {
            QThread::sleep(1);
            ++secondsElapsed;

            const int percent = qBound(1, secondsElapsed / 10, 60);
            const QString timer = QStringLiteral("%1:%2").arg(secondsElapsed / 60, 2, 10, QLatin1Char('0'))
                                                         .arg(secondsElapsed % 60, 2, 10, QLatin1Char('0'));
            ctx.progress(percent, QStringLiteral("%p%   [") + timer + QStringLiteral("]   [") +
                                      humanSize(QFileInfo(outputFile).size()) + QStringLiteral("]"));

            const bool running = erofsRunning();
            if (!uuidDetected) {
                QFile log(logFile);
                if (log.open(QIODevice::ReadOnly) && log.readAll().contains("Filesystem UUID"))
                    uuidDetected = true;
            }
            if (uuidDetected && !running && ++uuidWaitCounter >= 10)
                break;
            if (!uuidDetected && !running && secondsElapsed > 5) {
                ctx.log(QStringLiteral("mkfs.erofs is not running and never finished - see ") + logFile, LogLevel::Error);
                break;
            }
        }
        QThread::sleep(1);
        ctx.progress(100, QStringLiteral("%p%   [") + humanSize(QFileInfo(outputFile).size()) + QStringLiteral("]"));

        ctx.log(QStringLiteral("EROFS image created successfully: ") + outputFile, LogLevel::Success);
        ctx.log(QStringLiteral("Image size: "), LogLevel::Success);
        ctx.execute(QStringLiteral("sudo du -h ") + outputFile + QStringLiteral(" | cut -f1"), true);

        unmountCloneDir(ctx, cloneDir);
        createChecksum(ctx, outputFile);
        ctx.log(QStringLiteral("Current system cloned successfully using erofs ") +
                (lzma ? QStringLiteral("lzma") : QStringLiteral("lz4hc")) +
                QStringLiteral(" compression (level ") + level + QStringLiteral(")!"), LogLevel::Success);
    });
}
