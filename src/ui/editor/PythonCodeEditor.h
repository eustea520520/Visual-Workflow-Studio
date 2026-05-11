#pragma once

#include <QPlainTextEdit>

class QCompleter;

namespace vws::ui {

class LineNumberArea;
class PythonCompleter;
class PythonSyntaxHighlighter;

// Lightweight Python editor with line numbers, highlighting, indentation, and completion.
// It edits text only; workflow persistence and execution live in higher layers.
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
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyEditorFont();
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
    bool m_applyingEditorFont = false;
};

} // namespace vws::ui
