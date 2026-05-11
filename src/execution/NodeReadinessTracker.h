#pragma once

#include "execution/DataPacket.h"
#include "execution/GraphIndexes.h"

#include <QHash>
#include <QString>

namespace vws::execution {

class NodeReadinessTracker {
public:
    bool isReady(
        const QString& nodeId,
        const GraphIndexes& indexes,
        const QHash<QString, DataPacket>& completedEdgeData) const;
};

} // namespace vws::execution
