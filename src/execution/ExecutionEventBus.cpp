#include "execution/ExecutionEventBus.h"

namespace vws::execution {

ExecutionEventBus::ExecutionEventBus(QObject* parent)
    : QObject(parent)
{
}

void ExecutionEventBus::publishWorkflowStatusChanged(const QString& runId, WorkflowStatus status)
{
    emit workflowStatusChanged(runId, workflowStatusToString(status));
}

void ExecutionEventBus::publishNodeStatusChanged(const QString& runId, const QString& nodeId, NodeStatus status)
{
    emit nodeStatusChanged(runId, nodeId, nodeStatusToString(status));
}

void ExecutionEventBus::publishNodeOutputReady(const QString& runId, const QString& nodeId, const QJsonObject& outputs)
{
    emit nodeOutputReady(runId, nodeId, outputs);
}

void ExecutionEventBus::publishNodeError(const QString& runId, const QString& nodeId, const QString& message)
{
    emit nodeError(runId, nodeId, message);
}

void ExecutionEventBus::publishThreadTrace(
    const QString& runId,
    const QString& nodeId,
    const QString& phase,
    const QString& threadId,
    const QString& threadName)
{
    emit threadTrace(runId, nodeId, phase, threadId, threadName);
}

} // namespace vws::execution
