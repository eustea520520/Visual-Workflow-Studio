#pragma once

#include "execution/ExecutionState.h"
#include "execution/NodeExecutionResult.h"

namespace vws::execution {

NodeStatus statusForFailedNodeResult(const NodeExecutionResult& result);

} // namespace vws::execution
