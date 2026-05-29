#include "application/RunService.h"

#include "domain/Workflow.h"
#include "execution/WorkflowExecutionResult.h"
#include "infrastructure/FileSystemUtils.h"
#include "infrastructure/JsonUtils.h"

#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

namespace vws::application {

namespace {

QJsonArray artifactsToJson(const QList<domain::Artifact>& artifacts)
{
    QJsonArray array;
    for (const auto& artifact : artifacts) {
        array.append(artifact.toJson());
    }
    return array;
}

QJsonObject nodeOutputObject(const execution::NodeExecutionResult& result)
{
    return {
        {"run_id", result.runId},
        {"node_id", result.nodeId},
        {"success", result.success},
        {"outputs", result.outputs},
        {"metadata", result.metadata},
        {"artifacts", artifactsToJson(result.artifacts)},
        {"stdout", result.stdoutText},
        {"stderr", result.stderrText},
        {"error", result.errorMessage},
        {"traceback", result.errorStack},
    };
}

} // namespace

RunService::RunService()
{
}

QStringList RunService::recentRuns(const QString& workspaceRootPath) const
{
    const auto entries = recentRunEntries(workspaceRootPath);
    QStringList runs;
    for (const auto& entry : entries) {
        runs.append(QString("%1 (%2)").arg(entry.status, entry.runId.left(8)));
    }
    return runs;
}

QList<RunListEntry> RunService::recentRunEntries(const QString& workspaceRootPath) const
{
    QList<RunListEntry> entries;

    const auto runDirectories = QDir(QDir(workspaceRootPath).filePath("runs")).entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
        QDir::Time);

    for (const auto& runDirectory : runDirectories) {
        QJsonObject object;
        const auto recordPath = QDir(runDirectory.absoluteFilePath()).filePath("run_record.json");
        if (!infrastructure::JsonUtils::readObjectFromFile(recordPath, object, nullptr)) {
            continue;
        }

        const auto record = domain::RunRecord::fromJson(object);

        RunListEntry entry;
        entry.runId = record.id;
        entry.displayName = QString("%1 (%2)").arg(record.status, record.id.left(8));
        entry.recordPath = recordPath;
        entry.status = record.status;
        entry.startedAt = record.startedAt;
        entry.finishedAt = record.finishedAt;
        entries.append(entry);
    }
    return entries;
}

bool RunService::saveRunRecord(
    const QString& workspaceRootPath,
    const QString& workspaceId,
    const domain::Workflow& workflowSnapshot,
    const execution::WorkflowExecutionResult& result,
    QString* errorMessage) const
{
    const auto runDirectoryPath = QDir(workspaceRootPath).filePath(QString("runs/%1").arg(result.runId));
    if (!infrastructure::FileSystemUtils::ensureDirectory(runDirectoryPath, errorMessage)) {
        return false;
    }

    const auto workflowSnapshotPath =
        QDir(runDirectoryPath).filePath("workflow_snapshot.json");

    if (!infrastructure::JsonUtils::writeObjectToFile(
            workflowSnapshotPath,
            workflowSnapshot.toJson(),
            errorMessage)) {
        return false;
    }

    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    domain::RunRecord record;
    record.id = result.runId;
    record.workspaceId = workspaceId;
    record.workflowId = workflowSnapshot.workflowId;
    record.status = result.status;
    record.startedAt = now;
    record.finishedAt = now;
    record.workflowSnapshotPath = workflowSnapshotPath;

    QSet<QString> nodeIds;
    for (auto it = result.nodeStatuses.cbegin(); it != result.nodeStatuses.cend(); ++it) {
        nodeIds.insert(it.key());
    }
    for (auto it = result.nodeResults.cbegin(); it != result.nodeResults.cend(); ++it) {
        nodeIds.insert(it.key());
    }

    for (const auto& nodeId : nodeIds) {
        domain::NodeRunRecord nodeRun;
        nodeRun.id = QString("%1:%2").arg(result.runId, nodeId);
        nodeRun.runId = result.runId;
        nodeRun.nodeId = nodeId;
        nodeRun.status = result.nodeStatuses.value(nodeId);
        nodeRun.startedAt = now;
        nodeRun.finishedAt = now;
        nodeRun.outputPath = QDir(runDirectoryPath).filePath(QString("%1_output.json").arg(nodeId));
        record.nodeRuns.append(nodeRun);

        if (result.nodeResults.contains(nodeId)) {
            const auto nodeResult = result.nodeResults.value(nodeId);
            record.artifacts.append(nodeResult.artifacts);
            if (!infrastructure::JsonUtils::writeObjectToFile(
                    nodeRun.outputPath,
                    nodeOutputObject(nodeResult),
                    errorMessage)) {
                return false;
            }
        }
    }

    return infrastructure::JsonUtils::writeObjectToFile(
        QDir(runDirectoryPath).filePath("run_record.json"),
        record.toJson(),
        errorMessage);
}

bool RunService::loadRunRecord(
    const QString& workspaceRootPath,
    const QString& runId,
    domain::RunRecord& record,
    QString* errorMessage) const
{
    const auto recordPath = QDir(workspaceRootPath).filePath(
        QString("runs/%1/run_record.json").arg(runId));

    QJsonObject object;
    if (!infrastructure::JsonUtils::readObjectFromFile(recordPath, object, errorMessage)) {
        return false;
    }

    record = domain::RunRecord::fromJson(object);
    return true;
}

bool RunService::deleteRunsForWorkflow(
    const QString& workspaceRootPath,
    const QString& workflowId,
    int* deletedRunCount,
    QString* errorMessage) const
{
    if (deletedRunCount != nullptr) {
        *deletedRunCount = 0;
    }
    if (workspaceRootPath.trimmed().isEmpty() || workflowId.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Workspace path or workflow id is empty.");
        }
        return false;
    }

    const auto runDirectories = QDir(QDir(workspaceRootPath).filePath("runs")).entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
        QDir::Name);

    for (const auto& runDirectory : runDirectories) {
        QJsonObject object;
        const auto runDirectoryPath = runDirectory.absoluteFilePath();
        const auto recordPath = QDir(runDirectoryPath).filePath("run_record.json");
        if (!infrastructure::JsonUtils::readObjectFromFile(recordPath, object, nullptr)) {
            continue;
        }

        const auto record = domain::RunRecord::fromJson(object);
        if (record.workflowId != workflowId) {
            continue;
        }

        QDir directory(runDirectoryPath);
        if (!directory.removeRecursively()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Could not delete run directory: %1").arg(runDirectoryPath);
            }
            return false;
        }

        if (deletedRunCount != nullptr) {
            ++(*deletedRunCount);
        }
    }

    return true;
}

bool RunService::loadNodeOutputObject(
    const domain::NodeRunRecord& nodeRun,
    QJsonObject& object,
    QString* errorMessage) const
{
    if (nodeRun.outputPath.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Node run record has no output path.");
        }
        return false;
    }

    if (!infrastructure::JsonUtils::readObjectFromFile(nodeRun.outputPath, object, errorMessage)) {
        return false;
    }

    return true;
}

} // namespace vws::application
