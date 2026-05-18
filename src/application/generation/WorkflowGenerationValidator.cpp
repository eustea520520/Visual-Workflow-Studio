#include "application/generation/WorkflowGenerationValidator.h"

#include "domain/NodeConfigKeys.h"
#include "domain/NodeTypes.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

namespace vws::application {

namespace {

bool containsPort(const QStringList& ports, const QString& port)
{
    return !port.trimmed().isEmpty() && ports.contains(port);
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

QHash<QString, const domain::Node*> nodeIndex(const domain::Workflow& workflow)
{
    QHash<QString, const domain::Node*> index;
    for (const auto& node : workflow.nodes) {
        if (!node.nodeId.isEmpty() && !index.contains(node.nodeId)) {
            index.insert(node.nodeId, &node);
        }
    }
    return index;
}

bool jsonContainsSecretLikeKey(const QJsonValue& value)
{
    if (value.isObject()) {
        const auto object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            const auto key = it.key().toLower();
            if (key.contains("api_key") || key.contains("apikey") || key.contains("password") || key.contains("secret")) {
                const auto text = it.value().toVariant().toString().trimmed();
                if (!text.isEmpty()) {
                    return true;
                }
            }
            if (jsonContainsSecretLikeKey(it.value())) {
                return true;
            }
        }
    } else if (value.isArray()) {
        for (const auto& child : value.toArray()) {
            if (jsonContainsSecretLikeKey(child)) {
                return true;
            }
        }
    }
    return false;
}

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

WorkflowGenerationValidationResult WorkflowGenerationValidator::validateJsonText(const QString& jsonText) const
{
    WorkflowGenerationValidationResult result;
    const auto extracted = extractJsonObjectText(jsonText);
    if (extracted.trimmed().isEmpty()) {
        result.addError(QStringLiteral("LLM response was not valid JSON."));
        return result;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(extracted.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.addError(QStringLiteral("LLM response was not valid JSON: %1").arg(parseError.errorString()));
        return result;
    }

    result.json = document.object();
    validateSecrets(result.json, result);
    const auto workflow = domain::Workflow::fromJson(result.json);
    auto workflowResult = validateWorkflow(workflow);
    result.workflow = workflow;
    result.valid = result.valid && workflowResult.valid;
    result.errors.append(workflowResult.errors);
    result.warnings.append(workflowResult.warnings);
    return result;
}

WorkflowGenerationValidationResult WorkflowGenerationValidator::validateWorkflow(const domain::Workflow& workflow) const
{
    WorkflowGenerationValidationResult result;
    result.workflow = workflow;
    validateNodes(workflow, result);
    validateEdges(workflow, result);
    if (result.valid) {
        validateGraph(workflow, result);
    }
    validatePythonCode(workflow, result);
    return result;
}

QString WorkflowGenerationValidator::extractJsonObjectText(const QString& text) const
{
    const auto trimmed = text.trimmed();
    if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
        return trimmed;
    }

    const auto start = trimmed.indexOf('{');
    const auto end = trimmed.lastIndexOf('}');
    if (start >= 0 && end > start) {
        return trimmed.mid(start, end - start + 1);
    }
    return {};
}

void WorkflowGenerationValidator::validateNodes(const domain::Workflow& workflow, WorkflowGenerationValidationResult& result) const
{
    if (workflow.nodes.isEmpty()) {
        result.addError(QStringLiteral("Workflow must contain at least one node."));
        return;
    }

    const QRegularExpression idPattern(QStringLiteral("^[a-z0-9_-]+$"));
    QSet<QString> seenNodeIds;
    bool hasStarter = false;
    for (const auto& node : workflow.nodes) {
        if (node.nodeId.trimmed().isEmpty()) {
            result.addError(QStringLiteral("Node id must not be empty."));
            continue;
        }
        if (!idPattern.match(node.nodeId).hasMatch()) {
            result.addError(QStringLiteral("Node id contains invalid characters: %1").arg(node.nodeId));
        }
        if (seenNodeIds.contains(node.nodeId)) {
            result.addError(QStringLiteral("Duplicate node id: %1").arg(node.nodeId));
        }
        seenNodeIds.insert(node.nodeId);

        const auto type = node.type.trimmed().toLower();
        if (type != domain::NodeTypes::Starter && type != domain::NodeTypes::Function && type != domain::NodeTypes::Agent) {
            result.addError(QStringLiteral("Node %1 has invalid type: %2").arg(node.nodeId, node.type));
        }

        if (type == domain::NodeTypes::Starter) {
            hasStarter = true;
            if (!node.inputPorts.isEmpty()) {
                result.addError(QStringLiteral("Starter node %1 must not define input ports.").arg(node.nodeId));
            }
            if (!containsPort(node.outputPorts, QStringLiteral("output"))) {
                result.addError(QStringLiteral("Starter node %1 must define output port output.").arg(node.nodeId));
            }
        } else {
            if (!containsPort(node.inputPorts, QStringLiteral("input"))) {
                result.addError(QStringLiteral("Node %1 must define input port input.").arg(node.nodeId));
            }
            if (!containsPort(node.outputPorts, QStringLiteral("output"))) {
                result.addError(QStringLiteral("Node %1 must define output port output.").arg(node.nodeId));
            }
        }
    }

    if (!hasStarter) {
        result.addError(QStringLiteral("Workflow must contain at least one Starter node."));
    }
}

void WorkflowGenerationValidator::validateEdges(const domain::Workflow& workflow, WorkflowGenerationValidationResult& result) const
{
    const auto index = nodeIndex(workflow);
    QSet<QString> seenEdgeIds;
    QHash<QString, QStringList> targetSlotWriters;
    QHash<QString, bool> targetPortHasWholeEdge;
    QHash<QString, bool> targetPortHasSlotEdge;
    const QRegularExpression idPattern(QStringLiteral("^[a-z0-9_-]+$"));
    for (const auto& edge : workflow.edges) {
        if (edge.edgeId.trimmed().isEmpty()) {
            result.addError(QStringLiteral("Edge id must not be empty."));
        } else if (!idPattern.match(edge.edgeId).hasMatch()) {
            result.addError(QStringLiteral("Edge id contains invalid characters: %1").arg(edge.edgeId));
        } else if (seenEdgeIds.contains(edge.edgeId)) {
            result.addError(QStringLiteral("Duplicate edge id: %1").arg(edge.edgeId));
        }
        seenEdgeIds.insert(edge.edgeId);

        const auto fromNode = index.value(edge.fromNode, nullptr);
        const auto toNode = index.value(edge.toNode, nullptr);
        if (fromNode == nullptr) {
            result.addError(QStringLiteral("Edge %1 references missing from_node: %2").arg(edge.edgeId, edge.fromNode));
        } else if (!containsPort(fromNode->outputPorts, edge.fromPort)) {
            result.addError(QStringLiteral("Edge %1 references invalid source port: %2").arg(edge.edgeId, edge.fromPort));
        } else if (edge.fromSlot < -1) {
            result.addError(QStringLiteral("Edge %1 from_slot must be -1 or greater.").arg(edge.edgeId));
        } else if (edge.fromSlot >= portDimension(fromNode->ioSpec.outputs, edge.fromPort)) {
            result.addError(QStringLiteral("Edge %1 from_slot is outside source output dimension.").arg(edge.edgeId));
        }
        if (toNode == nullptr) {
            result.addError(QStringLiteral("Edge %1 references missing to_node: %2").arg(edge.edgeId, edge.toNode));
        } else if (!containsPort(toNode->inputPorts, edge.toPort)) {
            result.addError(QStringLiteral("Edge %1 references invalid target port: %2").arg(edge.edgeId, edge.toPort));
        } else if (edge.toSlot < -1) {
            result.addError(QStringLiteral("Edge %1 to_slot must be -1 or greater.").arg(edge.edgeId));
        } else if (edge.toSlot >= portDimension(toNode->ioSpec.inputs, edge.toPort)) {
            result.addError(QStringLiteral("Edge %1 to_slot is outside target input dimension.").arg(edge.edgeId));
        }

        const auto targetPortKey = QStringLiteral("%1:%2").arg(edge.toNode, edge.toPort);
        if (edge.toSlot >= 0) {
            targetPortHasSlotEdge.insert(targetPortKey, true);
            targetSlotWriters[QStringLiteral("%1:%2:%3").arg(edge.toNode, edge.toPort).arg(edge.toSlot)].append(edge.edgeId);
        } else {
            targetPortHasWholeEdge.insert(targetPortKey, true);
        }
    }

    for (auto it = targetSlotWriters.cbegin(); it != targetSlotWriters.cend(); ++it) {
        if (it.value().size() > 1) {
            result.addError(QStringLiteral("Input slot %1 is written by multiple edges: %2.")
                .arg(it.key(), it.value().join(", ")));
        }
    }
    for (auto it = targetPortHasSlotEdge.cbegin(); it != targetPortHasSlotEdge.cend(); ++it) {
        if (it.value() && targetPortHasWholeEdge.value(it.key())) {
            result.addError(QStringLiteral("Input port %1 mixes whole-port and slot-level edges.").arg(it.key()));
        }
    }
}

void WorkflowGenerationValidator::validateGraph(const domain::Workflow& workflow, WorkflowGenerationValidationResult& result) const
{
    QHash<QString, QStringList> adjacency;
    QHash<QString, int> indegree;
    for (const auto& node : workflow.nodes) {
        adjacency.insert(node.nodeId, {});
        indegree.insert(node.nodeId, 0);
    }
    for (const auto& edge : workflow.edges) {
        adjacency[edge.fromNode].append(edge.toNode);
        indegree[edge.toNode] = indegree.value(edge.toNode) + 1;
    }

    QSet<QString> visiting;
    QSet<QString> visited;
    for (const auto& node : workflow.nodes) {
        if (hasCycleFrom(node.nodeId, adjacency, visiting, visited)) {
            result.addError(QStringLiteral("Workflow graph contains a cycle."));
            return;
        }
    }

    QStringList stack;
    for (const auto& node : workflow.nodes) {
        if (node.type == domain::NodeTypes::Starter) {
            if (indegree.value(node.nodeId) != 0) {
                result.addError(QStringLiteral("Starter node %1 must not have incoming edges.").arg(node.nodeId));
            }
            stack.append(node.nodeId);
        } else if (indegree.value(node.nodeId) == 0) {
            result.addError(QStringLiteral("Node %1 has no incoming edges and is not a Starter node.").arg(node.nodeId));
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
    for (const auto& node : workflow.nodes) {
        if (!reachable.contains(node.nodeId)) {
            result.addError(QStringLiteral("Node %1 is not reachable from any Starter node.").arg(node.nodeId));
        }
    }
}

void WorkflowGenerationValidator::validatePythonCode(const domain::Workflow& workflow, WorkflowGenerationValidationResult& result) const
{
    for (const auto& node : workflow.nodes) {
        const auto code = node.config.value(domain::NodeConfigKeys::Code).toString();
        if (!code.contains("def run(")) {
            result.addError(QStringLiteral("Node %1 Python code must define def run(inputs, context):").arg(node.nodeId));
        }
        if (!code.contains("\"outputs\"") && !code.contains("'outputs'")) {
            result.addError(QStringLiteral("Node %1 Python code must return an outputs object.").arg(node.nodeId));
        }
    }
}

void WorkflowGenerationValidator::validateSecrets(const QJsonObject& json, WorkflowGenerationValidationResult& result) const
{
    if (jsonContainsSecretLikeKey(json)) {
        result.addError(QStringLiteral("Generated workflow contains unsafe secret-like values."));
    }
}

} // namespace vws::application
