#pragma once

#include "domain/Workflow.h"
#include "ui/canvas/NodeGraphicsItem.h"
#include "ui/canvas/NodePortSlotViewModel.h"
#include "ui/canvas/PortSlotHit.h"

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QRectF>

class QGraphicsScene;

namespace vws::ui {

class EdgeGraphicsItem;

// Owns the graphics-item maps for one QGraphicsScene. QGraphicsScene still owns
// the actual item memory; this controller keeps non-owning indexes in sync.
class WorkflowSceneController final : public QObject {
    Q_OBJECT

public:
    explicit WorkflowSceneController(QGraphicsScene* scene, QObject* parent = nullptr);

    void clear();
    void addNodeItem(const domain::Node& node);
    void addEdgeItem(const domain::Edge& edge, const domain::Workflow& workflow);
    void removeNodeItem(const QString& nodeId);
    void removeEdgeItem(const QString& edgeId);

    NodeGraphicsItem* nodeItem(const QString& nodeId) const;
    EdgeGraphicsItem* edgeItem(const QString& edgeId) const;
    QList<NodeGraphicsItem*> nodeItems() const;
    QList<EdgeGraphicsItem*> edgeItems() const;
    QStringList connectedEdgeIdsForNode(const QString& nodeId) const;
    QList<NodeGraphicsItem*> selectedNodeItems() const;
    std::optional<domain::Node> selectedNode() const;

    void clearSelection();
    void selectNode(const QString& nodeId);
    void syncWorkflowNodes(domain::Workflow& workflow) const;
    void updateAllEdgeRoutes(const domain::Workflow& workflow);
    void setNodeStatus(const QString& nodeId, NodeVisualState visualState);
    void setNodePortSlots(
        const QString& nodeId,
        const QList<NodePortSlotViewModel>& inputSlots,
        const QList<NodePortSlotViewModel>& outputSlots);
    void refreshItems();
    NodeGraphicsItem* outputNodeAt(const QPointF& scenePos, qreal hitRadius) const;
    NodeGraphicsItem* inputNodeAt(const QPointF& scenePos, qreal hitRadius, const QString& excludedNodeId = {}) const;
    std::optional<PortSlotHit> outputSlotAt(const QPointF& scenePos, qreal hitRadius) const;
    std::optional<PortSlotHit> inputSlotAt(
        const QPointF& scenePos,
        qreal hitRadius,
        const QString& excludedNodeId = {}) const;

signals:
    void nodeMoved(const QString& nodeId);
    void nodeSelected(const domain::Node& node);
    void nodeDoubleClicked(const domain::Node& node);

private:
    QList<QRectF> nodeObstacleRectsForEdge(const domain::Edge& edge) const;
    int parallelEdgeIndex(const domain::Edge& edge, const domain::Workflow& workflow) const;

    QGraphicsScene* m_scene = nullptr;
    QHash<QString, NodeGraphicsItem*> m_nodeItems;
    QHash<QString, EdgeGraphicsItem*> m_edgeItems;
};

} // namespace vws::ui
