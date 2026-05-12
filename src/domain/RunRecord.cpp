#include "domain/RunRecord.h"

#include <QJsonArray>

namespace vws::domain {

QJsonObject NodeRunRecord::toJson() const
{
    return {
        {"id", id},
        {"run_id", runId},
        {"node_id", nodeId},
        {"status", status},
        {"started_at", startedAt},
        {"finished_at", finishedAt},
        {"stdout_path", stdoutPath},
        {"stderr_path", stderrPath},
        {"error_path", errorPath},
        {"output_path", outputPath},
    };
}

NodeRunRecord NodeRunRecord::fromJson(const QJsonObject& object)
{
    NodeRunRecord record;
    record.id = object.value("id").toString();
    record.runId = object.value("run_id").toString();
    record.nodeId = object.value("node_id").toString();
    record.status = object.value("status").toString();
    record.startedAt = object.value("started_at").toString();
    record.finishedAt = object.value("finished_at").toString();
    record.stdoutPath = object.value("stdout_path").toString();
    record.stderrPath = object.value("stderr_path").toString();
    record.errorPath = object.value("error_path").toString();
    record.outputPath = object.value("output_path").toString();
    return record;
}

QJsonObject RunRecord::toJson() const
{
    QJsonArray nodeRunArray;
    for (const auto& nodeRun : nodeRuns) {
        nodeRunArray.append(nodeRun.toJson());
    }

    QJsonArray artifactArray;
    for (const auto& artifact : artifacts) {
        artifactArray.append(artifact.toJson());
    }

    return {
        {"id", id},
        {"workspace_id", workspaceId},
        {"workflow_id", workflowId},
        {"status", status},
        {"started_at", startedAt},
        {"finished_at", finishedAt},
        {"summary_path", summaryPath},
        {"workflow_snapshot_path", workflowSnapshotPath},
        {"node_runs", nodeRunArray},
        {"artifacts", artifactArray},
    };
}

RunRecord RunRecord::fromJson(const QJsonObject& object)
{
    RunRecord record;
    record.id = object.value("id").toString();
    record.workspaceId = object.value("workspace_id").toString();
    record.workflowId = object.value("workflow_id").toString();
    record.status = object.value("status").toString();
    record.startedAt = object.value("started_at").toString();
    record.finishedAt = object.value("finished_at").toString();
    record.summaryPath = object.value("summary_path").toString();
    record.workflowSnapshotPath = object.value("workflow_snapshot_path").toString();

    const auto nodeRuns = object.value("node_runs").toArray();
    for (const auto& nodeRunValue : nodeRuns) {
        record.nodeRuns.append(NodeRunRecord::fromJson(nodeRunValue.toObject()));
    }

    const auto artifacts = object.value("artifacts").toArray();
    for (const auto& artifactValue : artifacts) {
        record.artifacts.append(Artifact::fromJson(artifactValue.toObject()));
    }

    return record;
}

} // namespace vws::domain
