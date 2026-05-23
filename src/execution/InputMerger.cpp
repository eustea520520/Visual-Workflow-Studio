#include "execution/InputMerger.h"

#include <QJsonArray>
#include <QJsonObject>

namespace vws::execution {

QJsonObject InputMerger::buildInputs(
    const QString& nodeId,
    const GraphIndexes& indexes,
    const QHash<QString, DataPacket>& completedEdgeData,
    const QHash<QString, QJsonObject>& initialInputsByNodeId) const
{
    QJsonObject inputs = initialInputsByNodeId.value(nodeId);
    const auto portGroups = indexes.incomingEdgesByNodePort.value(nodeId);
    if (portGroups.isEmpty() && initialInputsByNodeId.contains(nodeId)) {
        return inputs;
    }

    for (auto portIt = portGroups.cbegin(); portIt != portGroups.cend(); ++portIt) {
        const auto& toPort = portIt.key();
        const auto& edges = portIt.value();

        int maxSlot = 0;
        for (const auto& edge : edges) {
            maxSlot = qMax(maxSlot, edge.toSlot);
        }

        QJsonArray inputArray;
        for (int index = 0; index <= maxSlot; ++index) {
            inputArray.append(QJsonValue());
        }

        for (const auto& edge : edges) {
            const auto packet = completedEdgeData.value(edge.edgeId);
            inputArray.replace(edge.toSlot, packet.value);
        }
        inputs.insert(toPort, inputArray);
    }

    return inputs;
}

} // namespace vws::execution
