#pragma once

class QAction;
class QWidget;

namespace vws::ui {

class CommandBar;
class EmptyStateOverlay;
class CanvasHeader;
class NodeInspector;
class OutputPanel;
class WorkflowCanvas;
class WorkspaceExplorer;

struct MainWindowLayoutActions {
    QAction* newWorkspace = nullptr;
    QAction* openWorkspace = nullptr;
    QAction* selectPython = nullptr;
    QAction* newWorkflow = nullptr;
    QAction* saveWorkflow = nullptr;
    QAction* saveTemplate = nullptr;
    QAction* connectNodes = nullptr;
    QAction* importTemplate = nullptr;
    QAction* generateWorkflowByLlm = nullptr;
    QAction* runWorkflow = nullptr;
    QAction* cancelRun = nullptr;
    QAction* toggleTheme = nullptr;
};

struct MainWindowLayout {
    QWidget* centralWidget = nullptr;
    CommandBar* commandBar = nullptr;
    CanvasHeader* canvasHeader = nullptr;
    WorkspaceExplorer* workspaceExplorer = nullptr;
    WorkflowCanvas* workflowCanvas = nullptr;
    NodeInspector* nodeInspector = nullptr;
    OutputPanel* outputPanel = nullptr;
    EmptyStateOverlay* canvasOverlay = nullptr;
};

class MainWindowLayoutBuilder final {
public:
    explicit MainWindowLayoutBuilder(QWidget* owner);

    MainWindowLayout build(const MainWindowLayoutActions& actions) const;

private:
    CommandBar* buildCommandBar(const MainWindowLayoutActions& actions) const;
    QWidget* buildCanvasHost(CanvasHeader* canvasHeader, WorkflowCanvas* workflowCanvas, EmptyStateOverlay* canvasOverlay) const;

    QWidget* m_owner = nullptr;
};

} // namespace vws::ui
