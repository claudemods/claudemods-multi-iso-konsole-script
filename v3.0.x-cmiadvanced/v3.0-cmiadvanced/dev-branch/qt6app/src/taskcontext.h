#pragma once

#include <QByteArray>
#include <QProcessEnvironment>
#include <QString>

#include <functional>

class MainWindow;
class QProcess;
class QStringDecoder;

enum class LogLevel { Command, Output, Info, Success, Warning, Error };

// Handed to a task body running on a worker thread. All commands run through
// bash with the sudo prelude, so `sudo ...` inside them uses the stored
// password. Output is streamed back to the main window's console.
class TaskContext
{
public:
    TaskContext(MainWindow* window, QProcessEnvironment env, QString prelude);

    // Runs a command, streaming its output. Returns the exit code (-1 on crash).
    int run(const QString& cmd, bool echo = true);
    // Runs a command and returns stdout without echoing it.
    QByteArray capture(const QString& cmd, int* exitCode = nullptr);
    // Runs a command feeding `input` on stdin.
    int runWithInput(const QString& cmd, const QByteArray& input, bool echo = true);
    // Like run(), but calls `tick` with the elapsed milliseconds while it runs.
    int runTimed(const QString& cmd, const std::function<void(qint64)>& tick);

    void log(const QString& text, LogLevel level = LogLevel::Info);
    // percent < 0 shows a busy indicator. `format` follows QProgressBar::setFormat.
    void progress(int percent, const QString& format = QString());
    // Queues `fn` to run on the GUI thread (use for touching config/widgets).
    void gui(std::function<void()> fn);

private:
    bool startShell(QProcess& p, const QString& cmd, bool usePty = false);
    void forward(QProcess& p, QStringDecoder& decoder);
    int pump(QProcess& p, const std::function<void(qint64)>& tick = {});

    MainWindow* m_window;
    QProcessEnvironment m_env;
    QString m_prelude;
};
