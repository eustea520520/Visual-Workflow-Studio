#include "ui/canvas/WorkflowCanvasInteractionController.h"

#include <QGraphicsView>
#include <QScrollBar>

namespace vws::ui {

namespace {

constexpr qreal ZoomStep = 1.15;
constexpr qreal MinZoom = 0.25;
constexpr qreal MaxZoom = 3.0;

} // namespace

bool WorkflowCanvasInteractionController::beginRightButtonPan(QGraphicsView& view, const QPoint& viewportPos)
{
    m_rightButtonPanning = true;
    m_lastPanViewportPos = viewportPos;
    m_previousCursor = view.cursor().shape();
    view.setCursor(Qt::ClosedHandCursor);
    return true;
}

bool WorkflowCanvasInteractionController::updateRightButtonPan(QGraphicsView& view, const QPoint& viewportPos)
{
    if (!m_rightButtonPanning) {
        return false;
    }

    const auto delta = viewportPos - m_lastPanViewportPos;
    m_lastPanViewportPos = viewportPos;
    view.horizontalScrollBar()->setValue(view.horizontalScrollBar()->value() - delta.x());
    view.verticalScrollBar()->setValue(view.verticalScrollBar()->value() - delta.y());
    return true;
}

bool WorkflowCanvasInteractionController::endRightButtonPan(QGraphicsView& view)
{
    if (!m_rightButtonPanning) {
        return false;
    }

    m_rightButtonPanning = false;
    view.setCursor(m_previousCursor);
    return true;
}

bool WorkflowCanvasInteractionController::isRightButtonPanning() const
{
    return m_rightButtonPanning;
}

bool WorkflowCanvasInteractionController::zoomAtCursor(QGraphicsView& view, int wheelDelta) const
{
    if (wheelDelta == 0) {
        return false;
    }

    const auto currentZoom = view.transform().m11();
    const auto factor = wheelDelta > 0 ? ZoomStep : 1.0 / ZoomStep;
    const auto nextZoom = currentZoom * factor;
    if (nextZoom < MinZoom || nextZoom > MaxZoom) {
        return false;
    }

    view.scale(factor, factor);
    return true;
}

void WorkflowCanvasInteractionController::panHorizontally(QGraphicsView& view, int wheelDelta) const
{
    view.horizontalScrollBar()->setValue(view.horizontalScrollBar()->value() - wheelDelta);
}

} // namespace vws::ui
