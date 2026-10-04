#include "theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace Theme {

QString bannerArt()
{
    return QString::fromUtf8(
        "███████████████████████████████████████████████████████████████████████████████████╗\n"
        "░█████╗░██║░░░░░░█████╗░██║░░░██║██████╗░███████╗███╗░░░███╗░█████╗░██████╗░██████╗\n"
        "██╔══██╗██║░░░░░██╔══██╗██║░░░██║██╔══██╗██╔════╝████╗░████║██╔══██╗██╔══██╗██╔════╝\n"
        "██║░░╚═╝██║░░░░░███████║██║░░░██║██║░░██║█████╗░░██╔████╔██║██║░░██║██║░░██║╚█████╗░\n"
        "██║░░██╗██║░░░░░██╔══██║██║░░░██║██║░░██║██╔══╝░░██║╚██╔╝██║██║░░██║██║░░██║░╚═══██╗\n"
        "╚█████╔╝███████╗██║░░██║╚██████╔╝██████╔╝███████╗██║░╚═╝░██║╚█████╔╝██████╔╝██████╔╝\n"
        "░╚════╝░╚══════╝╚═╝░░╚═╝░░░░░░╚═════╝░╚═════╝░╚══════╝╚═╝╚═╝░░░╚═╝░╚════╝░╚═════╝░╝░\n"
        "███████████████████████████████████████████████████████████████████████████████████╝");
}

QFont monoFont(int pointSize)
{
    QFont f(QStringLiteral("DejaVu Sans Mono"));
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    f.setPointSize(pointSize);
    return f;
}

void apply(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window, QColor("#0b1630"));
    p.setColor(QPalette::WindowText, QColor("#d6e4ff"));
    p.setColor(QPalette::Base, QColor("#071027"));
    p.setColor(QPalette::AlternateBase, QColor("#10214a"));
    p.setColor(QPalette::Text, QColor("#d6e4ff"));
    p.setColor(QPalette::Button, QColor("#142a5c"));
    p.setColor(QPalette::ButtonText, QColor("#d6e4ff"));
    p.setColor(QPalette::Highlight, QColor("#2f6fe0"));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::ToolTipBase, QColor("#10214a"));
    p.setColor(QPalette::ToolTipText, QColor("#d6e4ff"));
    p.setColor(QPalette::PlaceholderText, QColor("#5a6b8f"));
    p.setColor(QPalette::Link, QColor("#6fa8ff"));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor("#5a6b8f"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#5a6b8f"));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#5a6b8f"));
    app.setPalette(p);

    app.setStyleSheet(QStringLiteral(R"(
QWidget { background-color: #0b1630; color: #d6e4ff; font-size: 10pt; }
QLabel { background: transparent; }
QFrame#panel, QFrame#bannerPanel {
    background-color: #10214a; border: 1px solid #1f3b7a; border-radius: 10px;
}
QFrame#panel QWidget, QFrame#bannerPanel QWidget { background-color: transparent; }
QLabel#claudemodsText { color: #ff2a2a; }
QLabel#sectionTitle { color: #6fa8ff; font-size: 12pt; font-weight: bold; }
QLabel#menuHeader { color: #3ddc84; font-weight: bold; padding: 6px 2px 2px 2px; }
QLabel#taskLabel { color: #7fd8ff; }
QPushButton {
    background-color: #142a5c; border: 1px solid #24488f; border-radius: 6px;
    padding: 7px 14px; color: #d6e4ff; min-width: 80px;
}
QPushButton:hover { background-color: #1d3a7a; border-color: #3b6fd1; }
QPushButton:pressed { background-color: #0f2147; }
QPushButton:focus { border-color: #6fa8ff; }
QPushButton:disabled { color: #5a6b8f; background-color: #0f1c3d; border-color: #182e5c; }
QPushButton#menuButton { text-align: left; padding: 9px 14px; }
QPushButton#menuButton:hover { color: #ffffff; }
QPushButton#dangerButton { background-color: #5c1420; border-color: #a12b3d; }
QPushButton#dangerButton:hover { background-color: #7a1d2c; }
QPlainTextEdit, QLineEdit, QComboBox, QSpinBox {
    background-color: #071027; border: 1px solid #24488f; border-radius: 5px;
    color: #cfe0ff; selection-background-color: #2f5fbf; padding: 4px;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QPlainTextEdit:focus { border-color: #6fa8ff; }
QComboBox QAbstractItemView { background-color: #071027; border: 1px solid #24488f; selection-background-color: #2f5fbf; }
QProgressBar {
    background-color: #071027; border: 1px solid #24488f; border-radius: 5px;
    text-align: center; color: #ffffff; min-height: 20px;
}
QProgressBar::chunk { background-color: #2f6fe0; border-radius: 4px; }
QScrollBar:vertical { background: #071027; width: 12px; margin: 0; }
QScrollBar::handle:vertical { background: #24488f; border-radius: 5px; min-height: 30px; }
QScrollBar:horizontal { background: #071027; height: 12px; margin: 0; }
QScrollBar::handle:horizontal { background: #24488f; border-radius: 5px; min-width: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QSplitter::handle { background-color: #0b1630; }
QToolTip { background-color: #10214a; color: #d6e4ff; border: 1px solid #24488f; }
QMessageBox, QInputDialog { background-color: #0b1630; }
)"));
}

} // namespace Theme
