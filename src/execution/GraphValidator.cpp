#include "execution/GraphValidator.h"

#include <QHash>
#include <QSet>

namespace vws::execution {

namespace {

enum class VisitState {
    Visiting,
    Visited,
};

QHash<QString, const domain::Node*> buildNodeIndex(const domain::Workflow& workflow)
{
    // Edge validation needs O(1) node lookups instead of repeatedly scanning the node list.
    QHash<QString, const domain::Node*> nodes;
    for (const auto& node : workflow.nodes) {
        if (!node.nodeId.isEmpty() && !nodes.contains(node.nodeId)) {
            nodes.insert(node.nodeId, &node);
        }
    }
    return nodes;
}

QHash<QString, QStringList> buildAdjacency(const domain::Workflow& workflow)
{
    QHash<QString, QStringList> adjacency;
    for (const auto& node : workflow.nodes) {
        adjacency.insert(node.nodeId, QStringList{});
    }
    for (const auto& edge : workflow.edges) {
        adjacency[edge.fromNode].append(edge.toNode);
    }
    return adjacency;
}

bool hasCycleFrom(
    const QString& nodeId,
    const QHash<QString, QStringList>& adjacency,
    QHash<QString, VisitState>& states)
{
    // A node reached again while it is still on the DFS stack means the graph has a cycle.
    states.insert(nodeId, VisitState::Visiting);

    const auto children = adjacency.value(nodeId);
    for (const auto& childId : children) {
        const auto childState = states.find(childId);
        if (childState != states.end() && childState.value() == VisitState::Visiting) {
            return true;
        }
        if (childState == states.end() && hasCycleFrom(childId, adjacency, states)) {
            return true;
        }
    }

    states.insert(nodeId, VisitState::Visited);
    return false;
}

bool containsPort(const QStringList& ports, const QString& port)
{
    return !port.isEmpty() && ports.contains(port);
}

} // namespace

void GraphValidationResult::addError(const QString& error)
{
    valid = false;
    errors.append(error);
}

void GraphValidationResult::addWarning(const QString& warning)
{
    warnings.append(warning);
}

GraphValidationResult GraphValidator::validate(const domain::Workflow& workflow) const
{
    GraphValidationResult result;
    validateNodes(workflow, result);
    validateEdges(workflow, result);

    if (result.valid) {
        validateAcyclic(workflow, result);
    }
    if (result.valid) {
        validateStarterReachability(workflow, result);
    }

    return result;
}

void GraphValidator::validateNodes(const domain::Workflow& workflow, GraphValidationResult& result) const
{
    if (workflow.nodes.isEmpty()) {
        result.addError("Workflow must contain at least one node.");
        return;
    }

    QSet<QString> seenNodeIds;
    bool hasStarter = false;
    for (const auto& node : workflow.nodes) {
        if (node.nodeId.isEmpty()) {
            result.addError("Node id must not be empty.");
            continue;
        }

        if (seenNodeIds.contains(node.nodeId)) {
            result.addError(QString("Duplicate node id: %1").arg(node.nodeId));
        }
        seenNodeIds.insert(node.nodeId);

        if (node.type == "starter") {
            hasStarter = true;
            if (!node.inputPorts.isEmpty()) {
                result.addError(QString("Starter node %1 must not define input ports.").arg(node.nodeId));
            }
            if (!containsPort(node.outputPorts, "output")) {
                result.addError(QString("Starter node %1 must define an output port named output.").arg(node.nodeId));
            }
        }
    }

    if (!hasStarter) {
        result.addError("Workflow must contain at least one Starter node.");
    }
}

void GraphValidator::validateEdges(const domain::Workflow& workflow, GraphValidationResult& result) const
{
    // Edges must reference existing nodes and valid source/target ports.
    const auto nodeIndex = buildNodeIndex(workflow);
    QSet<QString> seenEdgeIds;

    for (const auto& edge : workflow.edges) {
        if (edge.edgeId.isEmpty()) {
            result.addError("Edge id must not be empty.");
        } else if (seenEdgeIds.contains(edge.edgeId)) {
            result.addError(QString("Duplicate edge id: %1").arg(edge.edgeId));
        }
        seenEdgeIds.insert(edge.edgeId);

        const auto fromNode = nodeIndex.value(edge.fromNode, nullptr);
        if (fromNode == nullptr) {
            result.addError(QString("Edge %1 references missing from_node: %2")
                .arg(edge.edgeId, edge.fromNode));
        } else if (!containsPort(fromNode->outputPorts, edge.fromPort)) {
            result.addError(QString("Edge %1 references invalid output port %2 on node %3")
                .arg(edge.edgeId, edge.fromPort, edge.fromNode));
        }

        const auto toNode = nodeIndex.value(edge.toNode, nullptr);
        if (toNode == nullptr) {
            result.addError(QString("Edge %1 references missing to_node: %2")
                .arg(edge.edgeId, edge.toNode));
        } else if (!containsPort(toNode->inputPorts, edge.toPort)) {
            result.addError(QString("Edge %1 references invalid input port %2 on node %3")
                .arg(edge.edgeId, edge.toPort, edge.toNode));
        }
    }
}

void GraphValidator::validateAcyclic(const domain::Workflow& workflow, GraphValidationResult& result) const
{
    // The runtime can only schedule a directed acyclic graph.
    const auto adjacency = buildAdjacency(workflow);
    QHash<QString, VisitState> states;

    for (const auto& node : workflow.nodes) {
        if (!states.contains(node.nodeId) && hasCycleFrom(node.nodeId, adjacency, states)) {
            result.addError("Workflow graph contains a cycle.");
            return;
        }
    }
}

void GraphValidator::validateStarterReachability(const domain::Workflow& workflow, GraphValidationResult& result) const
{
    const auto nodeIndex = buildNodeIndex(workflow);
    const auto adjacency = buildAdjacency(workflow);

    QHash<QString, int> indegree;
    for (const auto& node : workflow.nodes) {
        indegree.insert(node.nodeId, 0);
    }
    for (const auto& edge : workflow.edges) {
        indegree[edge.toNode] += 1;
    }

    QStringList stack;
    for (const auto& node : workflow.nodes) {
        if (node.type == "starter") {
            if (indegree.value(node.nodeId) != 0) {
                result.addError(QString("Starter node %1 must not have incoming edges.").arg(node.nodeId));
            }
            stack.append(node.nodeId);
        } else if (indegree.value(node.nodeId) == 0) {
            result.addError(QString("Node %1 has no incoming edges and is not a Starter node. Every executable path must start from a Starter node.")
                .arg(node.nodeId));
        }
    }

    QSet<QString> reachable;
    while (!stack.isEmpty()) {
        const auto nodeId = stack.takeLast();
        if (reachable.contains(nodeId)) {
            continue;
        }
        reachable.insert(nodeId);
        for (const auto& childId : adjacency.value(nodeId)) {
            stack.append(childId);
        }
    }

    for (const auto& nodeId : nodeIndex.keys()) {
        if (!reachable.contains(nodeId)) {
            result.addError(QString("Node %1 is not reachable from any Starter node.").arg(nodeId));
        }
    }
}

} // namespace vws::execution
