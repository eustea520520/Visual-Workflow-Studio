#pragma once

#include "domain/Artifact.h"

#include <QJsonObject>
#include <QList>
#include <QString>

namespace vws::domain {

// NodeRunRecord 记录一次工作流运行中，某一个节点的运行结果。
// stdout/stderr/error/output 都保存路径，避免把大日志或大输出塞进内存。
struct NodeRunRecord {
    QString id;
    QString runId;
    QString nodeId;
    QString status;
    QString startedAt;
    QString finishedAt;
    QString stdoutPath;
    QString stderrPath;
    QString errorPath;
    QString outputPath;

    QJsonObject toJson() const;
    static NodeRunRecord fromJson(const QJsonObject& object);
};

// RunRecord 记录“点击运行一次工作流”的整体历史。
// 它关联每个节点的 NodeRunRecord 和这次运行产生的 Artifact。
struct RunRecord {
    QString id;
    QString workspaceId;
    QString workflowId;
    QString status;
    QString startedAt;
    QString finishedAt;
    QString summaryPath;
    QList<NodeRunRecord> nodeRuns;
    QList<Artifact> artifacts;

    QJsonObject toJson() const;
    static RunRecord fromJson(const QJsonObject& object);
};

} // namespace vws::domain
