#include "application/generation/WorkflowGenerationAssembler.h"

#include "application/generation/WorkflowGenerationTemplateCatalog.h"
#include "application/io/NodeIoSpecUtils.h"
#include "domain/NodeConfigKeys.h"

#include <QDateTime>
#include <QSet>
#include <QUuid>

namespace vws::application {

namespace ConfigKeys = domain::NodeConfigKeys;

bool WorkflowGenerationAssembler::assemble(
    const WorkflowSkeleton& skeleton,
    const QHash<QString, NodeImplementation>& implementationsByNodeId,
    const WorkflowGenerationTemplateCatalog& catalog,
    const domain::Workspace& workspace,
    domain::Workflow& workflow,
    QStringList& errors) const
{
    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    workflow = {};
    workflow.schemaVersion = 1;
    workflow.workflowId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    workflow.workspaceId = workspace.id;
    workflow.name = skeleton.name.trimmed().isEmpty() ? QStringLiteral("Generated Workflow") : skeleton.name.trimmed();
    workflow.description = skeleton.description;
    workflow.createdAt = now;
    workflow.updatedAt = now;
    workflow.version = 1;

    QSet<QString> occupiedSlots;
    for (const auto& skeletonNode : skeleton.nodes) {
        const auto spec = catalog.fullSpec(skeletonNode.templateId);
        if (!spec.has_value()) {
            errors.append(QStringLiteral("Cannot assemble node %1: unknown template %2.")
                .arg(skeletonNode.nodeId, skeletonNode.templateId));
            continue;
        }
        if (!implementationsByNodeId.contains(skeletonNode.nodeId)) {
            errors.append(QStringLiteral("Cannot assemble node %1: implementation is missing.").arg(skeletonNode.nodeId));
            continue;
        }

        const auto implementation = implementationsByNodeId.value(skeletonNode.nodeId);
        auto config = spec->defaultConfig;
        for (auto it = implementation.configPatch.constBegin(); it != implementation.configPatch.constEnd(); ++it) {
            config.insert(it.key(), it.value());
        }
        config.insert(ConfigKeys::Language, QStringLiteral("python"));
        config.insert(ConfigKeys::Entry, QStringLiteral("run"));
        config.insert(ConfigKeys::IoTemplate, spec->ioKind);
        config.insert(ConfigKeys::Code, implementation.code);

        domain::Node node;
        node.nodeId = skeletonNode.nodeId;
        node.templateId = skeletonNode.templateId;
        node.type = spec->type;
        node.name = skeletonNode.name.trimmed().isEmpty() ? spec->displayName : skeletonNode.name.trimmed();
        node.description = skeletonNode.purpose;
        node.position.x = 80.0 + skeletonNode.layer * 320.0;
        node.position.y = 100.0 + skeletonNode.row * 180.0;
        auto slot = QStringLiteral("%1:%2").arg(skeletonNode.layer).arg(skeletonNode.row);
        while (occupiedSlots.contains(slot)) {
            node.position.y += 180.0;
            slot = QStringLiteral("%1:%2").arg(skeletonNode.layer).arg(qRound((node.position.y - 100.0) / 180.0));
        }
        occupiedSlots.insert(slot);
        node.rotationDegrees = 0;
        node.inputPorts = spec->inputPorts;
        node.outputPorts = spec->outputPorts;
        domain::NodeIoSpec skeletonIoSpec;
        if (!node.inputPorts.isEmpty() && skeletonNode.expectedInputDimension > 0) {
            skeletonIoSpec.inputs.append(NodeIoSpecUtils::makePortSpec(
                QStringLiteral("input"),
                skeletonNode.expectedInputDimension,
                QStringLiteral("llm-skeleton"),
                skeletonNode.inputItems));
        }
        skeletonIoSpec.outputs.append(NodeIoSpecUtils::makePortSpec(
            QStringLiteral("output"),
            skeletonNode.expectedOutputDimension,
            QStringLiteral("llm-skeleton"),
            skeletonNode.outputItems));
        node.ioSpec = NodeIoSpecUtils::merged(
            NodeIoSpecUtils::merged(spec->defaultIoSpec, skeletonIoSpec),
            implementation.ioSpecPatch);
        node.config = config;
        node.runtime = spec->defaultRuntime;
        node.runtime.timeoutMs = implementation.timeoutMs > 0 ? implementation.timeoutMs : spec->defaultRuntime.timeoutMs;
        workflow.nodes.append(node);
    }

    for (const auto& skeletonEdge : skeleton.edges) {
        domain::Edge edge;
        edge.edgeId = skeletonEdge.edgeId.trimmed().isEmpty()
            ? QUuid::createUuid().toString(QUuid::WithoutBraces)
            : skeletonEdge.edgeId;
        edge.fromNode = skeletonEdge.fromNode;
        edge.fromPort = skeletonEdge.fromPort.trimmed().isEmpty() ? QStringLiteral("output") : skeletonEdge.fromPort;
        edge.fromSlot = skeletonEdge.fromSlot;
        edge.toNode = skeletonEdge.toNode;
        edge.toPort = skeletonEdge.toPort.trimmed().isEmpty() ? QStringLiteral("input") : skeletonEdge.toPort;
        edge.toSlot = skeletonEdge.toSlot;
        workflow.edges.append(edge);
    }

    return errors.isEmpty();
}

} // namespace vws::application
