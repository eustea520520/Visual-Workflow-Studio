#include "application/subsystem/SubsystemIoMapper.h"

namespace vws::application {

QHash<QString, QJsonObject> SubsystemIoMapper::mapExternalInputsToInternalNodes(
    const SubsystemBoundary& boundary,
    const QJsonObject& externalInputs) const
{
    QHash<QString, QJsonObject> mapped;
    for (const auto& port : boundary.inputs) {
        auto nodeInputs = mapped.value(port.internalNodeId);
        nodeInputs.insert(port.internalPort, externalInputs.value(port.externalPort));
        mapped.insert(port.internalNodeId, nodeInputs);
    }
    return mapped;
}

QJsonObject SubsystemIoMapper::mapInternalOutputsToExternalPorts(
    const SubsystemBoundary& boundary,
    const execution::WorkflowExecutionResult& internalResult) const
{
    QJsonObject outputs;
    for (const auto& port : boundary.outputs) {
        const auto nodeResult = internalResult.nodeResults.value(port.internalNodeId);
        const auto value = nodeResult.outputs.contains(port.internalPort)
            ? nodeResult.outputs.value(port.internalPort)
            : nodeResult.outputs;
        outputs.insert(port.externalPort, value);
    }
    return outputs;
}

} // namespace vws::application
