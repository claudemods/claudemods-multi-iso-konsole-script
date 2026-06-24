#include <QApplication>
#include <QProcess>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    if (argc < 2) {
        QMessageBox::critical(nullptr, "Error", 
            "Usage: launcher <path-to-application>");
        return 1;
    }

    QString targetApp = argv[1];

    // Use pkexec to launch with proper authentication dialog
    QProcess process;
    process.start("pkexec", QStringList() << targetApp);

    if (!process.waitForStarted()) {
        QMessageBox::critical(nullptr, "Error", 
            "Failed to start pkexec. Is policykit installed?");
        return 1;
    }

    // Wait for authentication to complete
    process.waitForFinished(-1);

    if (process.exitCode() != 0) {
        QMessageBox::information(nullptr, "Info", 
            "Authentication cancelled or failed.");
    }

    return 0;
}
