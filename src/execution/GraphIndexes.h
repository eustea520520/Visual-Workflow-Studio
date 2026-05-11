#pragma once

#include "domain/Edge.h"
#include "domain/Workflow.h"

#include <QHash>
#include <QList>
#include <QString>

namespace vws::execution {

// GraphIndexes 是一次运行前构建的图索引。
// 执行时不再反复遍历整个 workflow，而是按 nodeId 快速找入边、出边和端口分组。
class GraphIndexes {
public:
    QHash<QString, domain::Node> nodesById;
    QHash<QString, QList<domain::Edge>> incomingEdgesByNode;
    QHash<QString, QList<domain::Edge>> outgoingEdgesByNode;
    QHash<QString, QHash<QString, QList<domain::Edge>>> incomingEdgesByNodePort;

    void build(const domain::Workflow& workflow);
};

} // namespace vws::execution
