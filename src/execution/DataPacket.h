#pragma once

#include "domain/Artifact.h"

#include <QJsonValue>
#include <QList>
#include <QString>

namespace vws::execution {

// DataPacket 表示一条 Edge 上已经准备好的数据。
// 它把“节点输出”转换成“边上传输的数据”，供下游 InputMerger 稳定合并。
struct DataPacket {
    QString edgeId;
    QString fromNodeId;
    QString fromPort;
    int fromSlot = 0;
    QString toNodeId;
    QString toPort;
    int toSlot = 0;
    QJsonValue value;
    QList<domain::Artifact> artifacts;
};

} // namespace vws::execution
