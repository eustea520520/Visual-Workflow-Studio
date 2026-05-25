#pragma once

#include "execution/NodeExecutionRequest.h"
#include "execution/NodeExecutionResult.h"
#include "execution/WorkflowExecutionResult.h"
#include "execution/WorkflowRunOptions.h"

#include <functional>

namespace vws::domain {
struct Workflow;
}

namespace vws::execution {

class SubsystemNodeExecutor final {
public:
    using CancelPredicate = std::function<bool()>;
    using NestedRunFunction = std::function<WorkflowExecutionResult(
        const QString& outerRunId,
        const domain::Workflow& nestedWorkflow,
        const WorkflowRunOptions& nestedOptions,
        const QString& workspacePath,
        const QString& runPath,
        const QString& artifactPath)>;

    SubsystemNodeExecutor(
        NestedRunFunction nestedRun,
        CancelPredicate cancelPredicate);

    NodeExecutionResult execute(
        const NodeExecutionRequest& request,
        int nodeDispatchDelayMs) const;

private:
    bool isCancelRequested() const;

    NestedRunFunction m_nestedRun;
    CancelPredicate m_cancelPredicate;
};

} // namespace vws::execution
