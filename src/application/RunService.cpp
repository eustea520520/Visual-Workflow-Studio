#include "application/RunService.h"

#include "execution/ExecutionEngine.h"
#include "infrastructure/FileSystemUtils.h"
#include "infrastructure/JsonUtils.h"

#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QSet>

namespace vws::application {

RunService::RunService(execution::ExecutionEngine& executionEngine)
    : m_executionEngine(executionEngine)
{
}

QStringList RunService::recentRuns(const QString& workspaceRootPath) const
{
    Q_UNUSED(m_executionEngine);

    const auto runDirectories = QDir(QDir(workspaceRootPath).filePath("runs")).entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
        QDir::Time);

    QStringList runs;
    for (const auto& runDirectory : runDirectories) {
        QJsonObject object;
        const auto recordPath = QDir(runDirectory.absoluteFilePath()).filePath("run_record.json");
        if (!infrastructure::JsonUtils::readObjectFromFile(recordPath, object, nullptr)) {
            continue;
        }

        const auto record = domain::RunRecord::fromJson(object);
        runs.append(QString("%1 (%2)").arg(record.status, record.id.left(8)));
    }
    return runs;
}

bool RunService::saveRunRecord(
    const QString& workspaceRootPath,
    const QString& workspaceId,
    const QString& workflowId,
    const execution::WorkflowExecutionResult& result,
    QString* errorMessage) const
{
    Q_UNUSED(m_executionEngine);

    const auto runDirectoryPath = QDir(workspaceRootPath).filePath(QString("runs/%1").arg(result.runId));
    if (!infrastructure::FileSystemUtils::ensureDirectory(runDirectoryPath, errorMessage)) {
        return false;
    }

    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    domain::RunRecord record;
    record.id = result.runId;
    record.workspaceId = workspaceId;
    record.workflowId = workflowId;
    record.status = result.status;
    record.startedAt = now;
    record.finishedAt = now;

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
            record.artifacts.append(result.nodeResults.value(nodeId).artifacts);
        }
    }

    return infrastructure::JsonUtils::writeObjectToFile(
        QDir(runDirectoryPath).filePath("run_record.json"),
        record.toJson(),
        errorMessage);
}

} // namespace vws::application
