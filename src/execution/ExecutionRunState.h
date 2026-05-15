#pragma once

#include "execution/DataPacket.h"
#include "execution/ExecutionState.h"
#include "execution/NodeExecutionResult.h"
#include "execution/WorkflowExecutionResult.h"

#include <QHash>
#include <QString>

namespace vws::execution {

// ExecutionRunState 管理“一次运行过程中的可变状态”。
// ExecutionEngine 负责调度和并发控制；本类负责记录节点状态、边数据、错误和最终运行结论。
class ExecutionRunState {
public:
    ExecutionRunState(QString runId, int totalNodeCount);

    WorkflowExecutionResult& result();
    const WorkflowExecutionResult& result() const;

    QHash<QString, DataPacket>& completedEdgeData();
    const QHash<QString, DataPacket>& completedEdgeData() const;

    int activeTasks() const;
    bool hasUnfinishedNodes() const;

    NodeStatus nodeStatus(const QString& nodeId) const;
    void initializeNode(const QString& nodeId);
    void setNodeStatus(const QString& nodeId, NodeStatus status);
    bool finishNode(const QString& nodeId, NodeStatus status);

    void incrementActiveTasks();
    void decrementActiveTasks();
    void recordNodeResult(const QString& nodeId, const NodeExecutionResult& nodeResult);
    void appendError(const QString& message);

    void setWorkflowStatus(WorkflowStatus status);
    WorkflowStatus applyFinalWorkflowStatus();

private:
    static bool isTerminalStatus(NodeStatus status);

    WorkflowExecutionResult m_result;
    QHash<QString, NodeStatus> m_nodeStates;
    QHash<QString, DataPacket> m_completedEdgeData;
    int m_unfinishedNodes = 0;
    int m_activeTasks = 0;
    bool m_sawFailure = false;
    bool m_sawSuccess = false;
    bool m_sawCancellation = false;
};

} // namespace vws::execution
