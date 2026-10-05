#include "dialogs.h"

#include "config.h"
#include "sudomanager.h"
#include "theme.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

QWidget* makeBanner(int pointSize, QWidget* parent)
{
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("bannerPanel"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(4);

    auto* art = new QLabel(Theme::bannerArt());
    art->setObjectName(QStringLiteral("claudemodsText"));
    art->setTextFormat(Qt::PlainText);
    art->setFont(Theme::monoFont(pointSize));
    art->setAlignment(Qt::AlignCenter);

    auto* version = new QLabel(Theme::VersionText);
    version->setObjectName(QStringLiteral("claudemodsText"));
    QFont vf = version->font();
    vf.setBold(true);
    // Only the ASCII art scales down; the version line stays readable.
    vf.setPointSize(qMax(12, pointSize + 4));
    version->setFont(vf);
    version->setAlignment(Qt::AlignCenter);

    auto* tagline = new QLabel(Theme::Tagline);
    tagline->setObjectName(QStringLiteral("claudemodsText"));
    tagline->setAlignment(Qt::AlignCenter);
    tagline->setWordWrap(true);

    layout->addWidget(art);
    layout->addWidget(version);
    layout->addWidget(tagline);
    return frame;
}

// ---------------------------------------------------------------- Password

PasswordDialog::PasswordDialog(SudoManager* sudo, QWidget* parent)
    : QDialog(parent)
    , m_sudo(sudo)
{
    setWindowTitle(Theme::VersionText + QStringLiteral(" - Authentication"));
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);
    layout->addWidget(makeBanner(6, this));

    auto* info = new QLabel(QStringLiteral(
        "cmiadvanced runs system commands with sudo (mounting, mksquashfs, mkinitcpio, xorriso, dd...).\n"
        "Enter your sudo password once - it is kept in memory only while the app is running."));
    info->setWordWrap(true);
    layout->addWidget(info);

    auto* form = new QFormLayout;
    auto* user = new QLabel(Paths::username());
    user->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(Theme::Green));
    form->addRow(QStringLiteral("User:"), user);

    m_password = new QLineEdit;
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(QStringLiteral("sudo password"));
    form->addRow(QStringLiteral("Password:"), m_password);
    layout->addLayout(form);

    m_error = new QLabel;
    m_error->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Red));
    m_error->setWordWrap(true);
    m_error->hide();
    layout->addWidget(m_error);

    auto* buttons = new QDialogButtonBox;
    QPushButton* unlock = buttons->addButton(QStringLiteral("Unlock"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QStringLiteral("Quit"), QDialogButtonBox::RejectRole);
    unlock->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &PasswordDialog::tryUnlock);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    // Enter in the password field triggers the default "Unlock" button.

    m_password->setFocus();
}

void PasswordDialog::tryUnlock()
{
    if (m_password->text().isEmpty()) {
        m_error->setText(QStringLiteral("Please enter your password."));
        m_error->show();
        return;
    }

    setEnabled(false);
    m_error->setText(QStringLiteral("Checking password..."));
    m_error->show();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();

    QString error;
    const bool ok = m_sudo->validate(m_password->text(), &error);

    QApplication::restoreOverrideCursor();
    setEnabled(true);

    if (ok) {
        m_password->clear();
        accept();
        return;
    }
    m_error->setText(error);
    m_password->clear();
    m_password->setFocus();
}

// ---------------------------------------------------------------- Editor

EditorDialog::EditorDialog(const QString& title, const QString& path, const QString& content,
                           bool readOnly, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    resize(1050, 720);

    auto* layout = new QVBoxLayout(this);
    auto* header = new QLabel((readOnly ? QStringLiteral("Viewing: ") : QStringLiteral("Editing: ")) + path);
    header->setObjectName(QStringLiteral("sectionTitle"));
    header->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(header);

    m_editor = new QPlainTextEdit;
    m_editor->setFont(Theme::monoFont(10));
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setPlainText(content);
    m_editor->setReadOnly(readOnly);
    layout->addWidget(m_editor, 1);

    auto* buttons = new QDialogButtonBox;
    if (readOnly) {
        buttons->addButton(QStringLiteral("Close"), QDialogButtonBox::RejectRole);
    } else {
        QPushButton* save = buttons->addButton(QStringLiteral("Save"), QDialogButtonBox::AcceptRole);
        save->setToolTip(QStringLiteral("Save (Ctrl+S)"));
        buttons->addButton(QStringLiteral("Close without saving"), QDialogButtonBox::RejectRole);
        auto* shortcut = new QShortcut(QKeySequence::Save, this);
        connect(shortcut, &QShortcut::activated, this, &QDialog::accept);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_editor->setFocus();
}

QString EditorDialog::text() const
{
    return m_editor->toPlainText();
}

// ---------------------------------------------------------------- USB

UsbDialog::UsbDialog(const QStringList& isoFiles, const QStringList& drives, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Install ISO To USB"));
    resize(640, 260);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_iso = new QComboBox;
    m_iso->addItems(isoFiles);
    form->addRow(QStringLiteral("ISO file:"), m_iso);

    m_drive = new QComboBox;
    m_drive->setEditable(true);
    for (const QString& line : drives) {
        const QString name = line.section(QLatin1Char(' '), 0, 0, QString::SectionSkipEmpty);
        m_drive->addItem(QStringLiteral("/dev/") + line.simplified(), QStringLiteral("/dev/") + name);
    }
    m_drive->setCurrentIndex(-1);
    m_drive->lineEdit()->setPlaceholderText(QStringLiteral("e.g. /dev/sda"));
    form->addRow(QStringLiteral("Target drive:"), m_drive);
    layout->addLayout(form);

    auto* warning = new QLabel(QStringLiteral("WARNING: This will overwrite all data on the selected drive!"));
    warning->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(Theme::Red));
    layout->addWidget(warning);
    layout->addStretch();

    auto* buttons = new QDialogButtonBox;
    QPushButton* write = buttons->addButton(QStringLiteral("Write to USB"), QDialogButtonBox::AcceptRole);
    write->setObjectName(QStringLiteral("dangerButton"));
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString UsbDialog::isoFile() const
{
    return m_iso->currentText();
}

QString UsbDialog::drive() const
{
    const int index = m_drive->currentIndex();
    if (index >= 0 && m_drive->itemText(index) == m_drive->currentText())
        return m_drive->itemData(index).toString();
    return m_drive->currentText().trimmed();
}
