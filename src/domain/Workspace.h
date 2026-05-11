#pragma once

#include <QJsonObject>
#include <QString>

namespace vws::domain {

// Workspace 是最高层组织单位。
// 一个工作区会包含多个工作流、节点模板、运行记录、产物目录和密钥引用。
struct Workspace {
    QString id;
    QString name;
    QString rootPath;
    QString createdAt;
    QString updatedAt;
    QJsonObject config;

    QJsonObject toJson() const;
    static Workspace fromJson(const QJsonObject& object);
};

} // namespace vws::domain
