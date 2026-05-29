#include "ui/main/MainWindowUiCoordinator.h"

#include "presentation/models/WorkflowDisplayModel.h"
#include "presentation/state/AppStore.h"
#include "ui/canvas/CanvasHeader.h"
#include "ui/canvas/WorkflowCanvas.h"
#include "ui/inspector/NodeInspector.h"
#include "ui/output/OutputPanel.h"
#include "ui/workspace/WorkspaceExplorer.h"

#include <QDir>
#include <QLabel>

namespace vws::ui {

MainWindowUiCoordinator::MainWindowUiCoordinator(
    presentation::AppStore& store,
    const MainWindowUiParts& parts)
    : m_store(store)
    , m_parts(parts)
{
}

void MainWindowUiCoordinator::setAdvancedDiagnosticsEnabled(bool enabled)
{
    if (m_parts.outputPanel != nullptr) {
        m_parts.outputPanel->setAdvancedDiagnosticsEnabled(enabled);
    }
}

void MainWindowUiCoordinator::renderWorkspaceExplorer(const WorkspaceExplorerViewModel& viewModel)
{
    if (m_parts.workspaceExplorer != nullptr) {
        m_parts.workspaceExplorer->render(viewModel);
    }
}

void MainWindowUiCoordinator::renderCanvasBreadcrumb(const CanvasBreadcrumbViewModel& viewModel)
{
    if (m_parts.canvasHeader != nullptr) {
        m_parts.canvasHeader->render(viewModel);
    }
}

void MainWindowUiCoordinator::renderCanvasOverlay(EmptyStateOverlay::Mode mode)
{
    if (m_parts.canvasOverlay != nullptr) {
        m_parts.canvasOverlay->render(mode);
    }
}

void MainWindowUiCoordinator::refreshCanvasTheme()
{
    if (m_parts.workflowCanvas != nullptr) {
        m_parts.workflowCanvas->refreshTheme();
    }
}

void MainWindowUiCoordinator::setPythonExecutableStatus(const QString& pythonExecutable)
{
    if (m_parts.pythonStatusLabel == nullptr) {
        return;
    }

    const auto normalized = pythonExecutable.trimmed();
    m_parts.pythonStatusLabel->setText(normalized.isEmpty()
            ? QObject::tr("Python: not selected")
            : QObject::tr("Python: %1").arg(QDir::toNativeSeparators(normalized)));
}

void MainWindowUiCoordinator::selectNode(const domain::Node& node)
{
    m_store.selectedNodeId() = node.nodeId;
    if (m_parts.nodeInspector != nullptr) {
        m_parts.nodeInspector->displayNode(node, m_store.nodeOutputsByNodeId().value(node.nodeId));
    }
    updateTimeoutStatus(&node);
}

void MainWindowUiCoordinator::clearNodeSelection()
{
    m_store.clearSelection();
    if (m_parts.nodeInspector != nullptr) {
        m_parts.nodeInspector->clear();
    }
    updateTimeoutStatus(nullptr);
}

void MainWindowUiCoordinator::refreshSelectedNode()
{
    const auto node = selectedNodeSnapshot();
    if (!node.has_value()) {
        return;
    }

    if (m_parts.nodeInspector != nullptr) {
        m_parts.nodeInspector->displayNode(node.value(), m_store.nodeOutputsByNodeId().value(node->nodeId));
    }
    updateTimeoutStatus(&node.value());
}

void MainWindowUiCoordinator::resetForWorkflowChange()
{
    clearNodeSelection();
    m_store.clearNodeOutputs();
    if (m_parts.outputPanel != nullptr) {
        m_parts.outputPanel->clearRun();
    }
}

void MainWindowUiCoordinator::resetForWorkspaceChange()
{
    resetForWorkflowChange();
}

void MainWindowUiCoordinator::beginRun(const domain::Workflow& workflow)
{
    if (m_parts.outputPanel != nullptr) {
        m_parts.outputPanel->clearRun();
        m_parts.outputPanel->render(outputViewModelFor(workflow));
    }

    if (!m_store.selectedNodeId().isEmpty() && m_parts.nodeInspector != nullptr) {
        const auto node = selectedNodeSnapshot();
        if (node.has_value()) {
            m_parts.nodeInspector->displayNode(node.value(), {});
        }
    }
}

void MainWindowUiCoordinator::finishRun(const execution::WorkflowExecutionResult& result)
{
    if (m_parts.outputPanel != nullptr) {
        m_parts.outputPanel->showExecutionResult(result);
    }

    for (auto it = result.nodeResults.cbegin(); it != result.nodeResults.cend(); ++it) {
        m_store.nodeOutputsByNodeId().insert(it.key(), it.value().outputs);
    }
    refreshSelectedNode();
}

void MainWindowUiCoordinator::restoreRunRecord(
    const domain::RunRecord& record,
    const domain::Workflow& workflowSnapshot,
    const QHash<QString, QJsonObject>& nodeOutputsByNodeId)
{
    if (m_parts.outputPanel != nullptr) {
        m_parts.outputPanel->clearRun();
    }

    m_store.clearNodeOutputs();
    m_store.nodeOutputsByNodeId() = nodeOutputsByNodeId;

    if (m_parts.outputPanel != nullptr) {
        m_parts.outputPanel->render(outputViewModelFor(workflowSnapshot));
    }

    if (m_parts.workflowCanvas != nullptr) {
        for (const auto& nodeRun : record.nodeRuns) {
            m_parts.workflowCanvas->setNodeStatus(nodeRun.nodeId, nodeRun.status);
        }
    }

    if (m_parts.outputPanel != nullptr) {
        m_parts.outputPanel->showRunRecord(record, m_store.nodeOutputsByNodeId());
    }
    refreshSelectedNode();
}

void MainWindowUiCoordinator::handleWorkflowStatus(
    const QString& runId,
    const QString& workflowId,
    const QString& status)
{
    if (!belongsToVisibleOrActiveRun(workflowId) || m_parts.outputPanel == nullptr) {
        return;
    }

    m_parts.outputPanel->recordWorkflowStatus(runId, status);
}

void MainWindowUiCoordinator::handleNodeStatus(
    const QString& runId,
    const QString& workflowId,
    const QString& nodeId,
    const QString& status)
{
    if (!belongsToVisibleOrActiveRun(workflowId)) {
        return;
    }

    if (m_store.currentWorkflow().workflowId == workflowId && m_parts.workflowCanvas != nullptr) {
        m_parts.workflowCanvas->setNodeStatus(nodeId, status);
    }
    if (m_parts.outputPanel != nullptr) {
        m_parts.outputPanel->recordNodeStatus(runId, nodeId, status);
    }
}

void MainWindowUiCoordinator::handleNodeOutput(
    const QString& runId,
    const QString& workflowId,
    const QString& nodeId,
    const QJsonObject& outputs)
{
    if (!belongsToVisibleOrActiveRun(workflowId)) {
        return;
    }

    m_store.nodeOutputsByNodeId().insert(nodeId, outputs);
    if (m_parts.outputPanel != nullptr) {
        m_parts.outputPanel->recordNodeOutput(runId, nodeId, outputs);
    }

    if (m_store.selectedNodeId() == nodeId) {
        refreshSelectedNode();
    }
}

void MainWindowUiCoordinator::handleNodeDebug(
    const QString& runId,
    const QString& workflowId,
    const QString& nodeId,
    const QString& text)
{
    if (!belongsToVisibleOrActiveRun(workflowId) || m_parts.outputPanel == nullptr) {
        return;
    }

    m_parts.outputPanel->recordNodeDebugOutput(runId, nodeId, text);
}

void MainWindowUiCoordinator::handleNodeError(
    const QString& runId,
    const QString& workflowId,
    const QString& nodeId,
    const QString& message)
{
    if (!belongsToVisibleOrActiveRun(workflowId) || m_parts.outputPanel == nullptr) {
        return;
    }

    m_parts.outputPanel->recordNodeError(runId, nodeId, message);
}

void MainWindowUiCoordinator::handleThreadTrace(
    const QString& runId,
    const QString& workflowId,
    const QString& nodeId,
    const QString& phase,
    const QString& threadId,
    const QString& threadName)
{
    if (!belongsToVisibleOrActiveRun(workflowId) || m_parts.outputPanel == nullptr) {
        return;
    }

    m_parts.outputPanel->recordThreadTrace(runId, nodeId, phase, threadId, threadName);
}

void MainWindowUiCoordinator::applyCachedNodeStatuses(const QString& workflowId)
{
    if (workflowId.trimmed().isEmpty() || m_parts.workflowCanvas == nullptr) {
        return;
    }

    const auto statuses = m_store.nodeStatusesByWorkflowId().value(workflowId);
    for (auto it = statuses.cbegin(); it != statuses.cend(); ++it) {
        m_parts.workflowCanvas->setNodeStatus(it.key(), it.value());
    }
}

void MainWindowUiCoordinator::appendLog(const QString& text)
{
    if (m_parts.outputPanel != nullptr) {
        m_parts.outputPanel->appendStdout(text);
    }
}

void MainWindowUiCoordinator::appendError(const QString& text)
{
    if (m_parts.outputPanel != nullptr) {
        m_parts.outputPanel->appendStderr(text);
    }
}

bool MainWindowUiCoordinator::belongsToVisibleOrActiveRun(const QString& workflowId) const
{
    return m_store.currentWorkflow().workflowId == workflowId
        || (!m_store.activeRunWorkflowId().isEmpty() && m_store.activeRunWorkflowId() == workflowId);
}

OutputPanelViewModel MainWindowUiCoordinator::outputViewModelFor(const domain::Workflow& workflow) const
{
    const auto displayModel = presentation::WorkflowDisplayModelBuilder::build(workflow);
    return {displayModel.workflowName, displayModel.nodeNamesById};
}

std::optional<domain::Node> MainWindowUiCoordinator::selectedNodeSnapshot() const
{
    const auto selectedNodeId = m_store.selectedNodeId().trimmed();
    if (selectedNodeId.isEmpty()) {
        return std::nullopt;
    }

    for (const auto& node : m_store.currentWorkflow().nodes) {
        if (node.nodeId == selectedNodeId) {
            return node;
        }
    }

    if (m_parts.workflowCanvas != nullptr) {
        const auto canvasNode = m_parts.workflowCanvas->selectedNode();
        if (canvasNode.has_value() && canvasNode->nodeId == selectedNodeId) {
            return canvasNode.value();
        }
    }

    return std::nullopt;
}

void MainWindowUiCoordinator::updateTimeoutStatus(const domain::Node* node)
{
    if (m_parts.timeoutStatusLabel == nullptr) {
        return;
    }

    m_parts.timeoutStatusLabel->setText(node == nullptr
            ? QObject::tr("Timeout: -")
            : QObject::tr("Timeout: %1 ms").arg(node->runtime.timeoutMs));
}

} // namespace vws::ui
