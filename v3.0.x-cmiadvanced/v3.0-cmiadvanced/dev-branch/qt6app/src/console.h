#pragma once

#include <QColor>
#include <QPlainTextEdit>

// Output pane. Handles raw process output, including carriage-return progress
// lines (dd status=progress) and ANSI colour codes, which are stripped.
class Console : public QPlainTextEdit
{
public:
    explicit Console(QWidget* parent = nullptr);

    void appendStream(const QString& text, const QColor& color);
    void appendLine(const QString& text, const QColor& color);

private:
    bool atBottom() const;
    void scrollToBottom();

    bool m_pendingCarriageReturn = false;
};
