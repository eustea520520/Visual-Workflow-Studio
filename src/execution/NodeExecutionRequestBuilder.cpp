#include "execution/NodeExecutionRequestBuilder.h"

#include "execution/InputMerger.h"

#include <utility>

namespace vws::execution {

NodeExecutionRequestBuilder::NodeExecutionRequestBuilder(
    QString runId,
    QString workspacePath,
    QString runPath,
    QString artifactPath)
    : m_runId(std::move(runId))
    , m_workspacePath(std::move(workspacePath))
    , m_runPath(std::move(runPath))
    , m_artifactPath(std::move(artifactPath))
{
}

NodeExecutionRequest NodeExecutionRequestBuilder::build(
    const QString& nodeId,
    const GraphIndexes& indexes,
    const QHash<QString, DataPacket>& completedEdgeData,
    const InputMerger& inputMerger) const
{
    const auto node = indexes.nodesById.value(nodeId);

    NodeExecutionRequest request;
    request.runId = m_runId;
    request.nodeId = node.nodeId;
    request.nodeType = node.type;
    request.nodeConfig = node.config;
    request.inputs = inputMerger.buildInputs(nodeId, indexes, completedEdgeData);
    request.workspacePath = m_workspacePath;
    request.runPath = m_runPath;
    request.artifactPath = m_artifactPath;
    request.timeoutMs = node.runtime.timeoutMs;
    return request;
}

} // namespace vws::execution
