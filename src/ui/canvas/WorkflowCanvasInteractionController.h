#pragma once

#include <QPoint>
#include <Qt>

class QGraphicsView;

namespace vws::ui {

class WorkflowCanvasInteractionController final {
public:
    bool beginRightButtonPan(QGraphicsView& view, const QPoint& viewportPos);
    bool updateRightButtonPan(QGraphicsView& view, const QPoint& viewportPos);
    bool endRightButtonPan(QGraphicsView& view);
    bool isRightButtonPanning() const;

    bool zoomAtCursor(QGraphicsView& view, int wheelDelta) const;
    void panHorizontally(QGraphicsView& view, int wheelDelta) const;

private:
    bool m_rightButtonPanning = false;
    QPoint m_lastPanViewportPos;
    Qt::CursorShape m_previousCursor = Qt::ArrowCursor;
};

} // namespace vws::ui
