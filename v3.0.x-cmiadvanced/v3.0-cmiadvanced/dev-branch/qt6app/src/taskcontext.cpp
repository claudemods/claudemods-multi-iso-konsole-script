#include "taskcontext.h"

#include "config.h"
#include "mainwindow.h"
#include "sudomanager.h"

#include <QDir>
#include <QMetaObject>
#include <QProcess>
#include <QStringDecoder>

#include <vector>

#include <cerrno>
#include <poll.h>
#include <pty.h>
#include <sys/wait.h>
#include <unistd.h>

TaskContext::TaskContext(MainWindow* window, QProcessEnvironment env, QString prelude)
    : m_window(window)
    , m_env(std::move(env))
    , m_prelude(std::move(prelude))
{
}

// Every command runs as root through sudo (using the stored password).
// USER/LOGNAME/HOME stay set to the real user so $USER and ~ in commands and
// scripts still point at the user's home, like the terminal version.
QString TaskContext::shellScript(const QString& cmd) const
{
    if (SudoManager::isRoot())
        return cmd;
    const QString user = Paths::username();
    return QStringLiteral("command sudo -A env USER=") + user + QStringLiteral(" LOGNAME=") + user +
           QStringLiteral(" HOME=") + shellQuote(QDir::homePath()) +
           QStringLiteral(" /bin/bash -c ") + shellQuote(m_prelude + cmd);
}

void TaskContext::gui(std::function<void()> fn)
{
    QMetaObject::invokeMethod(m_window, std::move(fn), Qt::QueuedConnection);
}

void TaskContext::log(const QString& text, LogLevel level)
{
    MainWindow* w = m_window;
    gui([w, text, level] { w->appendLog(text, level); });
}

void TaskContext::progress(int percent, const QString& format)
{
    MainWindow* w = m_window;
    gui([w, percent, format] { w->setTaskProgress(percent, format); });
}

void TaskContext::emitText(const QString& text)
{
    if (text.isEmpty())
        return;
    MainWindow* w = m_window;
    const QColor color = m_outputColor;
    gui([w, text, color] { w->appendOutput(text, color); });
}

bool TaskContext::execute(const QString& cmd, bool continueOnError)
{
    const int status = run(cmd);
    if (status != 0 && !continueOnError) {
        log(QStringLiteral("Error executing: ") + cmd, LogLevel::Error);
        return false;
    }
    if (status != 0)
        log(QStringLiteral("Command failed but continuing: ") + cmd, LogLevel::Warning);
    return true;
}

int TaskContext::run(const QString& cmd, bool echo)
{
    if (echo)
        log(cmd, LogLevel::Command);

    // Everything is prepared before fork(): the child only calls chdir/execve.
    QProcessEnvironment env = m_env;
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    std::vector<QByteArray> envBytes;
    for (const QString& kv : env.toStringList())
        envBytes.push_back(kv.toLocal8Bit());
    std::vector<char*> envp;
    for (QByteArray& kv : envBytes)
        envp.push_back(kv.data());
    envp.push_back(nullptr);

    QByteArray bash("/bin/bash");
    QByteArray dashC("-c");
    QByteArray script = (m_prelude + shellScript(cmd)).toLocal8Bit();
    char* argv[] = {bash.data(), dashC.data(), script.data(), nullptr};
    const QByteArray home = QDir::homePath().toLocal8Bit();

    // A real pseudo-terminal: tools like mksquashfs, dd and pacman see a
    // terminal and draw their animated progress bars/spinners.
    struct winsize ws = {};
    ws.ws_col = 110;
    ws.ws_row = 40;
    int master = -1;
    const pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0) {
        log(QStringLiteral("Failed to start: ") + cmd, LogLevel::Error);
        return -1;
    }
    if (pid == 0) {
        if (chdir(home.constData()) != 0) { /* stay in the current directory */ }
        execve("/bin/bash", argv, envp.data());
        _exit(127);
    }

    QStringDecoder decoder(QStringDecoder::Utf8);
    char buf[8192];
    int status = 0;
    bool exited = false;

    // Read output until the command itself has exited...
    while (!exited) {
        pollfd pfd = {master, POLLIN, 0};
        const int pr = poll(&pfd, 1, 100);
        if (pr > 0) {
            const ssize_t n = read(master, buf, sizeof buf);
            if (n > 0)
                emitText(decoder.decode(QByteArrayView(buf, n)));
            else if (n < 0 && errno == EINTR)
                continue;
            else {
                // Terminal closed: the command is ending; wait for it to finish.
                waitpid(pid, &status, 0);
                exited = true;
                break;
            }
        }
        if (waitpid(pid, &status, WNOHANG) == pid)
            exited = true;
    }
    // ...then collect whatever output is still buffered.
    while (true) {
        pollfd pfd = {master, POLLIN, 0};
        if (poll(&pfd, 1, 50) <= 0)
            break;
        const ssize_t n = read(master, buf, sizeof buf);
        if (n <= 0)
            break;
        emitText(decoder.decode(QByteArrayView(buf, n)));
    }
    close(master);

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

bool TaskContext::startShell(QProcess& p, const QString& cmd)
{
    p.setProcessEnvironment(m_env);
    p.setWorkingDirectory(QDir::homePath());
    p.start(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), m_prelude + shellScript(cmd)});
    if (!p.waitForStarted(10000)) {
        log(QStringLiteral("Failed to start: ") + cmd, LogLevel::Error);
        return false;
    }
    return true;
}

void TaskContext::forward(QProcess& p, QStringDecoder& decoder)
{
    const QByteArray data = p.readAll();
    if (!data.isEmpty())
        emitText(decoder.decode(data));
}

int TaskContext::pump(QProcess& p)
{
    QStringDecoder decoder(QStringDecoder::Utf8);
    while (p.state() != QProcess::NotRunning) {
        p.waitForReadyRead(100);
        forward(p, decoder);
    }
    forward(p, decoder);
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
}

int TaskContext::system(const QString& cmd)
{
    log(cmd, LogLevel::Command);
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    if (!startShell(p, cmd))
        return -1;
    p.closeWriteChannel();
    return pump(p);
}

int TaskContext::runWithInput(const QString& cmd, const QByteArray& input, bool echo)
{
    if (echo)
        log(cmd, LogLevel::Command);
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    if (!startShell(p, cmd))
        return -1;
    p.write(input);
    p.closeWriteChannel();
    return pump(p);
}

QByteArray TaskContext::capture(const QString& cmd, int* exitCode)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::SeparateChannels);
    if (!startShell(p, cmd)) {
        if (exitCode) *exitCode = -1;
        return {};
    }
    p.closeWriteChannel();

    QByteArray out;
    while (p.state() != QProcess::NotRunning) {
        p.waitForReadyRead(100);
        out += p.readAllStandardOutput();
        p.readAllStandardError();
    }
    out += p.readAllStandardOutput();

    if (exitCode)
        *exitCode = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    return out;
}
