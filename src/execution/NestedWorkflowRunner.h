#pragma once

#include "domain/Workflow.h"
#include "execution/WorkflowExecutionResult.h"
#include "execution/WorkflowRunOptions.h"

#include <functional>

namespace vws::workers {
class WorkerRegistry;
}

namespace vws::execution {

class ExecutionEventBus;

class NestedWorkflowRunner final {
public:
    using CancelPredicate = std::function<bool()>;

    NestedWorkflowRunner(
        workers::WorkerRegistry& workerRegistry,
        ExecutionEventBus& parentEventBus,
        CancelPredicate cancelPredicate);

    WorkflowExecutionResult run(
        const QString& outerRunId,
        const domain::Workflow& nestedWorkflow,
        const WorkflowRunOptions& nestedOptions,
        const QString& workspacePath,
        const QString& runPath,
        const QString& artifactPath) const;

private:
    bool isCancelRequested() const;

    workers::WorkerRegistry& m_workerRegistry;
    ExecutionEventBus& m_parentEventBus;
    CancelPredicate m_cancelPredicate;
};

} // namespace vws::execution
