#pragma once

#include "domain/Edge.h"

#include <QGraphicsPathItem>
#include <QList>
#include <QPolygonF>
#include <QRectF>

class QGraphicsSceneHoverEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace vws::ui {

class NodeGraphicsItem;

// EdgeGraphicsItem 是画布上的连接线。
//
// 它保存一份 domain::Edge，负责根据两端节点的位置重算贝塞尔曲线路径。
// 它不拥有节点，只保存节点指针；节点由 WorkflowCanvas 统一创建和删除。
class EdgeGraphicsItem final : public QGraphicsPathItem {
public:
    EdgeGraphicsItem(domain::Edge edge, NodeGraphicsItem* sourceNode, NodeGraphicsItem* targetNode, QGraphicsItem* parent = nullptr);

    domain::Edge edge() const;
    QString edgeId() const;
    QString sourceNodeId() const;
    QString targetNodeId() const;
    bool touchesNode(const QString& nodeId) const;
    void updatePath();
    void setRoutingContext(const QList<QRectF>& obstacleRects, int parallelIndex);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    QPen currentPen() const;
    qreal parallelOffset() const;

    domain::Edge m_edge;
    NodeGraphicsItem* m_sourceNode = nullptr;
    NodeGraphicsItem* m_targetNode = nullptr;
    QList<QRectF> m_obstacleRects;
    QPolygonF m_arrowHead;
    QRectF m_boundingRect;
    int m_parallelIndex = 0;
    bool m_hovered = false;
};

} // namespace vws::ui
