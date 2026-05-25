#include "execution/ExecutionOutputRouter.h"

#include <QJsonArray>
#include <QJsonValue>

namespace vws::execution {

namespace {

QJsonValue extractOutputValue(const QJsonObject& outputs, const QString& fromPort, int fromSlot)
{
    if (fromSlot < 0) {
        return QJsonValue(QJsonValue::Null);
    }

    const auto portValue = outputs.contains(fromPort) ? outputs.value(fromPort) : QJsonValue(outputs);
    if (portValue.isArray()) {
        const auto array = portValue.toArray();
        return fromSlot < array.size() ? array.at(fromSlot) : QJsonValue(QJsonValue::Null);
    }

    return fromSlot == 0 ? portValue : QJsonValue(QJsonValue::Null);
}

} // namespace

RoutedNodeOutputs ExecutionOutputRouter::route(
    const QString& nodeId,
    const GraphIndexes& indexes,
    const NodeExecutionResult& nodeResult) const
{
    RoutedNodeOutputs routed;

    for (const auto& edge : indexes.outgoingEdgesByNode.value(nodeId)) {
        routed.packets.append(DataPacket{
            edge.edgeId,
            edge.fromNode,
            edge.fromPort,
            edge.fromSlot,
            edge.toNode,
            edge.toPort,
            edge.toSlot,
            extractOutputValue(nodeResult.outputs, edge.fromPort, edge.fromSlot),
            nodeResult.artifacts,
        });
        routed.downstreamNodeIds.append(edge.toNode);
    }

    routed.downstreamNodeIds.removeDuplicates();
    return routed;
}

} // namespace vws::execution
