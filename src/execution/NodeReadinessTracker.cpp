#include "execution/NodeReadinessTracker.h"

namespace vws::execution {

bool NodeReadinessTracker::isReady(
    const QString& nodeId,
    const GraphIndexes& indexes,
    const QHash<QString, DataPacket>& completedEdgeData) const
{
    for (const auto& edge : indexes.incomingEdgesByNode.value(nodeId)) {
        if (!completedEdgeData.contains(edge.edgeId)) {
            return false;
        }
    }
    return true;
}

} // namespace vws::execution
