#include "application/WorkflowEditService.h"

#include <QHash>
#include <QUuid>

namespace vws::application {

namespace {

const domain::Node* findNode(const domain::Workflow& workflow, const QString& nodeId)
{
    for (const auto& node : workflow.nodes) {
        if (node.nodeId == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

bool containsPort(const QStringList& ports, const QString& port)
{
    return !port.trimmed().isEmpty() && ports.contains(port);
}

int normalizeRotation(int degrees)
{
    int normalized = degrees % 360;
    if (normalized < 0) {
        normalized += 360;
    }

    const int step = ((normalized + 45) / 90) * 90;
    return step % 360;
}
} // namespace

domain::Node WorkflowEditService::addNode(domain::Workflow& workflow, domain::Node node)
{
    if (node.nodeId.trimmed().isEmpty()) {
        node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    workflow.nodes.append(node);
    return node;
}

bool WorkflowEditService::updateNode(domain::Workflow& workflow, const domain::Node& node)
{
    for (auto& workflowNode : workflow.nodes) {
        if (workflowNode.nodeId == node.nodeId) {
            workflowNode = node;
            return true;
        }
    }
    return false;
}

bool WorkflowEditService::rotateNode(domain::Workflow& workflow, const QString& nodeId, int deltaDegrees)
{
    for (auto& workflowNode : workflow.nodes) {
        if (workflowNode.nodeId == nodeId) {
            workflowNode.rotationDegrees = normalizeRotation(workflowNode.rotationDegrees + deltaDegrees);
            return true;
        }
    }
    return false;
}

bool WorkflowEditService::connectNodes(
    domain::Workflow& workflow,
    const domain::Node& sourceNode,
    const domain::Node& targetNode,
    domain::Edge& createdEdge)
{
    if (sourceNode.nodeId == targetNode.nodeId
        || sourceNode.outputPorts.isEmpty()
        || targetNode.inputPorts.isEmpty()) {
        return false;
    }

    return connectNodes(
        workflow,
        domain::EdgeEndpoint{sourceNode.nodeId, sourceNode.outputPorts.first(), -1},
        domain::EdgeEndpoint{targetNode.nodeId, targetNode.inputPorts.first(), -1},
        createdEdge);
}

bool WorkflowEditService::connectNodes(
    domain::Workflow& workflow,
    const domain::EdgeEndpoint& source,
    const domain::EdgeEndpoint& target,
    domain::Edge& createdEdge)
{
    if (source.nodeId == target.nodeId
        || source.nodeId.trimmed().isEmpty()
        || target.nodeId.trimmed().isEmpty()
        || source.portName.trimmed().isEmpty()
        || target.portName.trimmed().isEmpty()
        || source.slotIndex < -1
        || target.slotIndex < -1) {
        return false;
    }

    const auto* sourceNode = findNode(workflow, source.nodeId);
    const auto* targetNode = findNode(workflow, target.nodeId);
    if (sourceNode == nullptr || targetNode == nullptr) {
        return false;
    }
    if (!containsPort(sourceNode->outputPorts, source.portName)
        || !containsPort(targetNode->inputPorts, target.portName)) {
        return false;
    }

    domain::Edge edge;
    edge.edgeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    edge.fromNode = source.nodeId;
    edge.fromPort = source.portName;
    edge.fromSlot = source.slotIndex;
    edge.toNode = target.nodeId;
    edge.toPort = target.portName;
    edge.toSlot = target.slotIndex;

    workflow.edges.append(edge);
    createdEdge = edge;
    return true;
}

void WorkflowEditService::removeEdges(domain::Workflow& workflow, const QSet<QString>& edgeIds)
{
    for (qsizetype index = workflow.edges.size() - 1; index >= 0; --index) {
        if (edgeIds.contains(workflow.edges.at(index).edgeId)) {
            workflow.edges.removeAt(index);
        }
    }
}

void WorkflowEditService::removeNodes(domain::Workflow& workflow, const QSet<QString>& nodeIds)
{
    for (qsizetype index = workflow.edges.size() - 1; index >= 0; --index) {
        const auto& edge = workflow.edges.at(index);
        if (nodeIds.contains(edge.fromNode) || nodeIds.contains(edge.toNode)) {
            workflow.edges.removeAt(index);
        }
    }

    for (qsizetype index = workflow.nodes.size() - 1; index >= 0; --index) {
        if (nodeIds.contains(workflow.nodes.at(index).nodeId)) {
            workflow.nodes.removeAt(index);
        }
    }
}

domain::Workflow WorkflowEditService::subgraphForNodes(
    const domain::Workflow& workflow,
    const QSet<QString>& nodeIds)
{
    domain::Workflow subgraph;
    subgraph.workflowId = workflow.workflowId;
    subgraph.workspaceId = workflow.workspaceId;
    subgraph.name = workflow.name;

    for (const auto& node : workflow.nodes) {
        if (nodeIds.contains(node.nodeId)) {
            subgraph.nodes.append(node);
        }
    }

    for (const auto& edge : workflow.edges) {
        if (nodeIds.contains(edge.fromNode) && nodeIds.contains(edge.toNode)) {
            subgraph.edges.append(edge);
        }
    }

    return subgraph;
}

domain::Workflow WorkflowEditService::duplicateSubgraph(
    const domain::Workflow& sourceSubgraph,
    qreal offset,
    const QString& copiedNameSuffix)
{
    domain::Workflow duplicate;
    duplicate.workflowId = sourceSubgraph.workflowId;
    duplicate.workspaceId = sourceSubgraph.workspaceId;
    duplicate.name = sourceSubgraph.name;

    QHash<QString, QString> oldToNewNodeIds;
    for (auto node : sourceSubgraph.nodes) {
        const auto oldId = node.nodeId;
        node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        node.name = QStringLiteral("%1%2").arg(node.name, copiedNameSuffix);
        node.position.x += offset;
        node.position.y += offset;
        oldToNewNodeIds.insert(oldId, node.nodeId);
        duplicate.nodes.append(node);
    }

    for (auto edge : sourceSubgraph.edges) {
        if (!oldToNewNodeIds.contains(edge.fromNode) || !oldToNewNodeIds.contains(edge.toNode)) {
            continue;
        }
        edge.edgeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        edge.fromNode = oldToNewNodeIds.value(edge.fromNode);
        edge.toNode = oldToNewNodeIds.value(edge.toNode);
        duplicate.edges.append(edge);
    }

    return duplicate;
}

void WorkflowEditService::appendSubgraph(domain::Workflow& workflow, const domain::Workflow& subgraph)
{
    workflow.nodes.append(subgraph.nodes);
    workflow.edges.append(subgraph.edges);
}

} // namespace vws::application
