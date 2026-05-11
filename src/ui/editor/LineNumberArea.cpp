#include "ui/editor/LineNumberArea.h"

#include "ui/editor/PythonCodeEditor.h"

namespace vws::ui {

LineNumberArea::LineNumberArea(PythonCodeEditor* editor)
    : QWidget(editor)
    , m_editor(editor)
{
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_editor != nullptr ? m_editor->lineNumberAreaWidth() : 0, 0);
}

void LineNumberArea::paintEvent(QPaintEvent* event)
{
    if (m_editor != nullptr) {
        m_editor->lineNumberAreaPaintEvent(event);
    }
}

} // namespace vws::ui
