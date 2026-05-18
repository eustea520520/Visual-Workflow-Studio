#include "application/generation/WorkflowSkeletonValidator.h"

#include "application/generation/WorkflowGenerationTemplateCatalog.h"
#include "domain/NodeTypes.h"

#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

namespace vws::application {

namespace {
constexpr int MaxSkeletonNodes = 30;

bool hasCycleFrom(const QString& nodeId, const QHash<QString, QStringList>& adjacency, QSet<QString>& visiting, QSet<QString>& visited)
{
    if (visiting.contains(nodeId)) {
        return true;
    }
    if (visited.contains(nodeId)) {
        return false;
    }
    visiting.insert(nodeId);
    for (const auto& childId : adjacency.value(nodeId)) {
        if (hasCycleFrom(childId, adjacency, visiting, visited)) {
            return true;
        }
    }
    visiting.remove(nodeId);
    visited.insert(nodeId);
    return false;
}
} // namespace

bool WorkflowSkeletonValidator::validateJsonText(
    const QString& jsonText,
    const WorkflowGenerationTemplateCatalog& catalog,
    WorkflowSkeleton& skeleton,
    QStringList& errors) const
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(extractJsonObjectText(jsonText).toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        errors.append(QStringLiteral("Skeleton response was not valid JSON: %1").arg(parseError.errorString()));
        return false;
    }

    skeleton = WorkflowSkeleton::fromJson(document.object());
    return validate(skeleton, catalog, errors);
}

