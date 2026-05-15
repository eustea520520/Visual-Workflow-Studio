#pragma once

#include "domain/RunRecord.h"
#include "domain/Workflow.h"
#include "domain/Workspace.h"

#include <QAction>
#include <QHash>
#include <QMainWindow>
#include <QPointF>
#include <QJsonObject>

class QLabel;
class QWidget;

namespace vws {

class AppContext;

namespace presentation {
class AppStore;
}

namespace ui {
class CommandBar;
class NodeInspector;
class OutputPanel;
class ThemeManager;
class WorkflowCanvas;
class WorkspaceExplorer;
class EmptyStateOverlay;
}

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(AppContext& appContext, QWidget* parent = nullptr);

private:
    void buildActions();
    void buildLayout();
    void renderCurrentWorkflowOnCanvas();
    void clearCanvasWorkflowView();
    void updateCanvasOverlay();
    void applyInitialTheme();
    void createWorkspace();
    void openWorkspace();
    void selectPythonInterpreter();
    void createWorkflow();
    void loadWorkflow();
    void openWorkflowById(const QString& workflowId);
    void openRunById(const QString& runId);
    void restoreRunRecordToUi(
        const domain::RunRecord& record,
        const domain::Workflow& workflowSnapshot,
        const QHash<QString, QJsonObject>& nodeOutputsByNodeId);
    void saveWorkflow();
    void saveSelectedNodeAsTemplate();
    void addNodeFromTemplate();
    void connectSelectedNodes();
    void importNodeTemplate();
    void runCurrentWorkflow();
    void cancelCurrentWorkflowRun();
    void openPythonNodeEditor(const domain::Node& node);
    void addNodeFromTemplateIdAt(const QString& templateId, const QPointF& scenePos);
    void resetInspectorAndOutput();
    void applyCachedNodeStatusesForWorkflow(const QString& workflowId);
    void refreshWorkspaceExplorer();
    void applyWorkspacePythonExecutable();
    void updatePythonStatus();
    void updateSelectedNodeStatus(const domain::Node& node);
    bool ensureWorkspaceOpen();
    bool ensureWorkflowOpen();

    AppContext& m_appContext;
    presentation::AppStore& m_store;
    ui::ThemeManager* m_themeManager = nullptr;
    ui::CommandBar* m_commandBar = nullptr;
    ui::WorkspaceExplorer* m_workspaceExplorer = nullptr;
    ui::WorkflowCanvas* m_workflowCanvas = nullptr;
    ui::NodeInspector* m_nodeInspector = nullptr;
    ui::OutputPanel* m_outputPanel = nullptr;
    ui::EmptyStateOverlay* m_canvasOverlay = nullptr;
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
    bool m_renderingWorkflow = false;
};

} // namespace vws
