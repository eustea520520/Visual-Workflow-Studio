#include "execution/SubsystemNodeExecutor.h"

#include "domain/NodeConfigKeys.h"
#include "domain/Workflow.h"
#include "domain/WorkflowJsonParser.h"
#include "execution/SubsystemRuntimeMapper.h"

#include <QStringList>

namespace vws::execution {

namespace {

QString subsystemTextOutput(
    const domain::Workflow& subWorkflow,
    const WorkflowExecutionResult& internalResult,
    const std::function<QString(const NodeExecutionResult&)>& textSelector)
{
    QStringList chunks;
    for (const auto& node : subWorkflow.nodes) {
        const auto nodeResult = internalResult.nodeResults.value(node.nodeId);
        const auto text = textSelector(nodeResult).trimmed();
        if (text.isEmpty()) {
            continue;
        }
        const auto label = node.name.trimmed().isEmpty() ? node.nodeId : node.name.trimmed();
        chunks.append(QStringLiteral("[%1]\n%2").arg(label, text));
    }
    return chunks.join(QStringLiteral("\n\n"));
}

} // namespace

SubsystemNodeExecutor::SubsystemNodeExecutor(
    NestedRunFunction nestedRun,
    CancelPredicate cancelPredicate)
    : m_nestedRun(std::move(nestedRun))
    , m_cancelPredicate(std::move(cancelPredicate))
{
}

NodeExecutionResult SubsystemNodeExecutor::execute(
    const NodeExecutionRequest& request,
    int nodeDispatchDelayMs) const
{
    NodeExecutionResult nodeResult;
    nodeResult.runId = request.runId;
    nodeResult.nodeId = request.nodeId;

    if (isCancelRequested()) {
        nodeResult.errorMessage = QStringLiteral("Run was cancelled.");
        return nodeResult;
    }

    const auto subWorkflowObject = request.nodeConfig.value(domain::NodeConfigKeys::SubsystemWorkflow).toObject();
    if (subWorkflowObject.isEmpty()) {
        nodeResult.errorMessage = QStringLiteral("Subsystem node has no embedded workflow.");
        return nodeResult;
    }

    const auto parseResult = domain::WorkflowJsonParser::parseStrict(subWorkflowObject);
    if (!parseResult.success) {
        nodeResult.errorMessage = parseResult.errors.join(QStringLiteral("\n"));
        return nodeResult;
    }

    const auto subWorkflow = parseResult.workflow;
    if (subWorkflow.nodes.isEmpty()) {
        nodeResult.errorMessage = QStringLiteral("Subsystem workflow is empty.");
        return nodeResult;
    }

    const auto boundary = request.nodeConfig.value(domain::NodeConfigKeys::SubsystemBoundary).toObject();
    const SubsystemRuntimeMapper subsystemMapper;
    WorkflowRunOptions subOptions;
    subOptions.validationMode = GraphValidationMode::SubsystemWorkflow;
    subOptions.allowImplicitEntryNodes = true;
    subOptions.nodeDispatchDelayMs = nodeDispatchDelayMs;
    subOptions.initialInputsByNodeId = subsystemMapper.mapExternalInputsToInternalNodes(boundary, request.inputs);

    const auto internalResult = m_nestedRun(
        request.runId,
        subWorkflow,
        subOptions,
        request.workspacePath,
        request.runPath,
        request.artifactPath);

    nodeResult.success = internalResult.success;
    nodeResult.outputs = subsystemMapper.mapInternalOutputsToExternalPorts(boundary, internalResult);
    nodeResult.stdoutText = subsystemTextOutput(
        subWorkflow,
        internalResult,
        [](const NodeExecutionResult& result) { return result.stdoutText; });
    nodeResult.stderrText = subsystemTextOutput(
        subWorkflow,
        internalResult,
        [](const NodeExecutionResult& result) { return result.stderrText; });
    for (const auto& internalNodeResult : internalResult.nodeResults) {
        nodeResult.artifacts.append(internalNodeResult.artifacts);
    }
    if (!internalResult.success) {
        nodeResult.errorMessage = QStringLiteral("Subsystem workflow failed: %1")
            .arg(internalResult.errors.join("; "));
    }
    return nodeResult;
}

bool SubsystemNodeExecutor::isCancelRequested() const
{
    return m_cancelPredicate && m_cancelPredicate();
}

} // namespace vws::execution
