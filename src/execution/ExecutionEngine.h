#pragma once

#include "domain/Workflow.h"
#include "execution/ExecutionCancellationState.h"
#include "execution/ExecutionEventBus.h"
#include "execution/WorkflowExecutionResult.h"
#include "execution/WorkflowRunOptions.h"

#include <QObject>
#include <QThreadPool>
#include <QString>
#include <functional>

namespace vws::workers {
class WorkerRegistry;
}

namespace vws::execution {

// 工作流运行主控：负责图校验、DAG 调度、Worker 执行和事件发布。
// 单次运行的临时状态放在 ExecutionRunState 中，避免引擎对象长期持有运行结果。
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
    WorkflowExecutionResult runWorkflow(
        const domain::Workflow& workflow,
        const WorkflowRunOptions& options,
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
    void runWorkflowAsync(
        const domain::Workflow& workflow,
        const WorkflowRunOptions& options,
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
    ExecutionCancellationState m_cancellationState;
};

} // namespace vws::execution
