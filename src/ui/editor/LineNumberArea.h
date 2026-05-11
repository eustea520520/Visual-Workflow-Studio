#pragma once

#include <QWidget>

namespace vws::ui {

class PythonCodeEditor;

// PythonCodeEditor 左侧的行号区域。
// 它是一个很薄的 QWidget，实际绘制逻辑委托给 PythonCodeEditor，避免两边重复计算行高。
class LineNumberArea final : public QWidget {
public:
    explicit LineNumberArea(PythonCodeEditor* editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    PythonCodeEditor* m_editor = nullptr;
};

} // namespace vws::ui
