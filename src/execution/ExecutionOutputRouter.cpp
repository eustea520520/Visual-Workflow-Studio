#include "execution/ExecutionOutputRouter.h"

#include <QJsonValue>

namespace vws::execution {

namespace {

QJsonValue extractOutputValue(const QJsonObject& outputs, const QString& fromPort)
{
    return outputs.contains(fromPort) ? outputs.value(fromPort) : QJsonValue(outputs);
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
            edge.toNode,
            edge.toPort,
            extractOutputValue(nodeResult.outputs, edge.fromPort),
            nodeResult.artifacts,
        });
        routed.downstreamNodeIds.append(edge.toNode);
    }

    routed.downstreamNodeIds.removeDuplicates();
    return routed;
}

} // namespace vws::execution
