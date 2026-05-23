#pragma once

#include "domain/Edge.h"
#include "domain/Node.h"
#include "execution/ExecutionState.h"
#include "execution/NodeExecutionRequest.h"
#include "execution/NodeExecutionResult.h"
#include "execution/WorkflowExecutionResult.h"

#include <QJsonArray>
#include <QList>
#include <functional>

namespace vws::execution {

struct NodeDebugOutput;

struct LoopNodeExecutionResult {
    bool success = false;
    QString errorMessage;
    NodeExecutionResult loopResult;
    NodeExecutionResult bodyResult;
    QList<NodeDebugOutput> debugOutputs;
};

class LoopNodeExecutor final {
public:
    using NodeRunner = std::function<NodeExecutionResult(const NodeExecutionRequest& request)>;
    using CancelPredicate = std::function<bool()>;
    using IterationStatusCallback = std::function<void(int iteration, const QString& nodeId, NodeStatus status)>;

    LoopNodeExecutionResult execute(
        const NodeExecutionRequest& loopRequest,
        const domain::Node& bodyNode,
        const QList<domain::Edge>& loopToBodyEdges,
        int iterations,
        const NodeRunner& runLoopNode,
        const NodeRunner& runBodyNode,
        const IterationStatusCallback& publishIterationStatus,
        const CancelPredicate& isCancelRequested) const;

private:
    static QJsonValue extractOutputValue(const QJsonObject& outputs, const QString& fromPort, int fromSlot);
    static QJsonValue firstSlotOrValue(const QJsonValue& value);
    static void putSlotValue(QJsonObject& inputs, const QString& port, int slot, const QJsonValue& value);
    static QJsonObject loopContext(
        int iter,
        int iterations,
        const QJsonValue& previousLoopOutput,
        const QJsonValue& previousBodyOutput,
        const QJsonArray& history);
    static QString appendIterationText(const QString& previous, const QString& text, int iter);
    static void appendIterationDebugOutput(QList<NodeDebugOutput>& outputs, const QString& nodeId, const QString& text, int iter);
};

} // namespace vws::execution
