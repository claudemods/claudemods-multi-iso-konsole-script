#include "dialogs.h"
#include "mainwindow.h"
#include "sudomanager.h"
#include "theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>

int main(int argc, char* argv[])
{
    // When sudo runs us as SUDO_ASKPASS, hand back the stored password and exit.
    if (SudoManager::launchedAsAskpass(argc, argv)) {
        QCoreApplication app(argc, argv);
        return SudoManager::runAskpassClient();
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("cmiadvanced"));
    QApplication::setApplicationDisplayName(Theme::VersionText);
    Theme::apply(app);

    SudoManager sudo;
    if (!SudoManager::isRoot()) {
        PasswordDialog dlg(&sudo);
        if (dlg.exec() != QDialog::Accepted)
            return 0;
    }

    QString error;
    if (!sudo.start(&error)) {
        QMessageBox::critical(nullptr, Theme::VersionText,
                              QStringLiteral("Could not start the sudo helper: ") + error);
        return 1;
    }

    MainWindow window(&sudo);
    window.show();
    return app.exec();
}
