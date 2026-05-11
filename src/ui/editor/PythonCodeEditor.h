#pragma once

#include <QPlainTextEdit>

class QCompleter;

namespace vws::ui {

class LineNumberArea;
class PythonCompleter;
class PythonSyntaxHighlighter;

// 轻量 Python 代码编辑器。
//
// 负责文本编辑体验：行号、当前行高亮、Tab 四空格、多行缩进、自动缩进和基础补全。
// 它不执行代码，也不直接保存 Workflow。
class PythonCodeEditor final : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit PythonCodeEditor(QWidget* parent = nullptr);

    void setCode(const QString& code);
    QString code() const;
    void setTabSpaces(int spaces);

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent* event);

signals:
    void cursorPositionInfoChanged(int line, int column);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect& rect, int dy);
    void highlightCurrentLine();
    void insertSpaces(int count);
    void indentSelection();
    void unindentSelection();
    void handleAutoIndent();
    void showCompletion(bool manual);
    void insertCompletion(const QString& completion);
    QString completionPrefix() const;
    int currentLineIndent() const;
    bool previousLineEndsWithColon() const;
    bool hasSelectionAcrossLines() const;

    LineNumberArea* m_lineNumberArea = nullptr;
    PythonCompleter* m_completer = nullptr;
    PythonSyntaxHighlighter* m_highlighter = nullptr;
    int m_tabSpaces = 4;
};

} // namespace vws::ui
