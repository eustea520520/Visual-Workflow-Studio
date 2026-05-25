#include "execution/NestedWorkflowRunner.h"

#include "execution/ExecutionEngine.h"
#include "execution/ExecutionEventForwarder.h"
#include "execution/ExecutionState.h"

namespace vws::execution {

namespace {

WorkflowExecutionResult cancelledNestedResult(const QString& runId)
{
    WorkflowExecutionResult nestedResult;
    nestedResult.runId = runId;
    nestedResult.success = false;
    nestedResult.status = workflowStatusToString(WorkflowStatus::Cancelled);
    nestedResult.errors.append(QStringLiteral("Run was cancelled."));
    return nestedResult;
}

} // namespace

NestedWorkflowRunner::NestedWorkflowRunner(
    workers::WorkerRegistry& workerRegistry,
    ExecutionEventBus& parentEventBus,
    CancelPredicate cancelPredicate)
    : m_workerRegistry(workerRegistry)
    , m_parentEventBus(parentEventBus)
    , m_cancelPredicate(std::move(cancelPredicate))
{
}

WorkflowExecutionResult NestedWorkflowRunner::run(
    const QString& outerRunId,
    const domain::Workflow& nestedWorkflow,
    const WorkflowRunOptions& nestedOptions,
    const QString& workspacePath,
    const QString& runPath,
    const QString& artifactPath) const
{
    if (isCancelRequested()) {
        return cancelledNestedResult(outerRunId);
    }

    ExecutionEngine nestedEngine(m_workerRegistry);
    ExecutionEventForwarder::connectNestedRun(nestedEngine.eventBus(), m_parentEventBus, outerRunId);

    auto effectiveOptions = nestedOptions;
    const auto nestedCancelPredicate = nestedOptions.cancelPredicate;
    effectiveOptions.runIdOverride = outerRunId;
    effectiveOptions.cancelPredicate = [this, nestedCancelPredicate]() {
        return isCancelRequested()
            || (nestedCancelPredicate && nestedCancelPredicate());
    };

    return nestedEngine.runWorkflow(
        nestedWorkflow,
        effectiveOptions,
        workspacePath,
        runPath,
        artifactPath);
}

bool NestedWorkflowRunner::isCancelRequested() const
{
    return m_cancelPredicate && m_cancelPredicate();
}

} // namespace vws::execution
