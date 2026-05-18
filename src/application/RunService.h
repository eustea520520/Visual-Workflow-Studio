#pragma once

#include "domain/RunRecord.h"
#include "domain/Workflow.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace vws::execution {
struct WorkflowExecutionResult;
}

namespace vws::application {

struct RunListEntry {
    QString runId;
    QString displayName;
    QString recordPath;
    QString status;
    QString startedAt;
    QString finishedAt;
};

// RunService persists execution summaries under runs/<run_id>/run_record.json.
class RunService {
public:
    RunService();

    QStringList recentRuns(const QString& workspaceRootPath) const;
    QList<RunListEntry> recentRunEntries(const QString& workspaceRootPath) const;

    bool saveRunRecord(
        const QString& workspaceRootPath,
        const QString& workspaceId,
        const domain::Workflow& workflowSnapshot,
        const execution::WorkflowExecutionResult& result,
        QString* errorMessage = nullptr) const;

    bool loadRunRecord(
        const QString& workspaceRootPath,
        const QString& runId,
        domain::RunRecord& record,
        QString* errorMessage = nullptr) const;

    bool deleteRunsForWorkflow(
        const QString& workspaceRootPath,
        const QString& workflowId,
        int* deletedRunCount = nullptr,
        QString* errorMessage = nullptr) const;

    bool loadNodeOutputObject(
        const domain::NodeRunRecord& nodeRun,
        QJsonObject& object,
        QString* errorMessage = nullptr) const;

};

} // namespace vws::application
