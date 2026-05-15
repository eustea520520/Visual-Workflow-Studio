#include "application/WorkflowEditService.h"

#include <QHash>
#include <QUuid>

namespace vws::application {

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

    domain::Edge edge;
    edge.edgeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    edge.fromNode = sourceNode.nodeId;
    edge.fromPort = sourceNode.outputPorts.first();
    edge.toNode = targetNode.nodeId;
    edge.toPort = targetNode.inputPorts.first();

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
