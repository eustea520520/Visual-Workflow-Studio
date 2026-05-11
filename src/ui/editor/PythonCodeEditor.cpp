#include "ui/editor/PythonCodeEditor.h"

#include "ui/editor/LineNumberArea.h"
#include "ui/editor/PythonCompleter.h"
#include "ui/editor/PythonSyntaxHighlighter.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QFont>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>

namespace vws::ui {

PythonCodeEditor::PythonCodeEditor(QWidget* parent)
    : QPlainTextEdit(parent)
{
    m_lineNumberArea = new LineNumberArea(this);
    m_completer = new PythonCompleter(this);
    m_highlighter = new PythonSyntaxHighlighter(document());
    m_completer->setWidget(this);
    setProperty("pythonCodeEditor", true);

    connect(this, &QPlainTextEdit::blockCountChanged, this, [this]() { updateLineNumberAreaWidth(); });
    connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect& rect, int dy) { updateLineNumberArea(rect, dy); });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        highlightCurrentLine();
        const auto cursor = textCursor();
        emit cursorPositionInfoChanged(cursor.blockNumber() + 1, cursor.positionInBlock() + 1);
    });
    connect(this, &QPlainTextEdit::textChanged, this, [this]() {
        m_completer->refreshFromCode(toPlainText());
    });
    connect(m_completer, QOverload<const QString&>::of(&QCompleter::activated), this, &PythonCodeEditor::insertCompletion);

    applyEditorFont();
    setLineWrapMode(QPlainTextEdit::NoWrap);
    updateLineNumberAreaWidth();
    highlightCurrentLine();

    if (auto* tm = ThemeManager::instance()) {
        connect(tm, &ThemeManager::themeChanged, this, [this]() {
            applyEditorFont();
            m_lineNumberArea->update();
            highlightCurrentLine();
            if (m_highlighter != nullptr) {
                m_highlighter->refreshTheme();
            }
        });
    }
}

void PythonCodeEditor::changeEvent(QEvent* event)
{
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange) {
        applyEditorFont();
        updateLineNumberAreaWidth();
    }
}

void PythonCodeEditor::applyEditorFont()
{
    if (m_applyingEditorFont) {
        return;
    }

    QFont editorFont(QStringLiteral("Consolas"));
    editorFont.setPointSize(11);
    editorFont.setFixedPitch(true);
    editorFont.setStyleHint(QFont::Monospace, QFont::PreferMatch);
    if (font() != editorFont) {
        m_applyingEditorFont = true;
        setFont(editorFont);
        m_applyingEditorFont = false;
    }
}

void PythonCodeEditor::setCode(const QString& code)
{
    setPlainText(code);
    m_completer->refreshFromCode(code);
}

QString PythonCodeEditor::code() const
{
    return toPlainText();
}

void PythonCodeEditor::setTabSpaces(int spaces)
{
    m_tabSpaces = qMax(1, spaces);
}

int PythonCodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    return 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void PythonCodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(m_lineNumberArea);
    auto* tm = ThemeManager::instance();
    painter.fillRect(event->rect(), tm ? tm->color("editor-line-bg") : QColor("#f3f4f6"));

    auto block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.setPen(tm ? tm->color("editor-line-text") : QColor("#6b7280"));
            painter.drawText(0, top, m_lineNumberArea->width() - 6, fontMetrics().height(),
                Qt::AlignRight, QString::number(blockNumber + 1));
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void PythonCodeEditor::keyPressEvent(QKeyEvent* event)
{
    if (m_completer->popup()->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:
            break;
        }
    }

    if (event->matches(QKeySequence::InsertParagraphSeparator) || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QPlainTextEdit::keyPressEvent(event);
        handleAutoIndent();
        return;
    }

    if (event->key() == Qt::Key_Tab) {
        hasSelectionAcrossLines() ? indentSelection() : insertSpaces(m_tabSpaces);
        return;
    }

    if (event->key() == Qt::Key_Backtab) {
        unindentSelection();
        return;
    }

    const bool manualCompletion = event->modifiers().testFlag(Qt::ControlModifier) && event->key() == Qt::Key_Space;
    if (!manualCompletion) {
        QPlainTextEdit::keyPressEvent(event);
    }

    if (manualCompletion || (!event->text().isEmpty() && event->text().at(0).isLetterOrNumber())) {
        showCompletion(manualCompletion);
    } else {
        m_completer->popup()->hide();
    }
}

void PythonCodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    const auto cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void PythonCodeEditor::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void PythonCodeEditor::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy != 0) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }
    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth();
    }
}

void PythonCodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> selections;
    QTextEdit::ExtraSelection selection;
    auto* tm = ThemeManager::instance();
    selection.format.setBackground(tm ? tm->color("editor-current-line") : QColor("#eff6ff"));
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    selections.append(selection);
    setExtraSelections(selections);
}

void PythonCodeEditor::insertSpaces(int count)
{
    textCursor().insertText(QString(count, ' '));
}

void PythonCodeEditor::indentSelection()
{
    auto cursor = textCursor();
    const auto start = cursor.selectionStart();
    const auto end = cursor.selectionEnd();
    cursor.setPosition(start);
    cursor.beginEditBlock();
    while (cursor.position() <= end) {
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.insertText(QString(m_tabSpaces, ' '));
        if (!cursor.movePosition(QTextCursor::NextBlock)) {
            break;
        }
        if (cursor.position() > end + m_tabSpaces) {
            break;
        }
    }
    cursor.endEditBlock();
}

void PythonCodeEditor::unindentSelection()
{
    auto cursor = textCursor();
    const auto start = cursor.selectionStart();
    const auto end = cursor.selectionEnd();
    cursor.setPosition(start);
    cursor.beginEditBlock();
    while (cursor.position() <= end) {
        cursor.movePosition(QTextCursor::StartOfBlock);
        for (int i = 0; i < m_tabSpaces; ++i) {
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            if (cursor.selectedText() == " ") {
                cursor.removeSelectedText();
            } else {
                cursor.clearSelection();
                break;
            }
        }
        if (!cursor.movePosition(QTextCursor::NextBlock)) {
            break;
        }
        if (cursor.position() > end) {
            break;
        }
    }
    cursor.endEditBlock();
}

void PythonCodeEditor::handleAutoIndent()
{
    auto indent = currentLineIndent();
    if (previousLineEndsWithColon()) {
        indent += m_tabSpaces;
    }
    insertSpaces(indent);
}

void PythonCodeEditor::showCompletion(bool manual)
{
    const auto prefix = completionPrefix();
    if (!manual && prefix.size() < 2) {
        m_completer->popup()->hide();
        return;
    }

    m_completer->setCompletionPrefix(prefix);
    auto popup = m_completer->popup();
    popup->setCurrentIndex(m_completer->completionModel()->index(0, 0));

    auto rect = cursorRect();
    rect.setWidth(popup->sizeHintForColumn(0) + popup->verticalScrollBar()->sizeHint().width());
    m_completer->complete(rect);
}

void PythonCodeEditor::insertCompletion(const QString& completion)
{
    auto cursor = textCursor();
    const auto prefix = completionPrefix();
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, prefix.size());
    cursor.insertText(completion);
    setTextCursor(cursor);
}

QString PythonCodeEditor::completionPrefix() const
{
    auto cursor = textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    return cursor.selectedText();
}

int PythonCodeEditor::currentLineIndent() const
{
    const auto previousBlock = textCursor().block().previous();
    const auto text = previousBlock.isValid() ? previousBlock.text() : QString{};
    int count = 0;
    while (count < text.size() && text.at(count) == ' ') {
        ++count;
    }
    return count;
}

bool PythonCodeEditor::previousLineEndsWithColon() const
{
    const auto previousBlock = textCursor().block().previous();
    return previousBlock.isValid() && previousBlock.text().trimmed().endsWith(':');
}

bool PythonCodeEditor::hasSelectionAcrossLines() const
{
    const auto cursor = textCursor();
    return cursor.hasSelection() && cursor.selectionStart() != cursor.selectionEnd();
}

} // namespace vws::ui
