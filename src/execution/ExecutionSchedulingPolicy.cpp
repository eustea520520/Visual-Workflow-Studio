#include "execution/ExecutionSchedulingPolicy.h"

#include "domain/NodeTypes.h"

namespace vws::execution {

bool ExecutionSchedulingPolicy::isStarterNode(const domain::Node& node) const
{
    return node.type.trimmed().toLower() == domain::NodeTypes::Starter;
}

bool ExecutionSchedulingPolicy::isTopLevelEntryNode(const domain::Node& node) const
{
    if (isStarterNode(node)) {
        return true;
    }

    // A subsystem with no external inputs is a composite source node: its
    // embedded workflow can start from internal Starter nodes and publish
    // outputs to the parent graph without receiving parent inputs.
    return node.type.trimmed().toLower() == domain::NodeTypes::Subsystem && node.inputPorts.isEmpty();
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
