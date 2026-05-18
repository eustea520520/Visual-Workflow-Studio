#include "ui/canvas/EdgeDragController.h"

#include "ui/canvas/CanvasZ.h"
#include "ui/theme/ThemeManager.h"

#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QPen>

namespace vws::ui {

EdgeDragController::~EdgeDragController()
{
    clear(nullptr);
}

bool EdgeDragController::begin(QGraphicsScene* scene, const domain::EdgeEndpoint& source, const QPointF& startScenePos)
{
    if (scene == nullptr || source.nodeId.trimmed().isEmpty() || source.portName.trimmed().isEmpty()) {
        return false;
    }

    clear(scene);
    m_source = source;
    m_dragging = true;
    m_startScenePos = startScenePos;
    m_previewItem = createPreviewItem();
    m_previewItem->setPath(previewPath(m_startScenePos, startScenePos));
    scene->addItem(m_previewItem);
    return true;
}

bool EdgeDragController::isDragging() const
{
    return m_dragging;
}

QString EdgeDragController::sourceNodeId() const
{
    return m_source.nodeId;
}

domain::EdgeEndpoint EdgeDragController::sourceEndpoint() const
{
    return m_source;
}

bool EdgeDragController::sourceIs(const QString& nodeId) const
{
    return m_dragging && m_source.nodeId == nodeId;
}

void EdgeDragController::update(const QPointF& scenePos)
{
    if (m_previewItem == nullptr) {
        return;
    }

    m_previewItem->setPath(previewPath(m_startScenePos, scenePos));
}

void EdgeDragController::clear(QGraphicsScene* scene)
{
    m_source = {};
    m_dragging = false;
    if (m_previewItem == nullptr) {
        return;
    }

    auto* ownerScene = scene != nullptr ? scene : m_previewItem->scene();
    if (ownerScene != nullptr && m_previewItem->scene() == ownerScene) {
        ownerScene->removeItem(m_previewItem);
    }
    delete m_previewItem;
    m_previewItem = nullptr;
}

void EdgeDragController::refreshTheme()
{
    if (m_previewItem == nullptr) {
        return;
    }

    auto* tm = ThemeManager::instance();
    m_previewItem->setPen(QPen(
        tm ? tm->color("edge-preview") : QColor("#2563EB"),
        2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
}

QGraphicsPathItem* EdgeDragController::createPreviewItem() const
{
    auto* item = new QGraphicsPathItem();
    item->setZValue(CanvasZ::EdgePreview);
    auto* tm = ThemeManager::instance();
    item->setPen(QPen(
        tm ? tm->color("edge-preview") : QColor("#2563EB"),
        2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    return item;
}

QPainterPath EdgeDragController::previewPath(const QPointF& start, const QPointF& end) const
{
    const auto dx = qMax<qreal>(80.0, qAbs(end.x() - start.x()) * 0.45);
    QPainterPath path(start);
    path.cubicTo(
        QPointF(start.x() + dx, start.y()),
        QPointF(end.x() - dx, end.y()),
        end);
    return path;
}

} // namespace vws::ui
