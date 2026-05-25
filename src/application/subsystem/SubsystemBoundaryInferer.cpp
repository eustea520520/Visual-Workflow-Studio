#include "application/subsystem/SubsystemBoundaryInferer.h"

#include <QHash>
#include <QRegularExpression>
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

QString boundaryKey(const QString& nodeId, const QString& portName)
{
    return QStringLiteral("%1:%2").arg(nodeId, portName);
}

QString edgePortKey(const QString& nodeId, const QString& portName)
{
    return boundaryKey(nodeId, portName);
}

QString stableToken(QString value)
{
    value = value.trimmed().toLower();
    value.replace(QRegularExpression(QStringLiteral("[^a-z0-9_\\-]+")), QStringLiteral("_"));
    value.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    value = value.trimmed();
    return value.isEmpty() ? QStringLiteral("port") : value;
}

QString stableExternalPort(const QString& directionPrefix, const domain::Node& node, const QString& portName)
{
    return QStringLiteral("%1_%2_%3")
        .arg(directionPrefix, stableToken(node.nodeId), stableToken(portName));
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

QHash<QString, SubsystemBoundaryPort> previousPortsByInternalEndpoint(const QList<SubsystemBoundaryPort>& ports)
{
    QHash<QString, SubsystemBoundaryPort> byEndpoint;
    for (const auto& port : ports) {
        byEndpoint.insert(boundaryKey(port.internalNodeId, port.internalPort), port);
    }
    return byEndpoint;
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
    const QString& directionPrefix,
    const QHash<QString, SubsystemBoundaryPort>& previousPorts,
    QSet<QString>& usedExternalNames)
{
    const auto displayName = externalPortName(node, internalPort);
    const auto previous = previousPorts.value(boundaryKey(node.nodeId, internalPort));
    SubsystemBoundaryPort port;
    port.externalPort = uniquePortName(
        previous.externalPort.trimmed().isEmpty()
            ? stableExternalPort(directionPrefix, node, internalPort)
            : previous.externalPort.trimmed(),
        usedExternalNames);
    port.displayName = displayName;
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

SubsystemBoundary SubsystemBoundaryInferer::infer(
    const domain::Workflow& subWorkflow,
    const SubsystemBoundary& previousBoundary,
    QStringList* warnings) const
{
    QSet<QString> writtenInputPorts;
    QSet<QString> consumedOutputPorts;
    for (const auto& edge : subWorkflow.edges) {
        consumedOutputPorts.insert(edgePortKey(edge.fromNode, edge.fromPort));
        writtenInputPorts.insert(edgePortKey(edge.toNode, edge.toPort));
    }

    SubsystemBoundary boundary;
    const auto previousInputs = previousPortsByInternalEndpoint(previousBoundary.inputs);
    const auto previousOutputs = previousPortsByInternalEndpoint(previousBoundary.outputs);
    QSet<QString> usedInputNames;
    QSet<QString> usedOutputNames;
    for (const auto& node : subWorkflow.nodes) {
        for (const auto& portName : node.inputPorts) {
            if (!writtenInputPorts.contains(edgePortKey(node.nodeId, portName))) {
                const auto spec = portSpecFor(node.ioSpec.inputs, portName);
                boundary.inputs.append(makeBoundaryPort(
                    node,
                    portName,
                    spec,
                    QStringLiteral("in"),
                    previousInputs,
                    usedInputNames));
            }
        }

        for (const auto& portName : node.outputPorts) {
            if (!consumedOutputPorts.contains(edgePortKey(node.nodeId, portName))) {
                const auto spec = portSpecFor(node.ioSpec.outputs, portName);
                boundary.outputs.append(makeBoundaryPort(
                    node,
                    portName,
                    spec,
                    QStringLiteral("out"),
                    previousOutputs,
                    usedOutputNames));
            }
        }
    }

    if (warnings != nullptr && subWorkflow.nodes.isEmpty()) {
        warnings->append(QStringLiteral("Empty subsystem has no boundary ports."));
    }

    return boundary;
}

} // namespace vws::application
