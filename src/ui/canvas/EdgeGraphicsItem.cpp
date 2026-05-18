#include "ui/canvas/EdgeGraphicsItem.h"

#include "ui/canvas/ArrowHeadBuilder.h"
#include "ui/canvas/CanvasZ.h"
#include "ui/canvas/EdgeRouter.h"
#include "ui/canvas/NodeGraphicsItem.h"
#include "ui/theme/ThemeManager.h"

#include <QGraphicsSceneHoverEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPen>

#include <utility>

namespace vws::ui {

namespace {

constexpr qreal EdgeHitWidth = 12.0;
constexpr qreal EdgeParallelStep = 8.0;

QString endpointText(const QString& nodeId, const QString& portName, int slotIndex)
{
    if (slotIndex < 0) {
        return QStringLiteral("%1.%2").arg(nodeId, portName);
    }
    return QStringLiteral("%1.%2[%3]").arg(nodeId, portName).arg(slotIndex + 1);
}

} // namespace

EdgeGraphicsItem::EdgeGraphicsItem(domain::Edge edge, NodeGraphicsItem* sourceNode, NodeGraphicsItem* targetNode, QGraphicsItem* parent)
    : QGraphicsPathItem(parent)
    , m_edge(std::move(edge))
    , m_sourceNode(sourceNode)
    , m_targetNode(targetNode)
{
    setFlag(ItemIsSelectable);
    setAcceptHoverEvents(true);
    setZValue(CanvasZ::Edge);
    setToolTip(QStringLiteral("%1 -> %2").arg(
        endpointText(m_edge.fromNode, m_edge.fromPort, m_edge.fromSlot),
        endpointText(m_edge.toNode, m_edge.toPort, m_edge.toSlot)));
    updatePath();
}

domain::Edge EdgeGraphicsItem::edge() const
{
    return m_edge;
}

QString EdgeGraphicsItem::edgeId() const
{
    return m_edge.edgeId;
}

QString EdgeGraphicsItem::sourceNodeId() const
{
    return m_edge.fromNode;
}

QString EdgeGraphicsItem::targetNodeId() const
{
    return m_edge.toNode;
}

bool EdgeGraphicsItem::touchesNode(const QString& nodeId) const
{
    return m_edge.fromNode == nodeId || m_edge.toNode == nodeId;
}

void EdgeGraphicsItem::setRoutingContext(const QList<QRectF>& obstacleRects, int parallelIndex)
{
    m_obstacleRects = obstacleRects;
    m_parallelIndex = parallelIndex;
    updatePath();
}

void EdgeGraphicsItem::updatePath()
{
    if (m_sourceNode == nullptr || m_targetNode == nullptr) {
        return;
    }

    prepareGeometryChange();

    EdgeRouteRequest request;
    request.sourcePortScenePos = m_sourceNode->outputAnchorScenePos(m_edge.fromSlot);
    request.targetPortScenePos = m_targetNode->inputAnchorScenePos(m_edge.toSlot);
    request.sourceNodeRect = m_sourceNode->bodySceneRect();
    request.targetNodeRect = m_targetNode->bodySceneRect();
    request.obstacleNodeRects = m_obstacleRects;
    request.parallelOffset = parallelOffset();

    const EdgeRouter router;
    const auto route = router.route(request);
    setPath(route.smoothPath);
    m_arrowHead = ArrowHeadBuilder::build(route.arrowTip, route.arrowDirection);
    m_boundingRect = route.smoothPath.boundingRect()
        .united(m_arrowHead.boundingRect())
        .adjusted(-EdgeHitWidth, -EdgeHitWidth, EdgeHitWidth, EdgeHitWidth);
    update();
}

QRectF EdgeGraphicsItem::boundingRect() const
{
    return m_boundingRect.isNull() ? QGraphicsPathItem::boundingRect() : m_boundingRect;
}

QPainterPath EdgeGraphicsItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(EdgeHitWidth);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    auto shapePath = stroker.createStroke(path());
    shapePath.addPolygon(m_arrowHead);
    return shapePath;
}

void EdgeGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(currentPen());
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());

    painter->setPen(Qt::NoPen);
    painter->setBrush(currentPen().color());
    painter->drawPolygon(m_arrowHead);
}

void EdgeGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    m_hovered = true;
    update();
    QGraphicsPathItem::hoverEnterEvent(event);
}

void EdgeGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    m_hovered = false;
    update();
    QGraphicsPathItem::hoverLeaveEvent(event);
}

QPen EdgeGraphicsItem::currentPen() const
{
    auto* tm = ThemeManager::instance();

    if (isSelected()) {
        return QPen(tm ? tm->color("edge-selected") : QColor("#2563eb"),
                    3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    }
    if (m_hovered) {
        return QPen(tm ? tm->color("edge-hover") : QColor("#334155"),
                    3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    }
    return QPen(tm ? tm->color("edge-normal") : QColor("#64748b"),
                2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

qreal EdgeGraphicsItem::parallelOffset() const
{
    if (m_parallelIndex <= 0) {
        return 0.0;
    }

    const auto lane = (m_parallelIndex + 1) / 2;
    const auto sign = (m_parallelIndex % 2) == 1 ? 1.0 : -1.0;
    return sign * lane * EdgeParallelStep;
}

} // namespace vws::ui
