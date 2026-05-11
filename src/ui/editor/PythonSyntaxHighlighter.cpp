#include "ui/editor/PythonSyntaxHighlighter.h"

#include <QFont>
#include <QTextDocument>

namespace vws::ui {

namespace {

QTextCharFormat makeTextFormat(const QColor& color, bool bold = false, bool italic = false)
{
    QTextCharFormat textFormat;
    textFormat.setForeground(color);
    if (bold) {
        textFormat.setFontWeight(QFont::Bold);
    }
    textFormat.setFontItalic(italic);
    return textFormat;
}

} // namespace

PythonSyntaxHighlighter::PythonSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    const auto keywordFormat = makeTextFormat(QColor("#7c3aed"), true);
    const auto numberFormat = makeTextFormat(QColor("#b45309"));
    const auto functionFormat = makeTextFormat(QColor("#2563eb"), true);
    const auto classFormat = makeTextFormat(QColor("#0f766e"), true);
    m_stringFormat = makeTextFormat(QColor("#15803d"));
    m_commentFormat = makeTextFormat(QColor("#6b7280"), false, true);

    const QStringList keywords = {
        "def", "class", "if", "else", "elif", "for", "while", "try", "except", "finally",
        "return", "import", "from", "as", "with", "lambda", "yield", "in", "is", "and", "or",
        "not", "None", "True", "False",
    };

    for (const auto& keyword : keywords) {
        m_rules.append(Rule{QRegularExpression(QString("\\b%1\\b").arg(keyword)), keywordFormat});
    }

    m_rules.append(Rule{QRegularExpression("\\b[0-9]+(?:\\.[0-9]+)?\\b"), numberFormat});
    m_rules.append(Rule{QRegularExpression("\\bdef\\s+([A-Za-z_][A-Za-z0-9_]*)"), functionFormat});
    m_rules.append(Rule{QRegularExpression("\\bclass\\s+([A-Za-z_][A-Za-z0-9_]*)"), classFormat});
}

void PythonSyntaxHighlighter::highlightBlock(const QString& text)
{
    setCurrentBlockState(0);

    for (const auto& rule : m_rules) {
        auto matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            const auto match = matchIterator.next();
            const auto start = match.lastCapturedIndex() >= 1 ? match.capturedStart(1) : match.capturedStart();
            const auto length = match.lastCapturedIndex() >= 1 ? match.capturedLength(1) : match.capturedLength();
            setFormat(start, length, rule.format);
        }
    }

    auto stringIterator = QRegularExpression(R"((\"[^\"\\]*(?:\\.[^\"\\]*)*\"|'[^'\\]*(?:\\.[^'\\]*)*'))").globalMatch(text);
    while (stringIterator.hasNext()) {
        const auto match = stringIterator.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_stringFormat);
    }

    const auto commentIndex = text.indexOf('#');
    if (commentIndex >= 0) {
        setFormat(commentIndex, text.size() - commentIndex, m_commentFormat);
    }

    // 三引号字符串可能跨越多个 QTextBlock。必须使用 blockState 记录“上一行仍在字符串中”，
    // 否则中间行会被当成普通 Python 代码，导致关键字和注释被错误高亮。
    highlightMultilineString(text, "\"\"\"", 1);
    if (currentBlockState() == 0) {
        highlightMultilineString(text, "'''", 2);
    }
}

void PythonSyntaxHighlighter::highlightMultilineString(const QString& text, const QString& delimiter, int state)
{
    const auto delimiterLength = delimiter.size();
    int startIndex = previousBlockState() == state ? 0 : text.indexOf(delimiter);

    while (startIndex >= 0) {
        const auto searchStart = previousBlockState() == state && startIndex == 0
            ? 0
            : startIndex + delimiterLength;
        const auto endIndex = text.indexOf(delimiter, searchStart);

        int length = 0;
        if (endIndex == -1) {
            setCurrentBlockState(state);
            length = text.size() - startIndex;
        } else {
            length = endIndex - startIndex + delimiterLength;
        }

        setFormat(startIndex, length, m_stringFormat);

        if (endIndex == -1) {
            return;
        }
        startIndex = text.indexOf(delimiter, startIndex + length);
    }
}

} // namespace vws::ui
