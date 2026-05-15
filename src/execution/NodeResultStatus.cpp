#include "execution/NodeResultStatus.h"

namespace vws::execution {

NodeStatus statusForFailedNodeResult(const NodeExecutionResult& result)
{
    const auto text = QString("%1\n%2").arg(result.errorMessage, result.stderrText).toLower();
    return text.contains("timed out") || text.contains("timeout")
        ? NodeStatus::Timeout
        : NodeStatus::Failed;
}

} // namespace vws::execution
