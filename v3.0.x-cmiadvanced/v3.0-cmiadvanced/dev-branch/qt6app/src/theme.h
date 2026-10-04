#pragma once

#include <QFont>
#include <QString>

class QApplication;

namespace Theme {

inline const QString VersionText = QStringLiteral("cmiadvanced Beta v3.0 04-10-2026");
inline const QString Tagline = QStringLiteral(
    "Sailing the 7 seas like Penguin's Eggs Remastersys, Refracta, Systemback and father Knoppix!");

// Palette
inline const QString ClaudemodsRed = QStringLiteral("#ff2a2a");
inline const QString Green = QStringLiteral("#3ddc84");
inline const QString Red = QStringLiteral("#ff5c5c");
inline const QString Yellow = QStringLiteral("#ffd54f");
inline const QString Cyan = QStringLiteral("#7fd8ff");
inline const QString Accent = QStringLiteral("#6fa8ff");
inline const QString Text = QStringLiteral("#d6e4ff");

void apply(QApplication& app);
QString bannerArt();
QFont monoFont(int pointSize);

} // namespace Theme
