#pragma once

#include "domain/Node.h"
#include "execution/ExecutionState.h"

namespace vws::execution {

class ExecutionSchedulingPolicy {
public:
    bool isStarterNode(const domain::Node& node) const;
    bool isTerminalStatus(NodeStatus status) const;
    bool canStillBeScheduled(NodeStatus status) const;
};

} // namespace vws::execution
