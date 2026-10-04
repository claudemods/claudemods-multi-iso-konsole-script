#include "console.h"

#include "theme.h"

#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>

Console::Console(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setMaximumBlockCount(20000);
    setFont(Theme::monoFont(10));
    setUndoRedoEnabled(false);
}

bool Console::atBottom() const
{
    const QScrollBar* bar = verticalScrollBar();
    return bar->value() >= bar->maximum() - 4;
}

void Console::scrollToBottom()
{
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void Console::appendStream(const QString& text, const QColor& color)
{
    static const QRegularExpression ansi(QStringLiteral("\x1B(\\[[0-9;?]*[ -/]*[@-~]|\\][^\x07]*\x07|[()][A-Za-z0-9])"));
    QString clean = text;
    clean.remove(ansi);
    if (clean.isEmpty())
        return;

    const bool follow = atBottom();
    QTextCursor cur(document());
    cur.movePosition(QTextCursor::End);

    QTextCharFormat fmt;
    fmt.setForeground(color);

    QString run;
    auto flushRun = [&] {
        if (!run.isEmpty()) {
            cur.insertText(run, fmt);
            run.clear();
        }
    };

    for (const QChar ch : clean) {
        if (ch == QLatin1Char('\r')) {
            flushRun();
            m_pendingCarriageReturn = true;
        } else if (ch == QLatin1Char('\n')) {
            flushRun();
            m_pendingCarriageReturn = false;
            cur.insertBlock();
        } else {
            if (m_pendingCarriageReturn) {
                flushRun();
                cur.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
                cur.removeSelectedText();
                m_pendingCarriageReturn = false;
            }
            run += ch;
        }
    }
    flushRun();

    if (follow)
        scrollToBottom();
}

void Console::appendLine(const QString& text, const QColor& color)
{
    const bool follow = atBottom();
    QTextCursor cur(document());
    cur.movePosition(QTextCursor::End);
    if (!cur.block().text().isEmpty())
        cur.insertBlock();

    QTextCharFormat fmt;
    fmt.setForeground(color);
    cur.insertText(text, fmt);
    cur.insertBlock();
    m_pendingCarriageReturn = false;

    if (follow)
        scrollToBottom();
}
