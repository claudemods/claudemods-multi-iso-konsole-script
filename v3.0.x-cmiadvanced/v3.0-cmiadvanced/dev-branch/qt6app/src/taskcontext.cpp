#include "taskcontext.h"

#include "config.h"
#include "mainwindow.h"

#include <QDir>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QProcess>
#include <QStringDecoder>

TaskContext::TaskContext(MainWindow* window, QProcessEnvironment env, QString prelude)
    : m_window(window)
    , m_env(std::move(env))
    , m_prelude(std::move(prelude))
{
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

bool TaskContext::startShell(QProcess& p, const QString& cmd, bool usePty)
{
    QProcessEnvironment env = m_env;
    // Same as running the terminal version from the home directory.
    p.setWorkingDirectory(QDir::homePath());

    static const QString script = QStandardPaths::findExecutable(QStringLiteral("script"));
    if (usePty && !script.isEmpty()) {
        // Run inside a pseudo-terminal (util-linux `script`) so tools such as
        // mksquashfs, dd and pacman draw their live, self-updating progress
        // bars; the console redraws "\r" lines in place.
        env.insert(QStringLiteral("SHELL"), QStringLiteral("/bin/bash"));
        env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
        const QString inner = QStringLiteral("stty cols 110 rows 40 2>/dev/null; ") + m_prelude + cmd;
        p.setProcessEnvironment(env);
        p.start(script, {QStringLiteral("-qefc"), QStringLiteral("/bin/bash -c ") + shellQuote(inner),
                         QStringLiteral("/dev/null")});
    } else {
        p.setProcessEnvironment(env);
        p.start(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), m_prelude + cmd});
    }
    if (!p.waitForStarted(10000)) {
        log(QStringLiteral("Failed to start: ") + cmd, LogLevel::Error);
        return false;
    }
    return true;
}

void TaskContext::forward(QProcess& p, QStringDecoder& decoder)
{
    const QByteArray data = p.readAll();
    if (data.isEmpty())
        return;
    const QString text = decoder.decode(data);
    if (text.isEmpty())
        return;
    MainWindow* w = m_window;
    gui([w, text] { w->appendOutput(text); });
}

int TaskContext::pump(QProcess& p, const std::function<void(qint64)>& tick)
{
    QStringDecoder decoder(QStringDecoder::Utf8);
    QElapsedTimer timer;
    timer.start();
    while (p.state() != QProcess::NotRunning) {
        p.waitForReadyRead(100);
        forward(p, decoder);
        if (tick)
            tick(timer.elapsed());
    }
    forward(p, decoder);
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
}

int TaskContext::run(const QString& cmd, bool echo)
{
    if (echo)
        log(cmd, LogLevel::Command);
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    if (!startShell(p, cmd, true))
        return -1;
    p.closeWriteChannel();
    return pump(p);
}

int TaskContext::runTimed(const QString& cmd, const std::function<void(qint64)>& tick)
{
    log(cmd, LogLevel::Command);
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    if (!startShell(p, cmd))
        return -1;
    p.closeWriteChannel();
    return pump(p, tick);
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
