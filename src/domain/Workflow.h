#pragma once

#include "domain/Edge.h"
#include "domain/Node.h"

#include <QJsonObject>
#include <QList>
#include <QString>

namespace vws::domain {

// Workflow 表示一个完整工作流，也就是一张有向图。
// nodes 是图里的点，edges 是点之间的数据依赖关系。
// domain 层只描述数据，不负责校验、调度、数据库或 UI。
struct Workflow {
    int schemaVersion = 1;
    QString workflowId;
    QString workspaceId;
    QString name;
    QString description;
    QList<Node> nodes;
    QList<Edge> edges;
    QString createdAt;
    QString updatedAt;
    int version = 1;

    QJsonObject toJson() const;
    static Workflow fromJson(const QJsonObject& object);
};

} // namespace vws::domain
