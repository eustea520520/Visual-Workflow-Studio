#pragma once

#include "domain/Workflow.h"
#include "domain/Workspace.h"

#include <QMainWindow>

class QLabel;
class QPushButton;
class QWidget;

namespace vws {

class AppContext;

namespace ui {
class NodeInspector;
class OutputPanel;
class WorkflowCanvas;
class WorkspaceExplorer;
}

// MainWindow 是主窗口壳子，只负责菜单、工具栏和主布局。
// 具体面板拆到 src/ui/*，避免 MainWindow 变成几千行的“万能类”。
class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(AppContext& appContext, QWidget* parent = nullptr);

private:
    void buildActions();
    void buildLayout();
    QWidget* buildCanvasOverlay();
    void updateCanvasOverlay();
    void createWorkspace();
    void openWorkspace();
    void selectPythonInterpreter();
    void createWorkflow();
    void loadWorkflow();
    void openWorkflowById(const QString& workflowId);
    void saveWorkflow();
    void saveSelectedNodeAsTemplate();
    void addNodeFromTemplate();
    void connectSelectedNodes();
    void importNodeTemplate();
    void runCurrentWorkflow();
    void cancelCurrentWorkflowRun();
    void openPythonNodeEditor(const domain::Node& node);
    void refreshWorkspaceExplorer();
    void applyWorkspacePythonExecutable();
    void updatePythonStatus();
    void updateSelectedNodeStatus(const domain::Node& node);
    bool ensureWorkspaceOpen();
    bool ensureWorkflowOpen();

    AppContext& m_appContext;
    domain::Workspace m_currentWorkspace;
    domain::Workflow m_currentWorkflow;
    ui::WorkspaceExplorer* m_workspaceExplorer = nullptr;
    ui::WorkflowCanvas* m_workflowCanvas = nullptr;
    ui::NodeInspector* m_nodeInspector = nullptr;
    ui::OutputPanel* m_outputPanel = nullptr;
    QWidget* m_canvasOverlay = nullptr;
    QLabel* m_canvasOverlayTitle = nullptr;
    QPushButton* m_overlayPrimaryButton = nullptr;
    QPushButton* m_overlaySecondaryButton = nullptr;
    QLabel* m_timeoutStatusLabel = nullptr;
    QLabel* m_pythonStatusLabel = nullptr;
    bool m_workflowRunning = false;
};

} // namespace vws
