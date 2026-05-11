#pragma once

#include "domain/Workflow.h"
#include "domain/Workspace.h"

#include <QAction>
#include <QMainWindow>

class QLabel;
class QPushButton;
class QWidget;

namespace vws {

class AppContext;

namespace ui {
class CommandBar;
class NodeInspector;
class OutputPanel;
class ThemeManager;
class WorkflowCanvas;
class WorkspaceExplorer;
}

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(AppContext& appContext, QWidget* parent = nullptr);

private:
    void buildActions();
    void buildLayout();
    void buildCommandBar();
    QWidget* buildCanvasOverlay();
    void updateCanvasOverlay();
    void applyInitialTheme();
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
    ui::ThemeManager* m_themeManager = nullptr;
    ui::CommandBar* m_commandBar = nullptr;
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
    QAction* m_newWorkspaceAction = nullptr;
    QAction* m_openWorkspaceAction = nullptr;
    QAction* m_selectPythonAction = nullptr;
    QAction* m_newWorkflowAction = nullptr;
    QAction* m_saveWorkflowAction = nullptr;
    QAction* m_saveTemplateAction = nullptr;
    QAction* m_connectNodesAction = nullptr;
    QAction* m_importTemplateAction = nullptr;
    QAction* m_runAction = nullptr;
    QAction* m_cancelRunAction = nullptr;
    QAction* m_toggleThemeAction = nullptr;
    bool m_workflowRunning = false;
};

} // namespace vws
