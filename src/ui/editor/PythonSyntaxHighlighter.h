#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

namespace vws::ui {

// 轻量 Python 语法高亮器。
// 使用正则表达式覆盖常见关键字、字符串、注释、数字、函数名和类名。
class PythonSyntaxHighlighter final : public QSyntaxHighlighter {
public:
    explicit PythonSyntaxHighlighter(QTextDocument* parent = nullptr);

    void refreshTheme();

protected:
    void highlightBlock(const QString& text) override;

private:
    void highlightMultilineString(const QString& text, const QString& delimiter, int state);
    void buildRules();

    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QList<Rule> m_rules;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;
};

} // namespace vws::ui
