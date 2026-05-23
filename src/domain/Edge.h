#pragma once

#include <QJsonObject>
#include <QString>

namespace vws::domain {

// Edge 是节点之间的连接线，同时也是数据依赖关系。
// from_node/from_port 的输出会传递到 to_node/to_port 的输入。
struct Edge {
    QString edgeId;
    QString fromNode;
    QString fromPort;
    int fromSlot = 0;
    QString toNode;
    QString toPort;
    int toSlot = 0;

    QJsonObject toJson() const;
    static Edge fromJson(const QJsonObject& object);
};

} // namespace vws::domain
