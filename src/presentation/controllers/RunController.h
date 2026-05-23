#pragma once

#include "application/RunService.h"
#include "domain/Workflow.h"
#include "execution/WorkflowExecutionResult.h"
#include "execution/WorkflowRunOptions.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <functional>

namespace vws::execution {
class ExecutionEngine;
struct WorkflowExecutionResult;
}

namespace vws::application {
class RunService;
}

namespace vws::presentation {

class AppStore;

struct WorkflowRunPlan {
    QString workflowId;
    QString workspaceRootPath;
    QString workspaceId;
    QString runRootPath;
    QString artifactPath;
    domain::Workflow workflow;
};

// RunController is the presentation boundary for execution events. The
// execution engine emits value events; this controller updates AppStore and
// forwards UI-ready events without exposing the event bus to MainWindow.
class RunController final : public QObject {
    Q_OBJECT

public:
    RunController(
        execution::ExecutionEngine& executionEngine,
        application::RunService& runService,
        AppStore& store,
        QObject* parent = nullptr);

    void prepareRun(const QString& workflowId);
    void finishRun(const QString& workflowId);
    bool prepareCurrentWorkflowRun(WorkflowRunPlan& plan, QString* errorMessage = nullptr);
    QList<application::RunListEntry> recentRunEntries() const;
    void runWorkflowAsync(
        const domain::Workflow& workflow,
        const QString& workspacePath,
        const QString& runPath,
        const QString& artifactPath,
        QObject* receiver,
        std::function<void(execution::WorkflowExecutionResult)> onFinished);
    void runWorkflowAsync(
        const domain::Workflow& workflow,
        const execution::WorkflowRunOptions& options,
        const QString& workspacePath,
        const QString& runPath,
        const QString& artifactPath,
        QObject* receiver,
        std::function<void(execution::WorkflowExecutionResult)> onFinished);
    bool saveRunRecord(
        const QString& workspaceRootPath,
        const QString& workspaceId,
        const domain::Workflow& workflow,
        const execution::WorkflowExecutionResult& result,
        QString* errorMessage = nullptr);
    bool saveRunRecord(
        const WorkflowRunPlan& plan,
        const execution::WorkflowExecutionResult& result,
        QString* errorMessage = nullptr);
    bool loadRunRecord(
        const QString& runId,
        domain::RunRecord& record,
        QString* errorMessage = nullptr) const;
    bool loadRunRecordWithNodeOutputs(
        const QString& runId,
        domain::RunRecord& record,
        QHash<QString, QJsonObject>& nodeOutputsByNodeId,
        QString* errorMessage = nullptr) const;
    bool loadNodeOutputObject(
        const domain::NodeRunRecord& nodeRun,
        QJsonObject& object,
        QString* errorMessage = nullptr) const;
    void requestCancelCurrentRun();

signals:
    void nodeStatusChanged(const QString& runId, const QString& workflowId, const QString& nodeId, const QString& status);
    void workflowStatusChanged(const QString& runId, const QString& workflowId, const QString& status);
    void nodeOutputReady(const QString& runId, const QString& workflowId, const QString& nodeId, const QJsonObject& outputs);
    void nodeError(const QString& runId, const QString& workflowId, const QString& nodeId, const QString& message);
    void threadTrace(
        const QString& runId,
        const QString& workflowId,
        const QString& nodeId,
        const QString& phase,
        const QString& threadId,
        const QString& threadName);

private:
    QString workflowIdForRun(const QString& runId) const;

    execution::ExecutionEngine& m_executionEngine;
    application::RunService& m_runService;
    AppStore& m_store;
};

} // namespace vws::presentation
