#include "execution/LoopNodeExecutor.h"

#include "execution/NodeResultStatus.h"
#include "execution/WorkflowExecutionResult.h"

#include <QJsonObject>
#include <QJsonValue>

namespace vws::execution {

QJsonValue LoopNodeExecutor::extractOutputValue(const QJsonObject& outputs, const QString& fromPort, int fromSlot)
{
    if (fromSlot < 0) {
        return QJsonValue(QJsonValue::Null);
    }

    const auto portValue = outputs.contains(fromPort) ? outputs.value(fromPort) : QJsonValue(outputs);
    if (portValue.isArray()) {
        const auto array = portValue.toArray();
        return fromSlot < array.size() ? array.at(fromSlot) : QJsonValue(QJsonValue::Null);
    }
    return fromSlot == 0 ? portValue : QJsonValue(QJsonValue::Null);
}

QJsonValue LoopNodeExecutor::firstSlotOrValue(const QJsonValue& value)
{
    if (!value.isArray()) {
        return value;
    }
    const auto array = value.toArray();
    return array.isEmpty() ? QJsonValue() : array.first();
}

void LoopNodeExecutor::putSlotValue(QJsonObject& inputs, const QString& port, int slot, const QJsonValue& value)
{
    if (slot < 0) {
        return;
    }

    auto array = inputs.value(port).toArray();
    while (array.size() <= slot) {
        array.append(QJsonValue());
    }
    array.replace(slot, value);
    inputs.insert(port, array);
}

QJsonObject LoopNodeExecutor::loopContext(
    int iter,
    int iterations,
    const QJsonValue& previousLoopOutput,
    const QJsonValue& previousBodyOutput,
    const QJsonArray& history)
{
    return {
        {QStringLiteral("iter"), iter},
        {QStringLiteral("index"), iter - 1},
        {QStringLiteral("iteration_count"), iterations},
        {QStringLiteral("previous_loop_output"), previousLoopOutput.isUndefined() ? QJsonValue(QJsonValue::Null) : previousLoopOutput},
        {QStringLiteral("previous_body_output"), previousBodyOutput.isUndefined() ? QJsonValue(QJsonValue::Null) : previousBodyOutput},
        {QStringLiteral("history"), history},
    };
}

QString LoopNodeExecutor::appendIterationText(const QString& previous, const QString& text, int iter)
{
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return previous;
    }
    const auto chunk = QStringLiteral("[Loop iteration %1]\n%2").arg(iter).arg(trimmed);
    return previous.trimmed().isEmpty()
        ? chunk
        : QStringLiteral("%1\n\n%2").arg(previous, chunk);
}

void LoopNodeExecutor::appendIterationDebugOutput(
    QList<NodeDebugOutput>& outputs,
    const QString& nodeId,
    const QString& text,
    int iter,
    const DebugOutputCallback& publishDebugOutput)
{
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    const NodeDebugOutput output{
        nodeId,
        QStringLiteral("[Loop iteration %1]\n%2").arg(iter).arg(trimmed),
    };
    outputs.append(output);
    if (publishDebugOutput) {
        publishDebugOutput(output);
    }
}

