#pragma once

#include "config.h"
#include "taskcontext.h"

#include <QColor>
#include <QList>
#include <QMainWindow>

#include <functional>

class Console;
class QLabel;
class QProcess;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QThread;
class QTimer;
class SudoManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(SudoManager* sudo, QWidget* parent = nullptr);

    // GUI-thread sinks used by TaskContext.
    // An invalid color uses the default output color.
    void appendOutput(const QString& text, const QColor& color = QColor());
    void appendLog(const QString& text, LogLevel level);
    void setTaskProgress(int percent, const QString& format);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    struct MenuEntry {
        QString text;
        std::function<void()> action;
        bool header = false;
    };
    using TaskBody = std::function<void(TaskContext&)>;

    // UI construction
    QWidget* buildStatusPanel();
    QWidget* buildMenus();
    QWidget* buildMenuPage(const QString& title, const QList<MenuEntry>& entries);
    QWidget* buildConsolePanel();

    void refreshStatus();
    void refreshDiskUsage();
    void updateClock();
    void setBusy(bool busy, const QString& title = QString());
    // expandLog: give the output panel the whole window while the task runs.
    void startTask(const QString& title, TaskBody body, std::function<void()> onDone = {}, bool expandLog = true);
    void setLogExpanded(bool expanded);
    void persist();
    void showError(const QString& title, const QString& message);

    // Main menu
    void showGuide();
    void openSetupMenu();
    void openCloneMenu();
    void createISO();
    void checkDiskUsage();
    void installISOToUSB();
    void launchInstaller();
    void runCalamares();
    void updateScript();

    // Setup menu
    void extractNeededFiles();
    void setCloneDir();
    void setIsoTag();
    void setIsoName();
    void setOutputDir();
    void selectVmlinuz();
    void checkMkinitcpioConfig();
    void copyMkinitcpioConfig();
    void generateMkinitcpio();
    void editSystemFile(const QString& label, const QString& path, bool ConfigState::*flag);

    // Clone menu
    bool clonePrecheck();
    void cloneSquashfs(bool xz);
    void cloneErofs(bool lzma);

    SudoManager* m_sudo;
    ConfigState m_config;  // loaded from / saved to ~/.config/cmi/configuration.txt

    QWidget* m_statusPanel = nullptr;
    QPushButton* m_expandButton = nullptr;
    bool m_logExpanded = false;
    QStackedWidget* m_menus = nullptr;
    QWidget* m_mainPage = nullptr;
    QWidget* m_setupPage = nullptr;
    QWidget* m_clonePage = nullptr;

    QLabel* m_clockLabel = nullptr;
    QLabel* m_diskLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_taskLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    Console* m_console = nullptr;

    QTimer* m_clockTimer = nullptr;
    QTimer* m_diskTimer = nullptr;
    QProcess* m_dfProcess = nullptr;
    QThread* m_task = nullptr;
};
