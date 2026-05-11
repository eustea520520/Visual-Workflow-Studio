#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace vws::domain {

// NodeTemplate 是可复用节点模板。
// 它不是画布上的节点实例，而是“以后可以拖出来生成 Node 的配置蓝本”。
struct NodeTemplate {
    int schemaVersion = 1;
    QString templateId;
    QString workspaceId;
    QString name;
    QString description;
    QString type;
    QStringList inputPorts;
    QStringList outputPorts;
    QJsonObject config;
    QString createdAt;
    QString updatedAt;
    int version = 1;

    QJsonObject toJson() const;
    static NodeTemplate fromJson(const QJsonObject& object);
};

} // namespace vws::domain