LoopNodeExecutionResult LoopNodeExecutor::execute(
    const NodeExecutionRequest& loopRequest,
    const domain::Node& bodyNode,
    const QList<domain::Edge>& loopToBodyEdges,
        int iterations,
        const NodeRunner& runLoopNode,
        const NodeRunner& runBodyNode,
        const IterationStatusCallback& publishIterationStatus,
        const DebugOutputCallback& publishDebugOutput,
        const CancelPredicate& isCancelRequested) const
{
    LoopNodeExecutionResult result;
    QJsonValue previousLoopOutput;
    QJsonValue previousBodyOutput;
    QJsonArray history;
    NodeExecutionResult latestLoopResult;
    NodeExecutionResult latestBodyResult;
    QString loopStdout;
    QString loopStderr;
    QString bodyStdout;
    QString bodyStderr;
    QList<domain::Artifact> loopArtifacts;
    QList<domain::Artifact> bodyArtifacts;

    for (int iter = 1; iter <= iterations; ++iter) {
        if (isCancelRequested && isCancelRequested()) {
            result.errorMessage = QStringLiteral("Run was cancelled.");
            return result;
        }

        auto iterLoopRequest = loopRequest;
        iterLoopRequest.context.insert(
            QStringLiteral("loop"),
            loopContext(iter, iterations, previousLoopOutput, previousBodyOutput, history));
        if (publishIterationStatus) {
            publishIterationStatus(iter, loopRequest.nodeId, NodeStatus::Queued);
            publishIterationStatus(iter, loopRequest.nodeId, NodeStatus::Running);
        }
        latestLoopResult = runLoopNode(iterLoopRequest);
        appendIterationDebugOutput(result.debugOutputs, loopRequest.nodeId, latestLoopResult.stdoutText, iter, publishDebugOutput);
        loopStdout = appendIterationText(loopStdout, latestLoopResult.stdoutText, iter);
        loopStderr = appendIterationText(loopStderr, latestLoopResult.stderrText, iter);
        loopArtifacts.append(latestLoopResult.artifacts);
        if (!latestLoopResult.success) {
            if (publishIterationStatus) {
                publishIterationStatus(iter, loopRequest.nodeId, statusForFailedNodeResult(latestLoopResult));
            }
            latestLoopResult.stdoutText = loopStdout;
            latestLoopResult.stderrText = loopStderr;
            latestLoopResult.artifacts = loopArtifacts;
            result.loopResult = latestLoopResult;
            result.errorMessage = latestLoopResult.errorMessage;
            return result;
        }
        if (publishIterationStatus) {
            publishIterationStatus(iter, loopRequest.nodeId, NodeStatus::Succeeded);
        }

        QJsonObject bodyInputs;
        for (const auto& edge : loopToBodyEdges) {
            putSlotValue(
                bodyInputs,
                edge.toPort,
                edge.toSlot,
                extractOutputValue(latestLoopResult.outputs, edge.fromPort, edge.fromSlot));
        }

        NodeExecutionRequest bodyRequest;
        bodyRequest.runId = loopRequest.runId;
        bodyRequest.nodeId = bodyNode.nodeId;
        bodyRequest.nodeType = bodyNode.type;
        bodyRequest.nodeConfig = bodyNode.config;
        bodyRequest.inputs = bodyInputs;
        bodyRequest.workspacePath = loopRequest.workspacePath;
        bodyRequest.runPath = loopRequest.runPath;
        bodyRequest.artifactPath = loopRequest.artifactPath;
        bodyRequest.timeoutMs = bodyNode.runtime.timeoutMs;
        if (publishIterationStatus) {
            publishIterationStatus(iter, bodyNode.nodeId, NodeStatus::Queued);
            publishIterationStatus(iter, bodyNode.nodeId, NodeStatus::Running);
        }
        latestBodyResult = runBodyNode(bodyRequest);
        appendIterationDebugOutput(result.debugOutputs, bodyNode.nodeId, latestBodyResult.stdoutText, iter, publishDebugOutput);
        bodyStdout = appendIterationText(bodyStdout, latestBodyResult.stdoutText, iter);
        bodyStderr = appendIterationText(bodyStderr, latestBodyResult.stderrText, iter);
        bodyArtifacts.append(latestBodyResult.artifacts);
        if (!latestBodyResult.success) {
            if (publishIterationStatus) {
                publishIterationStatus(iter, bodyNode.nodeId, statusForFailedNodeResult(latestBodyResult));
            }
            latestLoopResult.stdoutText = loopStdout;
            latestLoopResult.stderrText = loopStderr;
            latestLoopResult.artifacts = loopArtifacts;
            latestBodyResult.stdoutText = bodyStdout;
            latestBodyResult.stderrText = bodyStderr;
            latestBodyResult.artifacts = bodyArtifacts;
            result.loopResult = latestLoopResult;
            result.bodyResult = latestBodyResult;
            result.errorMessage = latestBodyResult.errorMessage;
            return result;
        }
        if (publishIterationStatus) {
            publishIterationStatus(iter, bodyNode.nodeId, NodeStatus::Succeeded);
        }

        previousLoopOutput = firstSlotOrValue(latestLoopResult.outputs.value(QStringLiteral("output")));
        previousBodyOutput = firstSlotOrValue(latestBodyResult.outputs.value(QStringLiteral("output")));
        history.append(previousBodyOutput);
    }

    latestLoopResult.metadata.insert(QStringLiteral("loop_summary"), QJsonObject{
        {QStringLiteral("iteration_count"), iterations},
        {QStringLiteral("last_loop_output"), previousLoopOutput},
        {QStringLiteral("last_body_output"), previousBodyOutput},
        {QStringLiteral("history"), history},
    });
    latestLoopResult.stdoutText = loopStdout;
    latestLoopResult.stderrText = loopStderr;
    latestLoopResult.artifacts = loopArtifacts;
    latestBodyResult.metadata.insert(QStringLiteral("loop_summary"), QJsonObject{
        {QStringLiteral("iteration_count"), iterations},
        {QStringLiteral("history"), history},
    });
    latestBodyResult.stdoutText = bodyStdout;
    latestBodyResult.stderrText = bodyStderr;
    latestBodyResult.artifacts = bodyArtifacts;

    result.success = true;
    result.loopResult = latestLoopResult;
    result.bodyResult = latestBodyResult;
    return result;
}

} // namespace vws::execution
