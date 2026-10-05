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

    // Same as the original execute_command(): runs the command in a real
    // terminal (pty) so progress bars/spinners animate, and waits until it has
    // completely finished. On failure logs "Command failed but continuing: ..."
    // (continueOnError) or "Error executing: ..." and returns false.
    bool execute(const QString& cmd, bool continueOnError = false);

    // Runs a command in a pty and waits for it to finish. Returns the exit code.
    int run(const QString& cmd, bool echo = true);
    // Same as the original system(): runs the command without a terminal and
    // returns its exit code (used for commands that background themselves).
    int system(const QString& cmd);
    // Runs a command and returns stdout without echoing it.
    QByteArray capture(const QString& cmd, int* exitCode = nullptr);
    // Runs a command feeding `input` on stdin.
    int runWithInput(const QString& cmd, const QByteArray& input, bool echo = true);

    void log(const QString& text, LogLevel level = LogLevel::Info);
    // percent < 0 shows a busy indicator. `format` follows QProgressBar::setFormat.
    void progress(int percent, const QString& format = QString());
    // Queues `fn` to run on the GUI thread (use for touching config/widgets).
    void gui(std::function<void()> fn);

private:
    QString shellScript(const QString& cmd) const;
    bool startShell(QProcess& p, const QString& cmd);
    void forward(QProcess& p, QStringDecoder& decoder);
    void emitText(const QString& text);
    int pump(QProcess& p);

    MainWindow* m_window;
    QProcessEnvironment m_env;
    QString m_prelude;
};
