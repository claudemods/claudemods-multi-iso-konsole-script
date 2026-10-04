#pragma once

#include <QDialog>
#include <QStringList>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class SudoManager;

QWidget* makeBanner(int pointSize, QWidget* parent = nullptr);

// Asked once at startup. The password is kept by SudoManager for every command.
class PasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordDialog(SudoManager* sudo, QWidget* parent = nullptr);

private:
    void tryUnlock();

    SudoManager* m_sudo;
    QLineEdit* m_password;
    QLabel* m_error;
};

// Replaces the nano editing steps of the terminal version.
class EditorDialog : public QDialog
{
    Q_OBJECT

public:
    EditorDialog(const QString& title, const QString& path, const QString& content,
                 bool readOnly, QWidget* parent = nullptr);
    QString text() const;

private:
    QPlainTextEdit* m_editor;
};

// Picks the ISO and target drive for Install ISO To USB.
class UsbDialog : public QDialog
{
    Q_OBJECT

public:
    UsbDialog(const QStringList& isoFiles, const QStringList& drives, QWidget* parent = nullptr);
    QString isoFile() const;
    QString drive() const;

private:
    QComboBox* m_iso;
    QComboBox* m_drive;
};
