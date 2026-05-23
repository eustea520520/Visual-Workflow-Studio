#pragma once

#include "execution/DataPacket.h"
#include "execution/GraphIndexes.h"

#include <QHash>
#include <QJsonObject>
#include <QString>

namespace vws::execution {

class InputMerger {
public:
    QJsonObject buildInputs(
        const QString& nodeId,
        const GraphIndexes& indexes,
        const QHash<QString, DataPacket>& completedEdgeData,
        const QHash<QString, QJsonObject>& initialInputsByNodeId = {}) const;
};

} // namespace vws::execution
