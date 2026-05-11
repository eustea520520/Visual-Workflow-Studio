#pragma once

#include <QJsonObject>
#include <QString>

namespace vws::domain {

// Artifact 是节点运行产生的产物引用。
// 大文件、表格、图片、模型输出等不直接塞进 UI 或 SQLite，
// 而是保存到文件系统，并在这里记录路径和元数据。
struct Artifact {
    QString artifactId;
    QString runId;
    QString nodeId;
    QString type;
    QString path;
    QJsonObject metadata;

    QJsonObject toJson() const;
    static Artifact fromJson(const QJsonObject& object);
};

} // namespace vws::domain
