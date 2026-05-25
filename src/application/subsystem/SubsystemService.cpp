#include "application/subsystem/SubsystemService.h"

#include "domain/NodeConfigKeys.h"
#include "domain/NodeTypes.h"
#include "domain/WorkflowJsonParser.h"
#include "domain/WorkflowSchema.h"

#include <QDateTime>
#include <QUuid>

namespace vws::application {

namespace ConfigKeys = domain::NodeConfigKeys;
namespace NodeTypes = domain::NodeTypes;

namespace {

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

domain::PortDimensionSpec toPortSpec(const SubsystemBoundaryPort& boundaryPort, bool input)
{
    domain::PortDimensionSpec spec;
    spec.portName = boundaryPort.externalPort;
    spec.dimension = qBound(1, boundaryPort.dimension, 32);
    spec.source = QStringLiteral("subsystem-boundary");
    spec.description = boundaryPort.displayName.trimmed().isEmpty()
        ? QStringLiteral("Mapped from internal node %1 port %2")
            .arg(boundaryPort.internalNodeName, boundaryPort.internalPort)
        : boundaryPort.displayName.trimmed();
    spec.itemLabels = boundaryPort.itemLabels;
    if (spec.itemLabels.isEmpty()) {
        for (int index = 0; index < spec.dimension; ++index) {
            spec.itemLabels.append(QString::number(index + 1));
        }
    }
    Q_UNUSED(input);
    return spec;
}

} // namespace

bool SubsystemService::isSubsystemNode(const domain::Node& node) const
{
    return node.type.trimmed().toLower() == NodeTypes::Subsystem;
}

domain::Node SubsystemService::createSubsystemNode(
    const QString& workspaceId,
    const QString& name,
    const domain::NodePosition& position) const
{
    domain::Node node;
    node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    node.type = NodeTypes::Subsystem;
    node.name = name.trimmed().isEmpty() ? QStringLiteral("Subsystem") : name.trimmed();
    node.description = QStringLiteral("Nested workflow node.");
    node.position = position;
    node.config.insert(ConfigKeys::SubsystemSchemaVersion, domain::CurrentWorkflowSchemaVersion);
    node.config.insert(ConfigKeys::SubsystemWorkflow,
        createEmbeddedWorkflow(workspaceId, node.nodeId, node.name).toJson());
    node.config.insert(ConfigKeys::SubsystemBoundary, SubsystemBoundary{}.toJson());
    return node;
}

bool SubsystemService::loadSubsystemWorkflow(
    const domain::Node& subsystemNode,
    domain::Workflow& subWorkflow,
    QString* errorMessage) const
{
    if (!isSubsystemNode(subsystemNode)) {
        setError(errorMessage, QStringLiteral("Node is not a subsystem node."));
        return false;
    }

    const auto workflowObject = subsystemNode.config.value(ConfigKeys::SubsystemWorkflow).toObject();
    if (workflowObject.isEmpty()) {
        setError(errorMessage, QStringLiteral("Subsystem node does not contain an embedded workflow."));
        return false;
    }

    const auto parseResult = domain::WorkflowJsonParser::parseStrict(workflowObject);
    if (!parseResult.success) {
        setError(errorMessage, parseResult.errors.join(QStringLiteral("\n")));
        return false;
    }
    subWorkflow = parseResult.workflow;
    return true;
}

bool SubsystemService::saveSubsystemWorkflow(
    domain::Node& subsystemNode,
    const domain::Workflow& subWorkflow,
    QString* errorMessage) const
{
    if (!isSubsystemNode(subsystemNode)) {
        setError(errorMessage, QStringLiteral("Node is not a subsystem node."));
        return false;
    }

    subsystemNode.config.insert(ConfigKeys::SubsystemSchemaVersion, domain::CurrentWorkflowSchemaVersion);
    subsystemNode.config.insert(ConfigKeys::SubsystemWorkflow, subWorkflow.toJson());
    return refreshSubsystemBoundary(subsystemNode, nullptr, errorMessage);
}

bool SubsystemService::refreshSubsystemBoundary(
    domain::Node& subsystemNode,
    QStringList* warnings,
    QString* errorMessage) const
{
    domain::Workflow subWorkflow;
    if (!loadSubsystemWorkflow(subsystemNode, subWorkflow, errorMessage)) {
        return false;
    }

    const auto previousBoundary = SubsystemBoundary::fromJson(
        subsystemNode.config.value(ConfigKeys::SubsystemBoundary).toObject());
    const auto boundary = m_boundaryInferer.infer(subWorkflow, previousBoundary, warnings);
    applyBoundary(subsystemNode, boundary);
    subsystemNode.config.insert(ConfigKeys::SubsystemBoundary, boundary.toJson());
    return true;
}

QString SubsystemService::breadcrumbLabel(const domain::Node& subsystemNode) const
{
    return subsystemNode.name.trimmed().isEmpty() ? subsystemNode.nodeId : subsystemNode.name.trimmed();
}

domain::Workflow SubsystemService::createEmbeddedWorkflow(
    const QString& workspaceId,
    const QString& nodeId,
    const QString& name)
{
    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    domain::Workflow workflow;
    workflow.schemaVersion = domain::CurrentWorkflowSchemaVersion;
    workflow.workflowId = QStringLiteral("%1__subworkflow").arg(nodeId);
    workflow.workspaceId = workspaceId;
    workflow.name = name;
    workflow.description = QStringLiteral("Internal workflow for %1").arg(name);
    workflow.createdAt = now;
    workflow.updatedAt = now;
    workflow.version = 1;
    return workflow;
}

void SubsystemService::applyBoundary(domain::Node& subsystemNode, const SubsystemBoundary& boundary)
{
    subsystemNode.inputPorts.clear();
    subsystemNode.outputPorts.clear();
    subsystemNode.ioSpec.inputs.clear();
    subsystemNode.ioSpec.outputs.clear();

    for (const auto& port : boundary.inputs) {
        subsystemNode.inputPorts.append(port.externalPort);
        subsystemNode.ioSpec.inputs.append(toPortSpec(port, true));
    }
    for (const auto& port : boundary.outputs) {
        subsystemNode.outputPorts.append(port.externalPort);
        subsystemNode.ioSpec.outputs.append(toPortSpec(port, false));
    }
}

} // namespace vws::application
