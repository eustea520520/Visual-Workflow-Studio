#include "execution/GraphValidator.h"

#include "domain/NodeTypes.h"
#include "domain/NodeConfigKeys.h"

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

bool isStarterNode(const domain::Node& node)
{
    return node.type.trimmed().toLower() == domain::NodeTypes::Starter;
}

bool isZeroInputSubsystemNode(const domain::Node& node)
{
    return node.type.trimmed().toLower() == domain::NodeTypes::Subsystem
        && node.inputPorts.isEmpty();
}

bool isTopLevelEntryNode(const domain::Node& node)
{
    return isStarterNode(node) || isZeroInputSubsystemNode(node);
}

bool isLoopNode(const domain::Node& node)
{
    return node.type.trimmed().toLower() == domain::NodeTypes::Loop;
}

int portDimension(const QList<domain::PortDimensionSpec>& specs, const QString& portName)
{
    for (const auto& spec : specs) {
        if (spec.portName == portName) {
            return qBound(1, spec.dimension, 32);
        }
    }
    return 1;
}

int inputPortDimension(const domain::Node& node, const QString& portName)
{
    return portDimension(node.ioSpec.inputs, portName);
}

int outputPortDimension(const domain::Node& node, const QString& portName)
{
    return portDimension(node.ioSpec.outputs, portName);
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

GraphValidationResult GraphValidator::validate(const domain::Workflow& workflow, GraphValidationMode mode) const
{
    GraphValidationResult result;
    validateNodes(workflow, mode, result);
    validateEdges(workflow, result);

    if (result.valid) {
        validateAcyclic(workflow, result);
    }
    if (result.valid) {
        validateLoopNodes(workflow, result);
    }
    if (result.valid) {
        validateStarterReachability(workflow, mode, result);
    }

    return result;
}

void GraphValidator::validateNodes(const domain::Workflow& workflow, GraphValidationMode mode, GraphValidationResult& result) const
{
    if (workflow.nodes.isEmpty()) {
        result.addError("Workflow must contain at least one node.");
        return;
    }

    QSet<QString> seenNodeIds;
    bool hasTopLevelEntry = false;
    for (const auto& node : workflow.nodes) {
        if (node.nodeId.isEmpty()) {
            result.addError("Node id must not be empty.");
            continue;
        }

        if (seenNodeIds.contains(node.nodeId)) {
            result.addError(QString("Duplicate node id: %1").arg(node.nodeId));
        }
        seenNodeIds.insert(node.nodeId);

        if (isTopLevelEntryNode(node)) {
            hasTopLevelEntry = true;
        }

        if (isStarterNode(node)) {
            if (!node.inputPorts.isEmpty()) {
                result.addError(QString("Starter node %1 must not define input ports.").arg(node.nodeId));
            }
            if (!containsPort(node.outputPorts, "output")) {
                result.addError(QString("Starter node %1 must define an output port named output.").arg(node.nodeId));
            }
        } else if (isLoopNode(node)) {
            if (!containsPort(node.inputPorts, "input")) {
                result.addError(QString("Loop node %1 must define an input port named input.").arg(node.nodeId));
            }
            if (!containsPort(node.outputPorts, "output")) {
                result.addError(QString("Loop node %1 must define an output port named output.").arg(node.nodeId));
            }
            if (!node.config.contains(domain::NodeConfigKeys::LoopIterations)
                || node.config.value(domain::NodeConfigKeys::LoopIterations).toInt(0) < 1) {
                result.addError(QString("Loop node %1 must define a positive loop_iterations.").arg(node.nodeId));
            }
        }
    }

    if (mode == GraphValidationMode::TopLevelWorkflow && !hasTopLevelEntry) {
        result.addError("Workflow must contain at least one Starter node or a zero-input Subsystem node.");
    }
}

void GraphValidator::validateLoopNodes(const domain::Workflow& workflow, GraphValidationResult& result) const
{
    const auto nodeIndex = buildNodeIndex(workflow);
    QHash<QString, QList<domain::Edge>> outgoing;
    QHash<QString, QList<domain::Edge>> incoming;
    for (const auto& edge : workflow.edges) {
        outgoing[edge.fromNode].append(edge);
        incoming[edge.toNode].append(edge);
    }

    for (const auto& node : workflow.nodes) {
        if (!isLoopNode(node)) {
            continue;
        }

        const auto loopOutgoing = outgoing.value(node.nodeId);
        QSet<QString> bodyNodeIds;
        for (const auto& edge : loopOutgoing) {
            bodyNodeIds.insert(edge.toNode);
        }
        if (bodyNodeIds.size() != 1) {
            result.addError(QString("Loop node %1 must connect to exactly one direct body node.").arg(node.nodeId));
            continue;
        }

        const auto bodyNodeId = *bodyNodeIds.constBegin();
        const auto* bodyNode = nodeIndex.value(bodyNodeId, nullptr);
        if (bodyNode == nullptr) {
            continue;
        }
        if (isLoopNode(*bodyNode)) {
            result.addError(QString("Loop node %1 cannot use another Loop node as its direct body node.").arg(node.nodeId));
        }

        for (const auto& edge : incoming.value(bodyNodeId)) {
            if (edge.fromNode != node.nodeId) {
                result.addError(QString("Loop body node %1 cannot receive input from node %2; it must only receive input from Loop node %3.")
                    .arg(bodyNodeId, edge.fromNode, node.nodeId));
            }
        }
    }
}

void GraphValidator::validateEdges(const domain::Workflow& workflow, GraphValidationResult& result) const
{
    // Edges must reference existing nodes and valid source/target ports.
    const auto nodeIndex = buildNodeIndex(workflow);
    QSet<QString> seenEdgeIds;
    QHash<QString, QStringList> targetSlotWriters;

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
        } else if (edge.fromSlot < 0) {
            result.addError(QString("Edge %1 from_slot must be 0 or greater.").arg(edge.edgeId));
        } else if (edge.fromSlot >= outputPortDimension(*fromNode, edge.fromPort)) {
            result.addError(QString("Edge %1 references output slot %2 on node %3, but output port %4 has dimension %5.")
                .arg(edge.edgeId)
                .arg(edge.fromSlot)
                .arg(edge.fromNode, edge.fromPort)
                .arg(outputPortDimension(*fromNode, edge.fromPort)));
        }

        const auto toNode = nodeIndex.value(edge.toNode, nullptr);
        if (toNode == nullptr) {
            result.addError(QString("Edge %1 references missing to_node: %2")
                .arg(edge.edgeId, edge.toNode));
        } else if (!containsPort(toNode->inputPorts, edge.toPort)) {
            result.addError(QString("Edge %1 references invalid input port %2 on node %3")
                .arg(edge.edgeId, edge.toPort, edge.toNode));
        } else if (edge.toSlot < 0) {
            result.addError(QString("Edge %1 to_slot must be 0 or greater.").arg(edge.edgeId));
        } else if (edge.toSlot >= inputPortDimension(*toNode, edge.toPort)) {
            result.addError(QString("Edge %1 references input slot %2 on node %3, but input port %4 has dimension %5.")
                .arg(edge.edgeId)
                .arg(edge.toSlot)
                .arg(edge.toNode, edge.toPort)
                .arg(inputPortDimension(*toNode, edge.toPort)));
        }

        if (edge.toSlot >= 0) {
            const auto targetSlotKey = QStringLiteral("%1:%2:%3").arg(edge.toNode, edge.toPort).arg(edge.toSlot);
            targetSlotWriters[targetSlotKey].append(edge.edgeId);
        }
    }

    for (auto it = targetSlotWriters.cbegin(); it != targetSlotWriters.cend(); ++it) {
        if (it.value().size() > 1) {
            result.addError(QString("Input slot %1 is written by multiple edges: %2.")
                .arg(it.key(), it.value().join(", ")));
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

void GraphValidator::validateStarterReachability(const domain::Workflow& workflow, GraphValidationMode mode, GraphValidationResult& result) const
{
    if (mode == GraphValidationMode::SubsystemWorkflow) {
        return;
    }

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
        if (isStarterNode(node)) {
            if (indegree.value(node.nodeId) != 0) {
                result.addError(QString("Starter node %1 must not have incoming edges.").arg(node.nodeId));
            }
            stack.append(node.nodeId);
        } else if (isZeroInputSubsystemNode(node)) {
            stack.append(node.nodeId);
        } else if (indegree.value(node.nodeId) == 0) {
            result.addError(QString("Node %1 has no incoming edges and is not a Starter or zero-input Subsystem node. Every executable path must start from a Starter node or a zero-input Subsystem node.")
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
