#include "application/generation/WorkflowGenerationNormalizer.h"

#include "application/generation/WorkflowAutoLayout.h"
#include "application/PythonCodeTemplates.h"
#include "domain/NodeConfigKeys.h"
#include "domain/NodeTypes.h"

#include <QDateTime>
#include <QSet>
#include <QUuid>

namespace vws::application {

namespace {
int normalizeRotation(int degrees)
{
    int normalized = degrees % 360;
    if (normalized < 0) {
        normalized += 360;
    }
    return (((normalized + 45) / 90) * 90) % 360;
}
}

WorkflowGenerationNormalizer::WorkflowGenerationNormalizer(WorkflowAutoLayout& autoLayout)
    : m_autoLayout(autoLayout)
{
}

domain::Workflow WorkflowGenerationNormalizer::normalize(const domain::Workflow& workflow, const domain::Workspace& workspace) const
{
    auto normalized = workflow;
    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    normalized.schemaVersion = 1;
    normalized.workflowId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    normalized.workspaceId = workspace.id;
    normalized.name = normalized.name.trimmed().isEmpty() ? QStringLiteral("Generated Workflow") : normalized.name.trimmed();
    normalized.createdAt = now;
    normalized.updatedAt = now;
    normalized.version = qMax(1, normalized.version);

    QSet<QString> seenNodeIds;
    int nodeIndex = 1;
    for (auto& node : normalized.nodes) {
        if (node.nodeId.trimmed().isEmpty() || seenNodeIds.contains(node.nodeId)) {
            node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        seenNodeIds.insert(node.nodeId);
        node.type = node.type.trimmed().toLower();
        if (node.type != domain::NodeTypes::Starter && node.type != domain::NodeTypes::Agent) {
            node.type = domain::NodeTypes::Function;
        }
        node.name = node.name.trimmed().isEmpty()
            ? QStringLiteral("%1 Node %2").arg(node.type, QString::number(nodeIndex))
            : node.name.trimmed();
        node.rotationDegrees = normalizeRotation(node.rotationDegrees);
        if (node.type == domain::NodeTypes::Starter) {
            node.inputPorts.clear();
            node.outputPorts = {"output"};
        } else {
            node.inputPorts = {"input"};
            node.outputPorts = {"output"};
        }
        node.config.insert(domain::NodeConfigKeys::Language, QStringLiteral("python"));
        node.config.insert(domain::NodeConfigKeys::Entry, QStringLiteral("run"));
        if (node.config.value(domain::NodeConfigKeys::Code).toString().trimmed().isEmpty()) {
            node.config.insert(domain::NodeConfigKeys::Code, PythonCodeTemplates::defaultCodeForNodeType(node.type));
        }
        if (node.runtime.timeoutMs <= 0) {
            node.runtime.timeoutMs = 300000;
        }
        if (node.runtime.maxMemoryMb <= 0) {
            node.runtime.maxMemoryMb = 1024;
        }
        if (node.runtime.concurrencyGroup.trimmed().isEmpty()) {
            node.runtime.concurrencyGroup = QStringLiteral("default");
        }
        ++nodeIndex;
    }

    QSet<QString> seenEdgeIds;
    for (auto& edge : normalized.edges) {
        if (edge.edgeId.trimmed().isEmpty() || seenEdgeIds.contains(edge.edgeId)) {
            edge.edgeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        seenEdgeIds.insert(edge.edgeId);
        edge.fromPort = QStringLiteral("output");
        edge.toPort = QStringLiteral("input");
        if (edge.fromSlot < -1) {
            edge.fromSlot = -1;
        }
        if (edge.toSlot < -1) {
            edge.toSlot = -1;
        }
    }

    m_autoLayout.applyIfNeeded(normalized);
    return normalized;
}

} // namespace vws::application
