#include "application/subsystem/SubsystemBoundaryInferer.h"

#include <QHash>
#include <QSet>

namespace vws::application {

namespace {

QString nodeDisplayName(const domain::Node& node)
{
    return node.name.trimmed().isEmpty() ? node.nodeId : node.name.trimmed();
}

QString externalPortName(const domain::Node& node, const QString& portName)
{
    return QStringLiteral("%1(%2)").arg(nodeDisplayName(node), portName);
}

QString uniquePortName(const QString& baseName, QSet<QString>& usedNames)
{
    if (!usedNames.contains(baseName)) {
        usedNames.insert(baseName);
        return baseName;
    }

    int suffix = 2;
    while (usedNames.contains(QStringLiteral("%1#%2").arg(baseName).arg(suffix))) {
        ++suffix;
    }

    const auto uniqueName = QStringLiteral("%1#%2").arg(baseName).arg(suffix);
    usedNames.insert(uniqueName);
    return uniqueName;
}

domain::PortDimensionSpec portSpecFor(
    const QList<domain::PortDimensionSpec>& specs,
    const QString& portName)
{
    for (const auto& spec : specs) {
        if (spec.portName == portName) {
            auto normalized = spec;
            normalized.dimension = qBound(1, normalized.dimension, 32);
            if (normalized.itemLabels.isEmpty()) {
                for (int index = 0; index < normalized.dimension; ++index) {
                    normalized.itemLabels.append(QString::number(index + 1));
                }
            }
            return normalized;
        }
    }

    domain::PortDimensionSpec fallback;
    fallback.portName = portName;
    fallback.dimension = 1;
    fallback.itemLabels = {QStringLiteral("1")};
    return fallback;
}

SubsystemBoundaryPort makeBoundaryPort(
    const domain::Node& node,
    const QString& internalPort,
    const domain::PortDimensionSpec& spec,
    QSet<QString>& usedExternalNames)
{
    SubsystemBoundaryPort port;
    port.externalPort = uniquePortName(externalPortName(node, internalPort), usedExternalNames);
    port.internalNodeId = node.nodeId;
    port.internalNodeName = nodeDisplayName(node);
    port.internalPort = internalPort;
    port.dimension = qBound(1, spec.dimension, 32);
    port.itemLabels = spec.itemLabels;
    if (port.itemLabels.isEmpty()) {
        for (int index = 0; index < port.dimension; ++index) {
            port.itemLabels.append(QString::number(index + 1));
        }
    }
    return port;
}

} // namespace

SubsystemBoundary SubsystemBoundaryInferer::infer(const domain::Workflow& subWorkflow, QStringList* warnings) const
{
    QHash<QString, int> incomingCount;
    QHash<QString, int> outgoingCount;
    for (const auto& node : subWorkflow.nodes) {
        incomingCount.insert(node.nodeId, 0);
        outgoingCount.insert(node.nodeId, 0);
    }
    for (const auto& edge : subWorkflow.edges) {
        outgoingCount[edge.fromNode] += 1;
        incomingCount[edge.toNode] += 1;
    }

    SubsystemBoundary boundary;
    QSet<QString> usedInputNames;
    QSet<QString> usedOutputNames;
    for (const auto& node : subWorkflow.nodes) {
        if (!node.inputPorts.isEmpty() && incomingCount.value(node.nodeId) == 0) {
            for (const auto& portName : node.inputPorts) {
                const auto spec = portSpecFor(node.ioSpec.inputs, portName);
                boundary.inputs.append(makeBoundaryPort(node, portName, spec, usedInputNames));
            }
        }

        if (!node.outputPorts.isEmpty() && outgoingCount.value(node.nodeId) == 0) {
            for (const auto& portName : node.outputPorts) {
                const auto spec = portSpecFor(node.ioSpec.outputs, portName);
                boundary.outputs.append(makeBoundaryPort(node, portName, spec, usedOutputNames));
            }
        }
    }

    if (warnings != nullptr && subWorkflow.nodes.isEmpty()) {
        warnings->append(QStringLiteral("Empty subsystem has no boundary ports."));
    }

    return boundary;
}

} // namespace vws::application