bool WorkflowSkeletonValidator::validate(
    const WorkflowSkeleton& skeleton,
    const WorkflowGenerationTemplateCatalog& catalog,
    QStringList& errors) const
{
    if (skeleton.name.trimmed().isEmpty()) {
        errors.append(QStringLiteral("Skeleton name must not be empty."));
    }
    if (skeleton.nodes.isEmpty()) {
        errors.append(QStringLiteral("Skeleton must contain at least one node."));
    }
    if (skeleton.nodes.size() > MaxSkeletonNodes) {
        errors.append(QStringLiteral("Skeleton contains too many nodes: %1").arg(skeleton.nodes.size()));
    }

    const QRegularExpression idPattern(QStringLiteral("^[a-z0-9_-]+$"));
    QSet<QString> nodeIds;
    QSet<QString> edgeIds;
    QHash<QString, const WorkflowSkeletonNode*> nodesById;
    bool hasStarter = false;

    for (const auto& node : skeleton.nodes) {
        if (!idPattern.match(node.nodeId).hasMatch()) {
            errors.append(QStringLiteral("Invalid node_id: %1").arg(node.nodeId));
        }
        if (nodeIds.contains(node.nodeId)) {
            errors.append(QStringLiteral("Duplicate node_id: %1").arg(node.nodeId));
        }
        nodeIds.insert(node.nodeId);
        nodesById.insert(node.nodeId, &node);

        const auto spec = catalog.fullSpec(node.templateId);
        if (!spec.has_value()) {
            errors.append(QStringLiteral("Unknown template_id %1 on node %2").arg(node.templateId, node.nodeId));
        } else if (spec->type != node.type) {
            errors.append(QStringLiteral("Node %1 type %2 does not match template type %3")
                .arg(node.nodeId, node.type, spec->type));
        }

        if (node.type == domain::NodeTypes::Starter) {
            hasStarter = true;
            if (!node.dependsOnNodeIds.isEmpty()) {
                errors.append(QStringLiteral("Starter node %1 must not depend on other nodes.").arg(node.nodeId));
            }
            if (node.expectedInputDimension != 0) {
                errors.append(QStringLiteral("Starter node %1 expected_input_dimension must be 0.").arg(node.nodeId));
            }
        } else if (node.dependsOnNodeIds.isEmpty()) {
            errors.append(QStringLiteral("Non-starter node %1 must define dependencies.").arg(node.nodeId));
        } else if (node.expectedInputDimension < 1 || node.expectedInputDimension > 12) {
            errors.append(QStringLiteral("Node %1 expected_input_dimension must be between 1 and 12.").arg(node.nodeId));
        }

        if (node.expectedOutputDimension < 1 || node.expectedOutputDimension > 12) {
            errors.append(QStringLiteral("Node %1 expected_output_dimension must be between 1 and 12.").arg(node.nodeId));
        }
        if (!node.inputItems.isEmpty() && node.inputItems.size() != node.expectedInputDimension) {
            errors.append(QStringLiteral("Node %1 input_items size must match expected_input_dimension.").arg(node.nodeId));
        }
        if (!node.outputItems.isEmpty() && node.outputItems.size() != node.expectedOutputDimension) {
            errors.append(QStringLiteral("Node %1 output_items size must match expected_output_dimension.").arg(node.nodeId));
        }

        if (node.layer < 0 || node.layer > 100 || node.row < 0 || node.row > 100) {
            errors.append(QStringLiteral("Node %1 has unreasonable layer/row values.").arg(node.nodeId));
        }
    }

    if (!hasStarter) {
        errors.append(QStringLiteral("Skeleton must contain at least one starter node."));
    }

    QHash<QString, QStringList> adjacency;
    QHash<QString, int> indegree;
    QHash<QString, QStringList> targetSlotWriters;
    QHash<QString, bool> targetPortHasWholeEdge;
    QHash<QString, bool> targetPortHasSlotEdge;
    for (const auto& node : skeleton.nodes) {
        adjacency.insert(node.nodeId, {});
        indegree.insert(node.nodeId, 0);
    }

    for (const auto& edge : skeleton.edges) {
        if (!idPattern.match(edge.edgeId).hasMatch()) {
            errors.append(QStringLiteral("Invalid edge_id: %1").arg(edge.edgeId));
        }
        if (edgeIds.contains(edge.edgeId)) {
            errors.append(QStringLiteral("Duplicate edge_id: %1").arg(edge.edgeId));
        }
        edgeIds.insert(edge.edgeId);

        if (!nodeIds.contains(edge.fromNode)) {
            errors.append(QStringLiteral("Edge %1 references missing from_node %2").arg(edge.edgeId, edge.fromNode));
        }
        if (!nodeIds.contains(edge.toNode)) {
            errors.append(QStringLiteral("Edge %1 references missing to_node %2").arg(edge.edgeId, edge.toNode));
        }
        if (edge.fromPort != "output" || edge.toPort != "input") {
            errors.append(QStringLiteral("Edge %1 must use output -> input ports.").arg(edge.edgeId));
        }
        if (edge.fromSlot < -1 || edge.toSlot < -1) {
            errors.append(QStringLiteral("Edge %1 from_slot/to_slot must be -1 or greater.").arg(edge.edgeId));
        }
        if (const auto* fromNode = nodesById.value(edge.fromNode, nullptr);
            fromNode != nullptr && edge.fromSlot >= fromNode->expectedOutputDimension) {
            errors.append(QStringLiteral("Edge %1 from_slot is outside source output dimension.").arg(edge.edgeId));
        }
        if (const auto* toNode = nodesById.value(edge.toNode, nullptr);
            toNode != nullptr && edge.toSlot >= toNode->expectedInputDimension) {
            errors.append(QStringLiteral("Edge %1 to_slot is outside target input dimension.").arg(edge.edgeId));
        }

        const auto targetPortKey = QStringLiteral("%1:%2").arg(edge.toNode, edge.toPort);
        if (edge.toSlot >= 0) {
            targetPortHasSlotEdge.insert(targetPortKey, true);
            targetSlotWriters[QStringLiteral("%1:%2:%3").arg(edge.toNode, edge.toPort).arg(edge.toSlot)].append(edge.edgeId);
        } else {
            targetPortHasWholeEdge.insert(targetPortKey, true);
        }

        adjacency[edge.fromNode].append(edge.toNode);
        indegree[edge.toNode] = indegree.value(edge.toNode) + 1;
    }

    for (auto it = targetSlotWriters.cbegin(); it != targetSlotWriters.cend(); ++it) {
        if (it.value().size() > 1) {
            errors.append(QStringLiteral("Input slot %1 is written by multiple edges: %2.")
                .arg(it.key(), it.value().join(", ")));
        }
    }
    for (auto it = targetPortHasSlotEdge.cbegin(); it != targetPortHasSlotEdge.cend(); ++it) {
        if (it.value() && targetPortHasWholeEdge.value(it.key())) {
            errors.append(QStringLiteral("Input port %1 mixes whole-port and slot-level edges.").arg(it.key()));
        }
    }

    for (const auto& node : skeleton.nodes) {
        for (const auto& dependency : node.dependsOnNodeIds) {
            if (!nodeIds.contains(dependency)) {
                errors.append(QStringLiteral("Node %1 depends on missing node %2").arg(node.nodeId, dependency));
            }
        }
        if (node.type != domain::NodeTypes::Starter && indegree.value(node.nodeId) == 0) {
            errors.append(QStringLiteral("Node %1 has no incoming edge.").arg(node.nodeId));
        }
    }

    QSet<QString> visiting;
    QSet<QString> visited;
    for (const auto& node : skeleton.nodes) {
        if (hasCycleFrom(node.nodeId, adjacency, visiting, visited)) {
            errors.append(QStringLiteral("Skeleton graph contains a cycle."));
            break;
        }
    }

    QStringList stack;
    for (const auto& node : skeleton.nodes) {
        if (node.type == domain::NodeTypes::Starter) {
            stack.append(node.nodeId);
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
    for (const auto& node : skeleton.nodes) {
        if (!reachable.contains(node.nodeId)) {
            errors.append(QStringLiteral("Node %1 is not reachable from a starter node.").arg(node.nodeId));
        }
    }

    return errors.isEmpty();
}

QString WorkflowSkeletonValidator::extractJsonObjectText(const QString& text) const
{
    const auto trimmed = text.trimmed();
    if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
        return trimmed;
    }
    const auto start = trimmed.indexOf('{');
    const auto end = trimmed.lastIndexOf('}');
    return start >= 0 && end > start ? trimmed.mid(start, end - start + 1) : QString();
}

} // namespace vws::application
