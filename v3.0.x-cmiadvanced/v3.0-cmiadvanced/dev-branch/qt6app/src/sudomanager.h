#pragma once

#include <QLocalServer>
#include <QObject>
#include <QProcessEnvironment>
#include <QString>

// Holds the user's sudo password in memory for the lifetime of the app and
// hands it to sudo on demand.
//
// Every command is run through bash with a `sudo` shell function that turns
// `sudo ...` into `sudo -A ...`. SUDO_ASKPASS points back at this executable;
// when sudo launches it, main() switches into askpass mode, connects to the
// private local socket served here, and prints the password for sudo. The
// password is never written to disk or passed on a command line.
class SudoManager : public QObject
{
    Q_OBJECT

public:
    explicit SudoManager(QObject* parent = nullptr);
    ~SudoManager() override;

    static bool isRoot();

    // Checks the password with `sudo -S -k true`; keeps it on success.
    bool validate(const QString& password, QString* error);
    // Starts the askpass socket and builds the environment for child processes.
    bool start(QString* error);

    QProcessEnvironment environment() const { return m_env; }
    QString shellPrelude() const;

    // Entry point used when this binary is run by sudo as the askpass helper.
    static int runAskpassClient();
    static bool launchedAsAskpass(int argc, char** argv);

private:
    void onNewConnection();

    QString m_password;
    QLocalServer m_server;
    QProcessEnvironment m_env;
};
