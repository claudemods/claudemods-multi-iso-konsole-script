#include "sudomanager.h"

#include <QCoreApplication>
#include <QFile>
#include <QLocalSocket>
#include <QProcess>
#include <QUuid>

#include <cstdio>
#include <cstring>
#include <unistd.h>

static const char* kSocketEnv = "CMI_ASKPASS_SOCKET";

SudoManager::SudoManager(QObject* parent)
    : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection, this, &SudoManager::onNewConnection);
}

SudoManager::~SudoManager()
{
    m_server.close();
    m_password.fill(QLatin1Char('\0'));
    m_password.clear();
}

bool SudoManager::isRoot()
{
    return geteuid() == 0;
}

bool SudoManager::validate(const QString& password, QString* error)
{
    QProcess p;
    p.start(QStringLiteral("sudo"), {QStringLiteral("-S"), QStringLiteral("-k"),
                                     QStringLiteral("-p"), QString(), QStringLiteral("true")});
    if (!p.waitForStarted(5000)) {
        if (error) *error = QStringLiteral("Could not start sudo. Is it installed?");
        return false;
    }

    QByteArray input = password.toUtf8();
    input.append('\n');
    p.write(input);
    p.closeWriteChannel();
    input.fill('\0');

    if (!p.waitForFinished(30000)) {
        p.kill();
        p.waitForFinished();
        if (error) *error = QStringLiteral("sudo did not respond.");
        return false;
    }

    if (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0) {
        m_password = password;
        return true;
    }

    if (error) *error = QStringLiteral("Incorrect password, or this user is not allowed to use sudo.");
    return false;
}

bool SudoManager::start(QString* error)
{
    m_env = QProcessEnvironment::systemEnvironment();
    m_env.remove(QString::fromLatin1(kSocketEnv));
    if (isRoot())
        return true;

    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    const QString name = QStringLiteral("cmiadvanced-askpass-") +
                         QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalServer::removeServer(name);
    if (!m_server.listen(name)) {
        if (error) *error = m_server.errorString();
        return false;
    }

    m_env.insert(QStringLiteral("SUDO_ASKPASS"), QCoreApplication::applicationFilePath());
    m_env.insert(QString::fromLatin1(kSocketEnv), m_server.fullServerName());
    return true;
}

QString SudoManager::shellPrelude() const
{
    if (isRoot())
        return QString();
    // Exported so scripts launched from the command (bash children) use it too.
    return QStringLiteral("sudo() { command sudo -A \"$@\"; }; export -f sudo; ");
}

void SudoManager::onNewConnection()
{
    while (QLocalSocket* socket = m_server.nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        QByteArray data = m_password.toUtf8();
        data.append('\n');
        socket->write(data);
        socket->flush();
        socket->disconnectFromServer();
        data.fill('\0');
    }
}

bool SudoManager::launchedAsAskpass(int argc, char** argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "--askpass") == 0)
        return true;
    if (qEnvironmentVariableIsEmpty(kSocketEnv))
        return false;

    // sudo runs SUDO_ASKPASS with only the prompt as an argument, so detect
    // that our parent is sudo.
    QFile comm(QStringLiteral("/proc/%1/comm").arg(getppid()));
    if (!comm.open(QIODevice::ReadOnly))
        return false;
    return comm.readAll().trimmed() == "sudo";
}

int SudoManager::runAskpassClient()
{
    const QString name = qEnvironmentVariable(kSocketEnv);
    if (name.isEmpty())
        return 1;

    QLocalSocket socket;
    socket.connectToServer(name, QIODevice::ReadOnly);
    if (!socket.waitForConnected(5000))
        return 1;

    QByteArray data;
    while (true) {
        data += socket.readAll();
        if (socket.state() != QLocalSocket::ConnectedState)
            break;
        if (!socket.waitForReadyRead(5000))
            break;
    }
    data += socket.readAll();

    if (data.isEmpty())
        return 1;
    std::fwrite(data.constData(), 1, static_cast<size_t>(data.size()), stdout);
    std::fflush(stdout);
    data.fill('\0');
    return 0;
}
