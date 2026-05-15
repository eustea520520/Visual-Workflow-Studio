#include "execution/ExecutionSchedulingPolicy.h"

#include "domain/NodeTypes.h"

namespace vws::execution {

bool ExecutionSchedulingPolicy::isStarterNode(const domain::Node& node) const
{
    return node.type == domain::NodeTypes::Starter;
}

bool ExecutionSchedulingPolicy::isTerminalStatus(NodeStatus status) const
{
    return status == NodeStatus::Succeeded
        || status == NodeStatus::Failed
        || status == NodeStatus::Skipped
        || status == NodeStatus::Cancelled
        || status == NodeStatus::Timeout;
}

bool ExecutionSchedulingPolicy::canStillBeScheduled(NodeStatus status) const
{
    return status == NodeStatus::Pending || status == NodeStatus::Waiting;
}

} // namespace vws::execution
