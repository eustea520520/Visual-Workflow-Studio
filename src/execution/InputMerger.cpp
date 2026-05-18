#include "execution/InputMerger.h"

#include <QJsonArray>
#include <QJsonObject>

namespace vws::execution {

QJsonObject InputMerger::buildInputs(
    const QString& nodeId,
    const GraphIndexes& indexes,
    const QHash<QString, DataPacket>& completedEdgeData) const
{
    QJsonObject inputs;

    const auto portGroups = indexes.incomingEdgesByNodePort.value(nodeId);
    for (auto portIt = portGroups.cbegin(); portIt != portGroups.cend(); ++portIt) {
        const auto& toPort = portIt.key();
        const auto& edges = portIt.value();
        bool hasSlotEdges = false;
        for (const auto& edge : edges) {
            if (edge.toSlot >= 0) {
                hasSlotEdges = true;
                break;
            }
        }

        if (hasSlotEdges) {
            int maxSlot = 0;
            for (const auto& edge : edges) {
                maxSlot = qMax(maxSlot, edge.toSlot);
            }

            QJsonArray inputArray;
            for (int index = 0; index <= maxSlot; ++index) {
                inputArray.append(QJsonValue());
            }

            for (const auto& edge : edges) {
                if (edge.toSlot < 0) {
                    continue;
                }
                const auto packet = completedEdgeData.value(edge.edgeId);
                inputArray.replace(edge.toSlot, packet.value);
            }
            inputs.insert(toPort, inputArray);
            continue;
        }

        if (edges.size() == 1) {
            const auto packet = completedEdgeData.value(edges.first().edgeId);
            inputs.insert(toPort, packet.value);
            continue;
        }

        QJsonArray packetArray;
        for (const auto& edge : edges) {
            const auto packet = completedEdgeData.value(edge.edgeId);
            // 多个上游共同连到同一个输入端口时，下游业务代码更关心“数据列表”，
            // 而不是边的调试元信息。packet.value 在 ExecutionEngine 中已经按 fromPort
            // 提取过：通常就是上游节点的 outputs["output"]。
            packetArray.append(packet.value);
        }
        inputs.insert(toPort, packetArray);
    }

    return inputs;
}

} // namespace vws::execution
