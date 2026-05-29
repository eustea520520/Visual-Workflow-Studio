#pragma once

#include "execution/ExecutionState.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace vws::execution {

// ExecutionEventBus 是执行层到 UI 层的事件出口。
// 执行器不直接操作画布或输出面板，只发信号；UI 自己决定如何展示。
class ExecutionEventBus final : public QObject {
    Q_OBJECT

public:
    explicit ExecutionEventBus(QObject* parent = nullptr);

    void publishWorkflowStatusChanged(const QString& runId, WorkflowStatus status);
    void publishNodeStatusChanged(const QString& runId, const QString& nodeId, NodeStatus status);
    void publishNodeStatusText(const QString& runId, const QString& nodeId, const QString& status);
    void publishNodeOutputReady(const QString& runId, const QString& nodeId, const QJsonObject& outputs);
    void publishNodeDebugOutputReady(const QString& runId, const QString& nodeId, const QString& text);
    void publishNodeError(const QString& runId, const QString& nodeId, const QString& message);
    void publishThreadTrace(
        const QString& runId,
        const QString& nodeId,
        const QString& phase,
        const QString& threadId,
        const QString& threadName);

signals:
    // status 用字符串而不是 enum 传出，方便 UI、日志和未来 JSON 事件直接消费。
    void workflowStatusChanged(const QString& runId, const QString& status);
    void nodeStatusChanged(const QString& runId, const QString& nodeId, const QString& status);
    void nodeOutputReady(const QString& runId, const QString& nodeId, const QJsonObject& outputs);
    void nodeDebugOutputReady(const QString& runId, const QString& nodeId, const QString& text);
    void nodeError(const QString& runId, const QString& nodeId, const QString& message);
    void threadTrace(const QString& runId, const QString& nodeId, const QString& phase, const QString& threadId, const QString& threadName);
};

} // namespace vws::execution
