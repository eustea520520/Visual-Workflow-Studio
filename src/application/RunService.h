#pragma once

#include "domain/RunRecord.h"

#include <QString>
#include <QStringList>

namespace vws::execution {
class ExecutionEngine;
struct WorkflowExecutionResult;
}

namespace vws::application {

// RunService persists execution summaries under runs/<run_id>/run_record.json.
class RunService {
public:
    explicit RunService(execution::ExecutionEngine& executionEngine);

    QStringList recentRuns(const QString& workspaceRootPath) const;
    bool saveRunRecord(
        const QString& workspaceRootPath,
        const QString& workspaceId,
        const QString& workflowId,
        const execution::WorkflowExecutionResult& result,
        QString* errorMessage = nullptr) const;

private:
    execution::ExecutionEngine& m_executionEngine;
};

} // namespace vws::application
