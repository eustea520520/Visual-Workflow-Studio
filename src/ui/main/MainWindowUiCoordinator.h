#pragma once

#include "domain/RunRecord.h"
#include "domain/Workflow.h"
#include "execution/WorkflowExecutionResult.h"
#include "ui/canvas/CanvasBreadcrumbViewModel.h"
#include "ui/output/OutputPanelViewModel.h"
#include "ui/widgets/EmptyStateOverlay.h"
#include "ui/workspace/WorkspaceExplorerViewModel.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <optional>

class QLabel;

namespace vws::presentation {
class AppStore;
}

namespace vws::ui {

class CanvasHeader;
class NodeInspector;
class OutputPanel;
class WorkflowCanvas;
class WorkspaceExplorer;

struct MainWindowUiParts {
    CanvasHeader* canvasHeader = nullptr;
    WorkspaceExplorer* workspaceExplorer = nullptr;
    WorkflowCanvas* workflowCanvas = nullptr;
    NodeInspector* nodeInspector = nullptr;
    OutputPanel* outputPanel = nullptr;
    EmptyStateOverlay* canvasOverlay = nullptr;
    QLabel* timeoutStatusLabel = nullptr;
    QLabel* pythonStatusLabel = nullptr;
};

// MainWindowUiCoordinator 是主窗口 UI 的单一刷新入口。
// MainWindow 负责业务命令；这里负责把 AppStore 中的状态稳定地渲染到各个 QWidget。
class MainWindowUiCoordinator final {
public:
    MainWindowUiCoordinator(presentation::AppStore& store, const MainWindowUiParts& parts);

    void setAdvancedDiagnosticsEnabled(bool enabled);
    void renderWorkspaceExplorer(const WorkspaceExplorerViewModel& viewModel);
    void renderCanvasBreadcrumb(const CanvasBreadcrumbViewModel& viewModel);
    void renderCanvasOverlay(EmptyStateOverlay::Mode mode);
    void refreshCanvasTheme();
    void setPythonExecutableStatus(const QString& pythonExecutable);

    void selectNode(const domain::Node& node);
    void clearNodeSelection();
    void refreshSelectedNode();
    void resetForWorkflowChange();
    void resetForWorkspaceChange();

    void beginRun(const domain::Workflow& workflow);
    void finishRun(const execution::WorkflowExecutionResult& result);
    void restoreRunRecord(
        const domain::RunRecord& record,
        const domain::Workflow& workflowSnapshot,
        const QHash<QString, QJsonObject>& nodeOutputsByNodeId);

    void handleWorkflowStatus(const QString& runId, const QString& workflowId, const QString& status);
    void handleNodeStatus(const QString& runId, const QString& workflowId, const QString& nodeId, const QString& status);
    void handleNodeOutput(
        const QString& runId,
        const QString& workflowId,
        const QString& nodeId,
        const QJsonObject& outputs);
    void handleNodeDebug(
        const QString& runId,
        const QString& workflowId,
        const QString& nodeId,
        const QString& text);
    void handleNodeError(const QString& runId, const QString& workflowId, const QString& nodeId, const QString& message);
    void handleThreadTrace(
        const QString& runId,
        const QString& workflowId,
        const QString& nodeId,
        const QString& phase,
        const QString& threadId,
        const QString& threadName);

    void applyCachedNodeStatuses(const QString& workflowId);
    void appendLog(const QString& text);
    void appendError(const QString& text);

private:
    bool belongsToVisibleOrActiveRun(const QString& workflowId) const;
    OutputPanelViewModel outputViewModelFor(const domain::Workflow& workflow) const;
    std::optional<domain::Node> selectedNodeSnapshot() const;
    void updateTimeoutStatus(const domain::Node* node);

    presentation::AppStore& m_store;
    MainWindowUiParts m_parts;
};

} // namespace vws::ui
