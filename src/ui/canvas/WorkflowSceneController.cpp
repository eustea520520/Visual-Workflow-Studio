#include "ui/canvas/WorkflowSceneController.h"

#include "ui/canvas/EdgeGraphicsItem.h"

#include <QGraphicsScene>
#include <QLineF>

namespace vws::ui {

WorkflowSceneController::WorkflowSceneController(QGraphicsScene* scene, QObject* parent)
    : QObject(parent)
    , m_scene(scene)
{
}

void WorkflowSceneController::clear()
{
    m_nodeItems.clear();
    m_edgeItems.clear();
    if (m_scene != nullptr) {
        m_scene->clear();
    }
}

void WorkflowSceneController::addNodeItem(const domain::Node& node)
{
    if (m_scene == nullptr) {
        return;
    }

    auto* item = new NodeGraphicsItem(node);
    m_scene->addItem(item);
    m_nodeItems.insert(node.nodeId, item);

    connect(item, &NodeGraphicsItem::nodeMoved, this, [this](const QString& nodeId, const QPointF&) {
        emit nodeMoved(nodeId);
    });
    connect(item, &NodeGraphicsItem::nodeSelected, this, &WorkflowSceneController::nodeSelected);
    connect(item, &NodeGraphicsItem::nodeDoubleClicked, this, &WorkflowSceneController::nodeDoubleClicked);
}

void WorkflowSceneController::addEdgeItem(const domain::Edge& edge, const domain::Workflow& workflow)
{
    if (m_scene == nullptr) {
        return;
    }

    auto* sourceNode = m_nodeItems.value(edge.fromNode, nullptr);
    auto* targetNode = m_nodeItems.value(edge.toNode, nullptr);
    if (sourceNode == nullptr || targetNode == nullptr) {
        return;
    }

    auto* item = new EdgeGraphicsItem(edge, sourceNode, targetNode);
    m_scene->addItem(item);
    m_edgeItems.insert(edge.edgeId, item);
    item->setRoutingContext(nodeObstacleRectsForEdge(edge), parallelEdgeIndex(edge, workflow));
}

void WorkflowSceneController::removeNodeItem(const QString& nodeId)
{
    auto* item = m_nodeItems.take(nodeId);
    if (item == nullptr || m_scene == nullptr) {
        return;
    }

    m_scene->removeItem(item);
    delete item;
}

void WorkflowSceneController::removeEdgeItem(const QString& edgeId)
{
    auto* item = m_edgeItems.take(edgeId);
    if (item == nullptr || m_scene == nullptr) {
        return;
    }

    m_scene->removeItem(item);
    delete item;
}

NodeGraphicsItem* WorkflowSceneController::nodeItem(const QString& nodeId) const
{
    return m_nodeItems.value(nodeId, nullptr);
}

EdgeGraphicsItem* WorkflowSceneController::edgeItem(const QString& edgeId) const
{
    return m_edgeItems.value(edgeId, nullptr);
}

QList<NodeGraphicsItem*> WorkflowSceneController::nodeItems() const
{
    return m_nodeItems.values();
}

QList<EdgeGraphicsItem*> WorkflowSceneController::edgeItems() const
{
    return m_edgeItems.values();
}

QStringList WorkflowSceneController::connectedEdgeIdsForNode(const QString& nodeId) const
{
    QStringList edgeIds;
    for (auto it = m_edgeItems.cbegin(); it != m_edgeItems.cend(); ++it) {
        if (it.value() != nullptr && it.value()->touchesNode(nodeId)) {
            edgeIds.append(it.key());
        }
    }
    return edgeIds;
}

QList<NodeGraphicsItem*> WorkflowSceneController::selectedNodeItems() const
{
    QList<NodeGraphicsItem*> nodes;
    if (m_scene == nullptr) {
        return nodes;
    }

    for (auto* item : m_scene->selectedItems()) {
        if (auto* nodeItem = dynamic_cast<NodeGraphicsItem*>(item)) {
            nodes.append(nodeItem);
        }
    }
    return nodes;
}

std::optional<domain::Node> WorkflowSceneController::selectedNode() const
{
    const auto nodes = selectedNodeItems();
    if (nodes.isEmpty()) {
        return std::nullopt;
    }
    return nodes.first()->node();
}

void WorkflowSceneController::clearSelection()
{
    if (m_scene != nullptr) {
        m_scene->clearSelection();
    }
}

void WorkflowSceneController::selectNode(const QString& nodeId)
{
    if (auto* item = nodeItem(nodeId)) {
        item->setSelected(true);
    }
}

void WorkflowSceneController::syncWorkflowNodes(domain::Workflow& workflow) const
{
    for (auto& node : workflow.nodes) {
        auto* item = m_nodeItems.value(node.nodeId, nullptr);
        if (item != nullptr) {
            node = item->node();
        }
    }
}

void WorkflowSceneController::updateAllEdgeRoutes(const domain::Workflow& workflow)
{
    for (auto* edgeItem : m_edgeItems) {
        if (edgeItem == nullptr) {
            continue;
        }
        const auto edge = edgeItem->edge();
        edgeItem->setRoutingContext(nodeObstacleRectsForEdge(edge), parallelEdgeIndex(edge, workflow));
    }
}

void WorkflowSceneController::setNodeStatus(const QString& nodeId, NodeVisualState visualState)
{
    auto* item = nodeItem(nodeId);
    if (item != nullptr) {
        item->setVisualState(visualState);
    }
}

void WorkflowSceneController::setNodePortSlots(
    const QString& nodeId,
    const QList<NodePortSlotViewModel>& inputSlots,
    const QList<NodePortSlotViewModel>& outputSlots)
{
    auto* item = nodeItem(nodeId);
    if (item != nullptr) {
        item->setPortSlots(inputSlots, outputSlots);
    }
}

void WorkflowSceneController::refreshItems()
{
    if (m_scene == nullptr) {
        return;
    }

    for (auto* item : m_scene->items()) {
        item->update();
    }
}

NodeGraphicsItem* WorkflowSceneController::outputNodeAt(const QPointF& scenePos, qreal hitRadius) const
{
    for (auto* nodeItem : m_nodeItems) {
        if (nodeItem == nullptr || nodeItem->node().outputPorts.isEmpty()) {
            continue;
        }
        if (QLineF(scenePos, nodeItem->outputAnchorScenePos()).length() <= hitRadius) {
            return nodeItem;
        }
    }
    return nullptr;
}

std::optional<PortSlotHit> WorkflowSceneController::outputSlotAt(const QPointF& scenePos, qreal hitRadius) const
{
    for (auto* nodeItem : m_nodeItems) {
        if (nodeItem == nullptr) {
            continue;
        }
        if (auto hit = nodeItem->outputSlotAt(scenePos, hitRadius); hit.has_value()) {
            return hit;
        }
    }
    return std::nullopt;
}

std::optional<PortSlotHit> WorkflowSceneController::inputSlotAt(
    const QPointF& scenePos,
    qreal hitRadius,
    const QString& excludedNodeId) const
{
    for (auto* nodeItem : m_nodeItems) {
        if (nodeItem == nullptr) {
            continue;
        }
        if (!excludedNodeId.isEmpty() && nodeItem->nodeId() == excludedNodeId) {
            continue;
        }
        if (auto hit = nodeItem->inputSlotAt(scenePos, hitRadius); hit.has_value()) {
            return hit;
        }
    }
    return std::nullopt;
}

NodeGraphicsItem* WorkflowSceneController::inputNodeAt(const QPointF& scenePos, qreal hitRadius, const QString& excludedNodeId) const
{
    for (auto* nodeItem : m_nodeItems) {
        if (nodeItem == nullptr) {
            continue;
        }
        if (!excludedNodeId.isEmpty() && nodeItem->nodeId() == excludedNodeId) {
            continue;
        }
        if (nodeItem->node().inputPorts.isEmpty()) {
            continue;
        }
        if (QLineF(scenePos, nodeItem->inputAnchorScenePos()).length() <= hitRadius) {
            return nodeItem;
        }
    }
    return nullptr;
}

QList<QRectF> WorkflowSceneController::nodeObstacleRectsForEdge(const domain::Edge& edge) const
{
    QList<QRectF> rects;
    for (auto* nodeItem : m_nodeItems) {
        if (nodeItem == nullptr) {
            continue;
        }
        const auto nodeId = nodeItem->nodeId();
        if (nodeId == edge.fromNode || nodeId == edge.toNode) {
            continue;
        }
        rects.append(nodeItem->bodySceneRect());
    }
    return rects;
}

int WorkflowSceneController::parallelEdgeIndex(const domain::Edge& edge, const domain::Workflow& workflow) const
{
    int index = 0;
    for (const auto& existingEdge : workflow.edges) {
        if (existingEdge.edgeId == edge.edgeId) {
            return index;
        }
        if (existingEdge.fromNode == edge.fromNode && existingEdge.toNode == edge.toNode) {
            ++index;
        }
    }
    return index;
}

} // namespace vws::ui
