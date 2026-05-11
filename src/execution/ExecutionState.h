#pragma once

#include <QString>

namespace vws::execution {

// 节点状态是运行时 UI 高亮、日志面板、RunRecord 的基础状态枚举。
enum class NodeStatus {
    Idle,
    Pending,
    Waiting,
    Queued,
    Running,
    Succeeded,
    Failed,
    Skipped,
    Cancelled,
    Timeout,
};

// 工作流整体状态。一次 run 会在这些状态之间流转。
enum class WorkflowStatus {
    Created,
    Validating,
    Running,
    Succeeded,
    Failed,
    Cancelled,
    PartiallySucceeded,
    Timeout,
};

inline QString nodeStatusToString(NodeStatus status)
{
    switch (status) {
    case NodeStatus::Idle:
        return "Idle";
    case NodeStatus::Pending:
        return "Pending";
    case NodeStatus::Waiting:
        return "Waiting";
    case NodeStatus::Queued:
        return "Queued";
    case NodeStatus::Running:
        return "Running";
    case NodeStatus::Succeeded:
        return "Succeeded";
    case NodeStatus::Failed:
        return "Failed";
    case NodeStatus::Skipped:
        return "Skipped";
    case NodeStatus::Cancelled:
        return "Cancelled";
    case NodeStatus::Timeout:
        return "Timeout";
    }

    return "Unknown";
}

inline QString workflowStatusToString(WorkflowStatus status)
{
    switch (status) {
    case WorkflowStatus::Created:
        return "Created";
    case WorkflowStatus::Validating:
        return "Validating";
    case WorkflowStatus::Running:
        return "Running";
    case WorkflowStatus::Succeeded:
        return "Succeeded";
    case WorkflowStatus::Failed:
        return "Failed";
    case WorkflowStatus::Cancelled:
        return "Cancelled";
    case WorkflowStatus::PartiallySucceeded:
        return "PartiallySucceeded";
    case WorkflowStatus::Timeout:
        return "Timeout";
    }

    return "Unknown";
}

} // namespace vws::execution
