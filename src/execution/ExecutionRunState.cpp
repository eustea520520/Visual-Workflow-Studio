#include "execution/ExecutionRunState.h"

#include <utility>

namespace vws::execution {

ExecutionRunState::ExecutionRunState(QString runId, int totalNodeCount)
    : m_unfinishedNodes(totalNodeCount)
{
    m_result.runId = std::move(runId);
}

WorkflowExecutionResult& ExecutionRunState::result()
{
    return m_result;
}

const WorkflowExecutionResult& ExecutionRunState::result() const
{
    return m_result;
}

QHash<QString, DataPacket>& ExecutionRunState::completedEdgeData()
{
    return m_completedEdgeData;
}

const QHash<QString, DataPacket>& ExecutionRunState::completedEdgeData() const
{
    return m_completedEdgeData;
}

int ExecutionRunState::activeTasks() const
{
    return m_activeTasks;
}

bool ExecutionRunState::hasUnfinishedNodes() const
{
    return m_unfinishedNodes > 0;
}

NodeStatus ExecutionRunState::nodeStatus(const QString& nodeId) const
{
    return m_nodeStates.value(nodeId);
}

void ExecutionRunState::initializeNode(const QString& nodeId)
{
    m_nodeStates.insert(nodeId, NodeStatus::Pending);
    m_result.nodeStatuses.insert(nodeId, nodeStatusToString(NodeStatus::Pending));
}

void ExecutionRunState::setNodeStatus(const QString& nodeId, NodeStatus status)
{
    m_nodeStates.insert(nodeId, status);
    m_result.nodeStatuses.insert(nodeId, nodeStatusToString(status));
}

bool ExecutionRunState::finishNode(const QString& nodeId, NodeStatus status)
{
    if (isTerminalStatus(m_nodeStates.value(nodeId))) {
        return false;
    }

    setNodeStatus(nodeId, status);
    --m_unfinishedNodes;

    if (status == NodeStatus::Succeeded) {
        m_sawSuccess = true;
    } else if (status == NodeStatus::Cancelled) {
        m_sawCancellation = true;
    } else {
        m_sawFailure = true;
    }

    return true;
}

void ExecutionRunState::incrementActiveTasks()
{
    ++m_activeTasks;
}

void ExecutionRunState::decrementActiveTasks()
{
    --m_activeTasks;
}

void ExecutionRunState::recordNodeResult(const QString& nodeId, const NodeExecutionResult& nodeResult)
{
    m_result.nodeResults.insert(nodeId, nodeResult);
}

void ExecutionRunState::appendDebugOutput(const QString& nodeId, const QString& text)
{
    if (text.trimmed().isEmpty()) {
        return;
    }
    m_result.debugOutputs.append(NodeDebugOutput{nodeId, text});
}

void ExecutionRunState::appendDebugOutputs(const QList<NodeDebugOutput>& outputs)
{
    for (const auto& output : outputs) {
        appendDebugOutput(output.nodeId, output.text);
    }
}

void ExecutionRunState::appendError(const QString& message)
{
    m_result.errors.append(message);
}

void ExecutionRunState::setWorkflowStatus(WorkflowStatus status)
{
    m_result.status = workflowStatusToString(status);
}

WorkflowStatus ExecutionRunState::applyFinalWorkflowStatus()
{
    if (m_sawCancellation) {
        m_result.success = false;
        setWorkflowStatus(WorkflowStatus::Cancelled);
        return WorkflowStatus::Cancelled;
    } else if (!m_sawFailure && m_sawSuccess) {
        m_result.success = true;
        setWorkflowStatus(WorkflowStatus::Succeeded);
        return WorkflowStatus::Succeeded;
    } else if (m_sawSuccess && m_sawFailure) {
        m_result.success = false;
        setWorkflowStatus(WorkflowStatus::PartiallySucceeded);
        return WorkflowStatus::PartiallySucceeded;
    } else {
        m_result.success = false;
        setWorkflowStatus(WorkflowStatus::Failed);
        return WorkflowStatus::Failed;
    }
}

bool ExecutionRunState::isTerminalStatus(NodeStatus status)
{
    return status == NodeStatus::Succeeded
        || status == NodeStatus::Failed
        || status == NodeStatus::Skipped
        || status == NodeStatus::Cancelled
        || status == NodeStatus::Timeout;
}

} // namespace vws::execution
