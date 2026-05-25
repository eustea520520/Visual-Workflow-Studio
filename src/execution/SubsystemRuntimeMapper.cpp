#include "execution/SubsystemRuntimeMapper.h"

#include <QJsonArray>
#include <QJsonValue>

namespace vws::execution {

QList<SubsystemBoundaryPortRuntime> SubsystemRuntimeMapper::boundaryPorts(
    const QJsonObject& boundary,
    const QString& key)
{
    QList<SubsystemBoundaryPortRuntime> ports;
    const auto array = boundary.value(key).toArray();
    for (const auto& value : array) {
        const auto object = value.toObject();
        SubsystemBoundaryPortRuntime port;
        port.externalPort = object.value(QStringLiteral("external_port")).toString();
        port.displayName = object.value(QStringLiteral("display_name")).toString();
        port.internalNodeId = object.value(QStringLiteral("internal_node_id")).toString();
        port.internalPort = object.value(QStringLiteral("internal_port")).toString();
        if (!port.externalPort.isEmpty() && !port.internalNodeId.isEmpty() && !port.internalPort.isEmpty()) {
            ports.append(port);
        }
    }
    return ports;
}

QHash<QString, QJsonObject> SubsystemRuntimeMapper::mapExternalInputsToInternalNodes(
    const QJsonObject& boundary,
    const QJsonObject& externalInputs) const
{
    QHash<QString, QJsonObject> mapped;
    for (const auto& port : boundaryPorts(boundary, QStringLiteral("inputs"))) {
        auto nodeInputs = mapped.value(port.internalNodeId);
        nodeInputs.insert(port.internalPort, externalInputs.value(port.externalPort));
        mapped.insert(port.internalNodeId, nodeInputs);
    }
    return mapped;
}

QJsonObject SubsystemRuntimeMapper::mapInternalOutputsToExternalPorts(
    const QJsonObject& boundary,
    const WorkflowExecutionResult& internalResult) const
{
    QJsonObject outputs;
    for (const auto& port : boundaryPorts(boundary, QStringLiteral("outputs"))) {
        const auto nodeResult = internalResult.nodeResults.value(port.internalNodeId);
        outputs.insert(port.externalPort,
            nodeResult.outputs.contains(port.internalPort)
                ? nodeResult.outputs.value(port.internalPort)
                : QJsonValue(nodeResult.outputs));
    }
    return outputs;
}

} // namespace vws::execution
