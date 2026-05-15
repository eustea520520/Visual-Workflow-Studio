#pragma once

#include "execution/DataPacket.h"
#include "execution/GraphIndexes.h"
#include "execution/NodeExecutionResult.h"

#include <QList>
#include <QStringList>

namespace vws::execution {

struct RoutedNodeOutputs {
    QList<DataPacket> packets;
    QStringList downstreamNodeIds;
};

class ExecutionOutputRouter {
public:
    RoutedNodeOutputs route(
        const QString& nodeId,
        const GraphIndexes& indexes,
        const NodeExecutionResult& nodeResult) const;
};

} // namespace vws::execution
