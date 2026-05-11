#pragma once

#include "domain/Workflow.h"
#include "execution/ExecutionEventBus.h"
#include "execution/NodeExecutionResult.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QThreadPool>
#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>

namespace vws::workers {
class WorkerRegistry;
}

namespace vws::execution {

// 一次工作流执行的内存结果摘要。
// 后续 RunService 会把这里的信息转成 RunRecord / NodeRunRecord 并持久化。
struct WorkflowExecutionResult {
    QString runId;
    bool success = false;
    QString status;
    QStringList errors;
    QHash<QString, QString> nodeStatuses;
    QHash<QString, NodeExecutionResult> nodeResults;
};

// ExecutionEngine 是工作流运行主控。
// 它负责串起：图校验 -> DAG 调度 -> Worker 执行 -> 状态事件 -> 执行结果。
class ExecutionEngine {
public:
    explicit ExecutionEngine(workers::WorkerRegistry& workerRegistry);

    ExecutionEventBus& eventBus();
    const ExecutionEventBus& eventBus() const;
    QString statusText() const;
    WorkflowExecutionResult runWorkflow(
        const domain::Workflow& workflow,
        const QString& workspacePath = QString(),
        const QString& runPath = QString(),
        const QString& artifactPath = QString());
    void runWorkflowAsync(
        const domain::Workflow& workflow,
        const QString& workspacePath,
        const QString& runPath,
        const QString& artifactPath,
        QObject* receiver,
        std::function<void(WorkflowExecutionResult)> onFinished);
    void requestCancelCurrentRun();

private:
    workers::WorkerRegistry& m_workerRegistry;
    ExecutionEventBus m_eventBus;
    QThreadPool m_runPool;
    mutable QMutex m_runControlMutex;
    QString m_currentRunId;
    std::atomic_bool m_cancelRequested = false;
};

} // namespace vws::execution
