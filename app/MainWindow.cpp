#include "MainWindow.h"

#include "AppContext.h"
#include "application/NodeFactory.h"
#include "application/PythonCodeTemplates.h"
#include "application/WorkflowHistory.h"
#include "application/WorkflowService.h"
#include "application/subsystem/SubsystemService.h"
#include "domain/NodeTemplate.h"
#include "domain/NodeConfigView.h"
#include "domain/NodeTypes.h"
#include "presentation/controllers/NodeTemplateController.h"
#include "presentation/controllers/PythonEnvironmentController.h"
#include "presentation/controllers/RunController.h"
#include "presentation/controllers/CanvasSessionController.h"
#include "presentation/controllers/CanvasNavigationController.h"
#include "presentation/controllers/WorkspaceBrowserController.h"
#include "presentation/controllers/WorkflowGenerationController.h"
#include "presentation/controllers/WorkflowIoController.h"
#include "presentation/controllers/WorkflowController.h"
#include "presentation/controllers/WorkspaceController.h"
#include "presentation/state/AppStore.h"
#include "ui/canvas/WorkflowCanvas.h"
#include "ui/canvas/CanvasHeader.h"
#include "ui/editor/PythonNodeEditorDialog.h"
#include "ui/generation/WorkflowGenerationDialog.h"
#include "ui/inspector/NodeInspector.h"
#include "ui/main/MainWindowLayoutBuilder.h"
#include "ui/main/MainWindowUiCoordinator.h"
#include "ui/output/OutputPanel.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/CommandBar.h"
#include "ui/widgets/EmptyStateOverlay.h"
#include "ui/workspace/WorkspaceExplorer.h"
#include "ui/workspace/WorkspaceExplorerViewModel.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHash>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScopedValueRollback>
#include <QStatusBar>
#include <QStyleHints>
#include <QTimer>
#include <QVBoxLayout>

namespace vws {

namespace NodeTypes = domain::NodeTypes;

namespace {

application::NodeFactory::StarterTemplateKind toApplicationStarterTemplate(ui::StarterNodeTemplate templateKind)
{
    switch (templateKind) {
    case ui::StarterNodeTemplate::EmptyOutput:
        return application::NodeFactory::StarterTemplateKind::EmptyOutput;
    case ui::StarterNodeTemplate::FileOutput:
        return application::NodeFactory::StarterTemplateKind::FileOutput;
    case ui::StarterNodeTemplate::DataOutput:
    default:
        return application::NodeFactory::StarterTemplateKind::DataOutput;
    }
}

application::DataTransferTemplate toApplicationDataTransferTemplate(ui::DataTransferNodeTemplate templateKind)
{
    switch (templateKind) {
    case ui::DataTransferNodeTemplate::DataToFile:
        return application::DataTransferTemplate::DataToFile;
    case ui::DataTransferNodeTemplate::FileToData:
        return application::DataTransferTemplate::FileToData;
    case ui::DataTransferNodeTemplate::FileToFile:
        return application::DataTransferTemplate::FileToFile;
    case ui::DataTransferNodeTemplate::DataToData:
    default:
        return application::DataTransferTemplate::DataToData;
    }
}

} // namespace

MainWindow::MainWindow(AppContext& appContext, QWidget* parent)
    : QMainWindow(parent)
    , m_appContext(appContext)
    , m_store(appContext.appStore())
{
    setWindowTitle("Visual Workflow Studio");
    setMinimumSize(1280, 760);
    resize(1440, 900);
    buildActions();
    buildLayout();
    applyInitialTheme();
    updateCanvasOverlay();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildActions()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* workflowMenu = menuBar()->addMenu(tr("&Workflow"));
    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    auto* advancedSettingsMenu = menuBar()->addMenu(tr("Advanced Settings"));

    m_newWorkspaceAction = fileMenu->addAction(tr("New Workspace"));
    m_openWorkspaceAction = fileMenu->addAction(tr("Open Workspace"));
    m_selectPythonAction = fileMenu->addAction(tr("Select Python Interpreter"));
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction(tr("Exit"));

    m_newWorkflowAction = workflowMenu->addAction(tr("New Workflow"));
    auto* loadWorkflowAction = workflowMenu->addAction(tr("Load Workflow"));
    m_saveWorkflowAction = workflowMenu->addAction(tr("Save Workflow"));
    workflowMenu->addSeparator();
    m_saveTemplateAction = workflowMenu->addAction(tr("Save Selected Node As Template"));
    auto* addNodeFromTemplateAction = workflowMenu->addAction(tr("Add Node From Template"));
    m_connectNodesAction = workflowMenu->addAction(tr("Connect Selected Nodes"));
    m_importTemplateAction = workflowMenu->addAction(tr("Import Node Template"));
    m_generateWorkflowByLlmAction = workflowMenu->addAction(tr("Generate Workflow by LLM"));
    workflowMenu->addSeparator();
    m_runAction = workflowMenu->addAction(tr("Run Workflow"));
    m_cancelRunAction = workflowMenu->addAction(tr("Cancel Run"));

    m_toggleThemeAction = viewMenu->addAction(tr("Toggle Light / Dark Theme"));
    m_advancedDiagnosticsAction = advancedSettingsMenu->addAction(tr("Enable Advanced Output Diagnostics"));
    m_advancedDiagnosticsAction->setCheckable(true);
    m_advancedDiagnosticsAction->setChecked(false);
    m_animateNodeStatusAction = advancedSettingsMenu->addAction(tr("Animate Node Status Step by Step"));
    m_animateNodeStatusAction->setCheckable(true);
    m_animateNodeStatusAction->setChecked(true);
    m_setNodeDispatchDelayAction = advancedSettingsMenu->addAction(tr("Set Node Step Delay..."));

    connect(m_newWorkspaceAction, &QAction::triggered, this, &MainWindow::createWorkspace);
    connect(m_openWorkspaceAction, &QAction::triggered, this, &MainWindow::openWorkspace);
    connect(m_selectPythonAction, &QAction::triggered, this, &MainWindow::selectPythonInterpreter);
    connect(m_newWorkflowAction, &QAction::triggered, this, &MainWindow::createWorkflow);
    connect(loadWorkflowAction, &QAction::triggered, this, &MainWindow::loadWorkflow);
    connect(m_saveWorkflowAction, &QAction::triggered, this, &MainWindow::saveWorkflow);
    connect(m_saveTemplateAction, &QAction::triggered, this, &MainWindow::saveSelectedNodeAsTemplate);
    connect(addNodeFromTemplateAction, &QAction::triggered, this, &MainWindow::addNodeFromTemplate);
    connect(m_connectNodesAction, &QAction::triggered, this, &MainWindow::connectSelectedNodes);
    connect(m_importTemplateAction, &QAction::triggered, this, &MainWindow::importNodeTemplate);
    connect(m_generateWorkflowByLlmAction, &QAction::triggered, this, &MainWindow::openWorkflowGenerationDialog);
    connect(m_runAction, &QAction::triggered, this, &MainWindow::runCurrentWorkflow);
    connect(m_cancelRunAction, &QAction::triggered, this, &MainWindow::cancelCurrentWorkflowRun);
    connect(m_advancedDiagnosticsAction, &QAction::toggled, this, [this](bool enabled) {
        if (m_uiCoordinator != nullptr) {
            m_uiCoordinator->setAdvancedDiagnosticsEnabled(enabled);
        }
    });
    connect(m_setNodeDispatchDelayAction, &QAction::triggered, this, &MainWindow::configureNodeDispatchDelay);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
}

void MainWindow::buildLayout()
{
    // ThemeManager owns the application-level stylesheet and color tokens.
    m_themeManager = new ui::ThemeManager(this);
    ui::ThemeManager::setInstance(m_themeManager);
    connect(m_toggleThemeAction, &QAction::triggered, m_themeManager, &ui::ThemeManager::toggleTheme);

    const ui::MainWindowLayoutBuilder layoutBuilder(this);
    const auto layout = layoutBuilder.build({
        m_newWorkspaceAction,
        m_openWorkspaceAction,
        m_selectPythonAction,
        m_newWorkflowAction,
        m_saveWorkflowAction,
        m_saveTemplateAction,
        m_connectNodesAction,
        m_importTemplateAction,
        m_generateWorkflowByLlmAction,
        m_runAction,
        m_cancelRunAction,
        m_toggleThemeAction,
    });
    m_commandBar = layout.commandBar;
    m_canvasHeader = layout.canvasHeader;
    m_workspaceExplorer = layout.workspaceExplorer;
    m_workflowCanvas = layout.workflowCanvas;
    m_nodeInspector = layout.nodeInspector;
    m_outputPanel = layout.outputPanel;
    m_canvasOverlay = layout.canvasOverlay;
    setCentralWidget(layout.centralWidget);

    // Status bar shows selected-node runtime info and the workspace Python interpreter.
    m_timeoutStatusLabel = new QLabel(tr("Timeout: -"), this);
    m_timeoutStatusLabel->setObjectName("timeoutStatus");
    m_pythonStatusLabel = new QLabel(tr("Python: not selected"), this);
    m_pythonStatusLabel->setObjectName("pythonStatus");
    statusBar()->addPermanentWidget(m_timeoutStatusLabel);
    statusBar()->addPermanentWidget(m_pythonStatusLabel, 1);

    m_uiCoordinator = std::make_unique<ui::MainWindowUiCoordinator>(m_store, ui::MainWindowUiParts{
        m_canvasHeader,
        m_workspaceExplorer,
        m_workflowCanvas,
        m_nodeInspector,
        m_outputPanel,
        m_canvasOverlay,
        m_timeoutStatusLabel,
        m_pythonStatusLabel,
    });
    m_uiCoordinator->setAdvancedDiagnosticsEnabled(
        m_advancedDiagnosticsAction != nullptr && m_advancedDiagnosticsAction->isChecked());

    // UI widgets emit intent; application services and the execution engine do the work.
    connect(m_workflowCanvas, &ui::WorkflowCanvas::nodeSelected, this, [this](const domain::Node& node) {
        m_uiCoordinator->selectNode(node);
    });
    connect(m_workflowCanvas, &ui::WorkflowCanvas::nodeSelectionCleared, this, [this]() {
        m_uiCoordinator->clearNodeSelection();
    });
    connect(m_canvasHeader, &ui::CanvasHeader::breadcrumbClicked, this, &MainWindow::navigateCanvasBreadcrumb);
    connect(m_workflowCanvas, &ui::WorkflowCanvas::nodeDoubleClicked, this, [this](const domain::Node& node) {
        if (m_appContext.subsystemService().isSubsystemNode(node)) {
            // Entering a subsystem rebuilds the QGraphicsScene. Defer it until
            // the double-click event has fully returned so the clicked item is
            // not deleted while Qt is still dispatching its mouse event.
            QTimer::singleShot(0, this, [this, node]() {
                enterSubsystemNode(node);
            });
            return;
        }
        openPythonNodeEditor(node);
    });
    connect(m_workflowCanvas, &ui::WorkflowCanvas::saveRequested, this, &MainWindow::saveWorkflow);
    connect(m_workflowCanvas, &ui::WorkflowCanvas::workflowChanged, this, [this](const domain::Workflow& workflow) {
        if (m_renderingWorkflow) {
            return;
        }
        m_appContext.canvasSessionController().syncCurrentView(workflow);
    });
    connect(m_workspaceExplorer, &ui::WorkspaceExplorer::workflowActivated, this, &MainWindow::openWorkflowById);
    connect(m_workspaceExplorer, &ui::WorkspaceExplorer::workflowDeleteRequested, this, &MainWindow::deleteWorkflowById);
    connect(m_workspaceExplorer, &ui::WorkspaceExplorer::workflowRenameRequested, this, &MainWindow::renameWorkflowById);
    connect(m_workspaceExplorer, &ui::WorkspaceExplorer::workflowAddAsSubsystemRequested, this, &MainWindow::addWorkflowAsSubsystemNode);
    connect(m_workspaceExplorer, &ui::WorkspaceExplorer::runActivated, this, &MainWindow::openRunById);
    connect(m_canvasOverlay, &ui::EmptyStateOverlay::createWorkspaceRequested, this, &MainWindow::createWorkspace);
    connect(m_canvasOverlay, &ui::EmptyStateOverlay::openWorkspaceRequested, this, &MainWindow::openWorkspace);
    connect(m_canvasOverlay, &ui::EmptyStateOverlay::createWorkflowRequested, this, &MainWindow::createWorkflow);
    connect(m_canvasOverlay, &ui::EmptyStateOverlay::openWorkflowRequested, this, &MainWindow::loadWorkflow);
    connect(m_workflowCanvas, &ui::WorkflowCanvas::starterNodeRequested, this,
        [this](const QPointF& scenePos, ui::StarterNodeTemplate templateKind) {
            m_workflowCanvas->addNode(application::NodeFactory::createStarterNode(
                scenePos,
                m_workflowCanvas->workflow().nodes.size(),
                toApplicationStarterTemplate(templateKind)));
        });
    connect(m_workflowCanvas, &ui::WorkflowCanvas::functionNodeRequested, this,
        [this](const QPointF& scenePos, ui::DataTransferNodeTemplate templateKind) {
            m_workflowCanvas->addNode(application::NodeFactory::createFunctionNode(
                scenePos,
                m_workflowCanvas->workflow().nodes.size(),
                toApplicationDataTransferTemplate(templateKind)));
        });
    connect(m_workflowCanvas, &ui::WorkflowCanvas::agentNodeRequested, this,
        [this](const QPointF& scenePos, ui::DataTransferNodeTemplate templateKind) {
            m_workflowCanvas->addNode(application::NodeFactory::createAgentNode(
                scenePos,
                m_workflowCanvas->workflow().nodes.size(),
                toApplicationDataTransferTemplate(templateKind)));
        });
    connect(m_workflowCanvas, &ui::WorkflowCanvas::subsystemNodeRequested, this,
        [this](const QPointF& scenePos) {
            if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
                return;
            }
            domain::NodePosition position;
            position.x = scenePos.x();
            position.y = scenePos.y();
            const auto name = tr("Subsystem Node %1").arg(m_workflowCanvas->workflow().nodes.size() + 1);
            m_workflowCanvas->addNode(m_appContext.subsystemService().createSubsystemNode(
                m_store.currentWorkspace().id,
                name,
                position));
        });
    connect(m_workflowCanvas, &ui::WorkflowCanvas::loopNodeRequested, this,
        [this](const QPointF& scenePos) {
            if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
                return;
            }
            m_workflowCanvas->addNode(application::NodeFactory::createLoopNode(
                scenePos,
                m_workflowCanvas->workflow().nodes.size(),
                1));
        });
    connect(m_workflowCanvas, &ui::WorkflowCanvas::subsystemNodeRetitleRequested,
        this, &MainWindow::retitleSubsystemNode);
    connect(m_workflowCanvas, &ui::WorkflowCanvas::nodeTemplateDropped,
        this, &MainWindow::addNodeFromTemplateIdAt);
    connect(&m_appContext.runController(), &presentation::RunController::nodeStatusChanged, this,
        [this](const QString& runId, const QString& workflowId, const QString& nodeId, const QString& status) {
            m_uiCoordinator->handleNodeStatus(runId, workflowId, nodeId, status);
        });
    connect(&m_appContext.runController(), &presentation::RunController::workflowStatusChanged, this,
        [this](const QString& runId, const QString& workflowId, const QString& status) {
            m_uiCoordinator->handleWorkflowStatus(runId, workflowId, status);
        });
    connect(&m_appContext.runController(), &presentation::RunController::nodeOutputReady, this,
        [this](const QString& runId, const QString& workflowId, const QString& nodeId, const QJsonObject& outputs) {
            m_uiCoordinator->handleNodeOutput(runId, workflowId, nodeId, outputs);
        });
    connect(&m_appContext.runController(), &presentation::RunController::nodeDebugOutputReady, this,
        [this](const QString& runId, const QString& workflowId, const QString& nodeId, const QString& text) {
            m_uiCoordinator->handleNodeDebug(runId, workflowId, nodeId, text);
        });
    connect(&m_appContext.runController(), &presentation::RunController::nodeError, this,
        [this](const QString& runId, const QString& workflowId, const QString& nodeId, const QString& message) {
            m_uiCoordinator->handleNodeError(runId, workflowId, nodeId, message);
        });
    connect(&m_appContext.runController(), &presentation::RunController::threadTrace, this,
        [this](const QString& runId, const QString& workflowId, const QString& nodeId, const QString& phase, const QString& threadId, const QString& threadName) {
            m_uiCoordinator->handleThreadTrace(runId, workflowId, nodeId, phase, threadId, threadName);
        });

    // Canvas items are painted manually, so they refresh when the theme changes.
    connect(m_themeManager, &ui::ThemeManager::themeChanged, this, [this](ui::AppTheme) {
        m_uiCoordinator->refreshCanvasTheme();
        updateCanvasOverlay();
    });

}

void MainWindow::applyInitialTheme()
{
    auto theme = ui::AppTheme::Light;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (qApp != nullptr
        && qApp->styleHints() != nullptr
        && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark) {
        theme = ui::AppTheme::Dark;
    } else
#endif
    {
        const auto windowColor = QApplication::palette().color(QPalette::Window);
        if (windowColor.isValid() && windowColor.lightness() < 128) {
            theme = ui::AppTheme::Dark;
        }
    }
    m_themeManager->applyTheme(theme);
}

void MainWindow::cacheCurrentWorkflowViewState()
{
    if (m_workflowCanvas == nullptr || m_store.currentWorkflow().workflowId.trimmed().isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_appContext.canvasSessionController().cacheCurrentView(
            m_workflowCanvas->workflow(),
            m_workflowCanvas->history(),
            &errorMessage)
        && m_uiCoordinator != nullptr) {
        m_uiCoordinator->appendError(errorMessage);
    }
}

void MainWindow::renderCurrentWorkflowOnCanvas()
{
    if (m_workflowCanvas == nullptr) {
        return;
    }

    application::WorkflowHistory history;
    if (m_store.workflowHistory(m_store.currentWorkflow().workflowId, &history)) {
        // Existing per-workflow history is restored below.
    }
    m_appContext.canvasSessionController().startRootSession(history);
    renderWorkflowOnCanvas(m_store.currentWorkflow(), history);
    updateCanvasBreadcrumb();
}

void MainWindow::renderWorkflowOnCanvas(const domain::Workflow& workflow, const application::WorkflowHistory& history)
{
    if (m_workflowCanvas == nullptr) {
        return;
    }

    QScopedValueRollback<bool> guard(m_renderingWorkflow, true);
    m_workflowCanvas->setWorkflow(workflow);
    m_workflowCanvas->setHistory(history);
    m_workflowCanvas->applyRuntimeIoSpecs(
        m_appContext.workflowIoController().visualSpecsForWorkflow(workflow));
}

void MainWindow::clearCanvasWorkflowView()
{
    if (m_workflowCanvas == nullptr) {
        return;
    }

    QScopedValueRollback<bool> guard(m_renderingWorkflow, true);
    m_workflowCanvas->clearWorkflow();
    m_appContext.canvasNavigationController().reset();
    updateCanvasBreadcrumb();
}

void MainWindow::updateCanvasOverlay()
{
    if (m_uiCoordinator == nullptr) {
        return;
    }

    if (m_store.currentWorkspace().rootPath.isEmpty()) {
        m_uiCoordinator->renderCanvasOverlay(ui::EmptyStateOverlay::Mode::NoWorkspace);
        updateCanvasBreadcrumb();
        return;
    }

    if (m_store.currentWorkflow().workflowId.isEmpty()) {
        m_uiCoordinator->renderCanvasOverlay(ui::EmptyStateOverlay::Mode::NoWorkflow);
        updateCanvasBreadcrumb();
        return;
    }

    m_uiCoordinator->renderCanvasOverlay(ui::EmptyStateOverlay::Mode::Hidden);
    updateCanvasBreadcrumb();
}

void MainWindow::updateCanvasBreadcrumb()
{
    if (m_uiCoordinator == nullptr) {
        return;
    }
    if (m_appContext.canvasNavigationController().hasRootWorkflow()) {
        m_uiCoordinator->renderCanvasBreadcrumb(m_appContext.canvasNavigationController().breadcrumbViewModel());
        return;
    }

    ui::CanvasBreadcrumbViewModel viewModel;
    ui::CanvasBreadcrumbItemViewModel workspaceItem;
    workspaceItem.label = m_store.currentWorkspace().name.trimmed().isEmpty()
        ? tr("No workspace")
        : m_store.currentWorkspace().name.trimmed();
    workspaceItem.clickable = false;
    viewModel.items.append(workspaceItem);
    if (m_store.currentWorkflow().workflowId.isEmpty()) {
        ui::CanvasBreadcrumbItemViewModel workflowItem;
        workflowItem.label = tr("No workflow");
        workflowItem.depth = 1;
        workflowItem.clickable = false;
        viewModel.items.append(workflowItem);
    }
    m_uiCoordinator->renderCanvasBreadcrumb(viewModel);
}

void MainWindow::enterSubsystemNode(const domain::Node& node)
{
    QString errorMessage;
    if (!m_appContext.canvasSessionController().enterSubsystem(
            m_workflowCanvas->workflow(),
            m_workflowCanvas->history(),
            node.nodeId,
            &errorMessage)) {
        QMessageBox::warning(this, tr("Subsystem"), errorMessage);
        return;
    }

    renderWorkflowOnCanvas(
        m_appContext.canvasNavigationController().currentWorkflow(),
        m_appContext.canvasNavigationController().currentHistory());
    applyCachedNodeStatusesForWorkflow(m_store.currentWorkflow().workflowId);
    resetInspectorView();
    updateCanvasOverlay();
}

void MainWindow::navigateCanvasBreadcrumb(int depth)
{
    QString errorMessage;
    if (!m_appContext.canvasSessionController().navigateToDepth(
            depth,
            m_workflowCanvas->workflow(),
            m_workflowCanvas->history(),
            &errorMessage)) {
        QMessageBox::warning(this, tr("Canvas Navigation"), errorMessage);
        return;
    }

    renderWorkflowOnCanvas(
        m_appContext.canvasNavigationController().currentWorkflow(),
        m_appContext.canvasNavigationController().currentHistory());
    applyCachedNodeStatusesForWorkflow(m_store.currentWorkflow().workflowId);
    resetInspectorView();
    updateCanvasOverlay();
}

void MainWindow::createWorkspace()
{
    const auto rootPath = QFileDialog::getExistingDirectory(this, tr("Select Workspace Directory"));
    if (rootPath.isEmpty()) {
        return;
    }

    const auto defaultName = QDir(rootPath).dirName().isEmpty() ? tr("Untitled Workspace") : QDir(rootPath).dirName();
    bool accepted = false;
    const auto workspaceName = QInputDialog::getText(
        this,
        tr("New Workspace"),
        tr("Workspace name"),
        QLineEdit::Normal,
        defaultName,
        &accepted);
    if (!accepted) {
        return;
    }

    QString errorMessage;
    if (!m_appContext.workspaceController().createWorkspace(rootPath, workspaceName, &errorMessage)) {
        QMessageBox::warning(this, tr("Workspace Error"), errorMessage);
        return;
    }

    clearCanvasWorkflowView();
    m_appContext.canvasNavigationController().reset();
    resetInspectorAndOutput();
    applyWorkspacePythonExecutable();
    refreshWorkspaceExplorer();
    updateCanvasOverlay();
    m_uiCoordinator->appendLog(tr("Created workspace: %1").arg(m_store.currentWorkspace().rootPath));
}

void MainWindow::openWorkspace()
{
    const auto rootPath = QFileDialog::getExistingDirectory(this, tr("Open Workspace Directory"));
    if (rootPath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_appContext.workspaceController().openWorkspace(rootPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Workspace Error"), errorMessage);
        return;
    }

    clearCanvasWorkflowView();
    m_appContext.canvasNavigationController().reset();
    resetInspectorAndOutput();
    applyWorkspacePythonExecutable();
    refreshWorkspaceExplorer();
    updateCanvasOverlay();
    m_uiCoordinator->appendLog(tr("Opened workspace: %1").arg(m_store.currentWorkspace().rootPath));
}

void MainWindow::selectPythonInterpreter()
{
    if (!ensureWorkspaceOpen()) {
        return;
    }

    const auto currentPath = m_appContext.pythonEnvironmentController().pythonExecutable();
    const auto selectedPath = QFileDialog::getOpenFileName(
        this,
        tr("Select Python Interpreter"),
        currentPath,
        tr("Python executable (python.exe);;All files (*.*)"));
    if (selectedPath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_appContext.pythonEnvironmentController().updatePythonExecutable(selectedPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Workspace Error"), errorMessage);
        return;
    }

    updatePythonStatus();
    m_uiCoordinator->appendLog(tr("Selected Python interpreter: %1").arg(selectedPath));
}

void MainWindow::createWorkflow()
{
    if (!ensureWorkspaceOpen()) {
        return;
    }

    bool accepted = false;
    const auto workflowName = QInputDialog::getText(
        this,
        tr("New Workflow"),
        tr("Workflow name"),
        QLineEdit::Normal,
        tr("Untitled Workflow"),
        &accepted);
    if (!accepted) {
        return;
    }

    cacheCurrentWorkflowViewState();
    m_appContext.workflowController().createWorkflow(workflowName);
    renderCurrentWorkflowOnCanvas();
    resetInspectorAndOutput();
    applyCachedNodeStatusesForWorkflow(m_store.currentWorkflow().workflowId);
    updateCanvasOverlay();

    saveWorkflow();
    m_uiCoordinator->appendLog(tr("Created workflow: %1").arg(m_store.currentWorkflow().name));
}

void MainWindow::loadWorkflow()
{
    if (!ensureWorkspaceOpen()) {
        return;
    }

    const auto filePath = QFileDialog::getOpenFileName(
        this,
        tr("Load Workflow"),
        QDir(m_store.currentWorkspace().rootPath).filePath("workflows"),
        tr("Workflow JSON (*.json);;All files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }

    cacheCurrentWorkflowViewState();
    QString errorMessage;
    if (!m_appContext.workflowController().loadWorkflowFile(filePath, &errorMessage)) {
        QMessageBox::warning(this, tr("Workflow Error"), errorMessage);
        return;
    }

    renderCurrentWorkflowOnCanvas();
    resetInspectorAndOutput();
    applyCachedNodeStatusesForWorkflow(m_store.currentWorkflow().workflowId);
    updateCanvasOverlay();
    m_uiCoordinator->appendLog(tr("Loaded workflow: %1").arg(m_store.currentWorkflow().name));
}

void MainWindow::openWorkflowGenerationDialog()
{
    if (!ensureWorkspaceOpen()) {
        return;
    }

    auto* dialog = new ui::WorkflowGenerationDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->render({
        QString(),
        QString(),
        m_appContext.workflowGenerationController().presetPrompt(),
        QString(),
        QString(),
        QString(),
        false,
    });

    QPointer<ui::WorkflowGenerationDialog> guardedDialog(dialog);

    connect(dialog, &ui::WorkflowGenerationDialog::copyPromptRequested, this,
        [this, guardedDialog](const QString& requirement) {
            const auto placeholder = requirement.trimmed().isEmpty()
                ? QStringLiteral("<Describe the workflow you want here>")
                : requirement;
            QApplication::clipboard()->setText(
                m_appContext.workflowGenerationController().presetPromptForCopy(placeholder));
            if (!guardedDialog.isNull()) {
                guardedDialog->setStatusMessage(tr("Prompt copied. You can paste it into another LLM."));
            }
        });

    connect(dialog, &ui::WorkflowGenerationDialog::importJsonRequested, this,
        [this, guardedDialog](const QString& jsonText) {
            cacheCurrentWorkflowViewState();
            presentation::WorkflowGenerationUiResult result;
            if (!m_appContext.workflowGenerationController().importGeneratedJson(jsonText, result)) {
                if (!guardedDialog.isNull()) {
                    guardedDialog->setStatusMessage(result.errorMessage, true);
                }
                return;
            }

            renderCurrentWorkflowOnCanvas();
            resetInspectorAndOutput();
            refreshWorkspaceExplorer();
            updateCanvasOverlay();
            if (!guardedDialog.isNull()) {
                guardedDialog->setGeneratedJson(result.rawJson);
                guardedDialog->setStatusMessage(tr("Imported workflow: %1").arg(result.workflow.name));
            }
            m_uiCoordinator->appendLog(tr("Imported generated workflow: %1").arg(result.workflow.name));
        });

    connect(dialog, &ui::WorkflowGenerationDialog::generateRequested, this,
        [this, guardedDialog](const infrastructure::LlmProviderSettings& provider, const QString& requirement) {
            if (guardedDialog.isNull()) {
                return;
            }
            cacheCurrentWorkflowViewState();
            guardedDialog->setLoading(true);
            guardedDialog->setStatusMessage(tr("Generating workflow..."));
            m_appContext.workflowGenerationController().generateWorkflow(
                provider,
                requirement,
                guardedDialog,
                [guardedDialog](const QString& message) {
                    if (!guardedDialog.isNull()) {
                        guardedDialog->setStatusMessage(message);
                    }
                },
                [this, guardedDialog](presentation::WorkflowGenerationUiResult result) {
                    if (guardedDialog.isNull()) {
                        return;
                    }
                    guardedDialog->setLoading(false);
                    guardedDialog->setGeneratedJson(result.rawJson);
                    if (!result.success) {
                        guardedDialog->setStatusMessage(result.errorMessage, true);
                        return;
                    }

                    renderCurrentWorkflowOnCanvas();
                    resetInspectorAndOutput();
                    refreshWorkspaceExplorer();
                    updateCanvasOverlay();
                    guardedDialog->setStatusMessage(tr("Generated workflow: %1").arg(result.workflow.name));
                    m_uiCoordinator->appendLog(tr("Generated workflow by LLM: %1").arg(result.workflow.name));
                });
        });

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::openWorkflowById(const QString& workflowId)
{
    if (!ensureWorkspaceOpen() || workflowId.trimmed().isEmpty()) {
        return;
    }
    if (m_store.currentWorkflow().workflowId == workflowId) {
        if (m_appContext.canvasNavigationController().isInsideSubsystem()) {
            navigateCanvasBreadcrumb(0);
        }
        return;
    }

    cacheCurrentWorkflowViewState();
    QString errorMessage;
    if (!m_appContext.workflowController().loadWorkflowFromWorkspace(workflowId, &errorMessage)) {
        QMessageBox::warning(this, tr("Workflow Error"), errorMessage);
        return;
    }

    renderCurrentWorkflowOnCanvas();
    resetInspectorAndOutput();
    applyCachedNodeStatusesForWorkflow(m_store.currentWorkflow().workflowId);
    updateCanvasOverlay();
    m_uiCoordinator->appendLog(tr("Loaded workflow: %1").arg(m_store.currentWorkflow().name));
}

void MainWindow::deleteWorkflowById(const QString& workflowId, const QString& workflowName)
{
    if (!ensureWorkspaceOpen() || workflowId.trimmed().isEmpty()) {
        return;
    }

    const auto displayName = workflowName.trimmed().isEmpty() ? workflowId : workflowName.trimmed();
    const auto answer = QMessageBox::question(
        this,
        tr("Delete Workflow"),
        tr("Delete workflow \"%1\" and all of its run history?\n\nThis cannot be undone.").arg(displayName),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }

    int deletedRunCount = 0;
    QString errorMessage;
    if (!m_appContext.workspaceBrowserController().deleteWorkflowAndRuns(
            workflowId,
            &deletedRunCount,
            &errorMessage)) {
        QMessageBox::warning(this, tr("Delete Workflow"), errorMessage);
        return;
    }

    if (m_store.currentWorkflow().workflowId == workflowId) {
        clearCanvasWorkflowView();
        m_appContext.canvasNavigationController().reset();
        m_store.clearCurrentWorkflow();
        m_store.removeOpenWorkflowDocument(workflowId);
        m_store.removeWorkflowHistory(workflowId);
        resetInspectorAndOutput();
        updateCanvasOverlay();
    } else {
        m_store.removeOpenWorkflowDocument(workflowId);
        m_store.removeWorkflowHistory(workflowId);
    }

    refreshWorkspaceExplorer();
    m_uiCoordinator->appendLog(
        tr("Deleted workflow: %1; deleted runs: %2").arg(displayName).arg(deletedRunCount));
}

void MainWindow::renameWorkflowById(const QString& workflowId, const QString& workflowName)
{
    if (!ensureWorkspaceOpen() || workflowId.trimmed().isEmpty()) {
        return;
    }

    const auto newName = workflowName.trimmed();
    if (newName.isEmpty()) {
        QMessageBox::warning(this, tr("Rename Workflow"), tr("Workflow name cannot be empty."));
        refreshWorkspaceExplorer();
        return;
    }

    cacheCurrentWorkflowViewState();
    QString errorMessage;
    if (!m_appContext.workflowController().renameWorkflow(workflowId, newName, &errorMessage)) {
        QMessageBox::warning(this, tr("Rename Workflow"), errorMessage);
        refreshWorkspaceExplorer();
        return;
    }

    if (m_store.currentWorkflow().workflowId == workflowId) {
        if (!m_appContext.canvasNavigationController().isInsideSubsystem()) {
            renderCurrentWorkflowOnCanvas();
            applyCachedNodeStatusesForWorkflow(workflowId);
        }
        updateCanvasBreadcrumb();
    }

    refreshWorkspaceExplorer();
    m_uiCoordinator->appendLog(tr("Renamed workflow: %1").arg(newName));
}

void MainWindow::addWorkflowAsSubsystemNode(const QString& workflowId, const QString& workflowName)
{
    if (!ensureWorkspaceOpen() || !ensureWorkflowOpen() || workflowId.trimmed().isEmpty()) {
        return;
    }

    cacheCurrentWorkflowViewState();
    if (m_store.currentWorkflow().workflowId == workflowId) {
        QMessageBox::warning(
            this,
            tr("Add Subsystem"),
            tr("Cannot add the current workflow as a subsystem of itself."));
        return;
    }

    domain::Workflow sourceWorkflow;
    QString errorMessage;
    if (!m_appContext.workflowController().workflowSnapshot(workflowId, sourceWorkflow, &errorMessage)) {
        QMessageBox::warning(this, tr("Add Subsystem"), errorMessage);
        return;
    }

    const auto scenePos = m_workflowCanvas->mapToScene(m_workflowCanvas->viewport()->rect().center());
    domain::NodePosition position;
    position.x = scenePos.x();
    position.y = scenePos.y();

    auto subsystemNode = m_appContext.subsystemService().createSubsystemNode(
        m_store.currentWorkspace().id,
        sourceWorkflow.name.trimmed().isEmpty() ? workflowName : sourceWorkflow.name,
        position);
    subsystemNode.description = sourceWorkflow.description.trimmed().isEmpty()
        ? tr("Subsystem copied from workflow: %1").arg(
            sourceWorkflow.name.trimmed().isEmpty() ? workflowId : sourceWorkflow.name)
        : sourceWorkflow.description;

    auto embeddedWorkflow = sourceWorkflow;
    embeddedWorkflow.workflowId = QStringLiteral("%1__subworkflow").arg(subsystemNode.nodeId);
    embeddedWorkflow.workspaceId = m_store.currentWorkspace().id;
    if (!m_appContext.subsystemService().saveSubsystemWorkflow(
            subsystemNode,
            embeddedWorkflow,
            &errorMessage)) {
        QMessageBox::warning(this, tr("Add Subsystem"), errorMessage);
        return;
    }

    m_workflowCanvas->addNode(subsystemNode);
    m_uiCoordinator->appendLog(
        tr("Added workflow as subsystem node: %1").arg(subsystemNode.name));
}

void MainWindow::openRunById(const QString& runId)
{
    if (!ensureWorkspaceOpen() || runId.trimmed().isEmpty()) {
        return;
    }

    if (m_store.workflowRunning()) {
        QMessageBox::warning(
            this,
            tr("Load Run"),
            tr("Cannot load a historical run while a workflow is running."));
        return;
    }

    domain::RunRecord record;
    QHash<QString, QJsonObject> nodeOutputsByNodeId;
    QString errorMessage;

    if (!m_appContext.runController().loadRunRecordWithNodeOutputs(
            runId,
            record,
            nodeOutputsByNodeId,
            &errorMessage)) {
        QMessageBox::warning(this, tr("Load Run"), errorMessage);
        return;
    }

    domain::Workflow workflowSnapshot;
    bool usedCurrentWorkflowFile = false;
    const auto loadedSnapshot = m_appContext.workflowController().loadWorkflowSnapshotForRun(
        record,
        workflowSnapshot,
        &usedCurrentWorkflowFile,
        &errorMessage);

    if (loadedSnapshot && usedCurrentWorkflowFile) {
        QMessageBox::information(
            this,
            tr("Historical Run"),
            tr("This run does not contain a workflow snapshot. "
               "The current workflow file was loaded instead, so the historical canvas structure may not be exact."));
    }

    if (!loadedSnapshot) {
        QMessageBox::warning(
            this,
            tr("Load Run"),
            tr("Could not load workflow snapshot for this run: %1").arg(errorMessage));
        return;
    }

    restoreRunRecordToUi(record, workflowSnapshot, nodeOutputsByNodeId);
}

void MainWindow::restoreRunRecordToUi(
    const domain::RunRecord& record,
    const domain::Workflow& workflowSnapshot,
    const QHash<QString, QJsonObject>& nodeOutputsByNodeId)
{
    cacheCurrentWorkflowViewState();
    m_store.setCurrentWorkflow(workflowSnapshot);
    renderCurrentWorkflowOnCanvas();
    updateCanvasOverlay();

    m_uiCoordinator->restoreRunRecord(record, workflowSnapshot, nodeOutputsByNodeId);
    m_uiCoordinator->appendLog(
        tr("Loaded run record: %1").arg(record.id));
}

void MainWindow::saveWorkflow()
{
    if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
        return;
    }

    domain::Workflow rootWorkflow;
    QString errorMessage;
    if (!m_appContext.canvasSessionController().prepareRootWorkflowFromView(
            m_workflowCanvas->workflow(),
            m_workflowCanvas->history(),
            rootWorkflow,
            &errorMessage)) {
        QMessageBox::warning(this, tr("Workflow Error"), errorMessage);
        return;
    }

    if (!m_appContext.workflowController().saveCurrentWorkflow(&errorMessage)) {
        QMessageBox::warning(this, tr("Workflow Error"), errorMessage);
        return;
    }

    if (!m_appContext.canvasNavigationController().isInsideSubsystem()) {
        m_appContext.canvasSessionController().updateCurrentHistory(m_workflowCanvas->history());
    }
    refreshWorkspaceExplorer();
    updateCanvasBreadcrumb();
    m_uiCoordinator->appendLog(tr("Saved workflow: %1").arg(m_store.currentWorkflow().name));
}

void MainWindow::saveSelectedNodeAsTemplate()
{
    if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
        return;
    }

    m_appContext.canvasSessionController().syncCurrentView(m_workflowCanvas->workflow());
    const auto selectedNode = m_workflowCanvas->selectedNode();
    if (!selectedNode.has_value()) {
        QMessageBox::information(this, tr("Template Required"), tr("Select one node on the canvas before saving a template."));
        return;
    }

    bool accepted = false;
    const auto templateName = QInputDialog::getText(
        this,
        tr("Save Node Template"),
        tr("Template name"),
        QLineEdit::Normal,
        selectedNode->name,
        &accepted);
    if (!accepted) {
        return;
    }

    domain::NodeTemplate nodeTemplate;
    QString errorMessage;
    if (!m_appContext.nodeTemplateController().saveTemplateFromNode(
            selectedNode.value(),
            templateName,
            &nodeTemplate,
            &errorMessage)) {
        QMessageBox::warning(this, tr("Template Error"), errorMessage);
        return;
    }

    refreshWorkspaceExplorer();
    m_uiCoordinator->appendLog(tr("Saved node template: %1").arg(nodeTemplate.name));
}

void MainWindow::addNodeFromTemplate()
{
    if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
        return;
    }

    const auto filePath = QFileDialog::getOpenFileName(
        this,
        tr("Add Node From Template"),
        QDir(m_store.currentWorkspace().rootPath).filePath("node_templates"),
        tr("Node Template JSON (*.json);;All files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }

    domain::NodeTemplate nodeTemplate;
    domain::Node node;
    QString errorMessage;
    if (!m_appContext.nodeTemplateController().createNodeFromTemplateFile(
            filePath,
            node,
            &nodeTemplate,
            &errorMessage)) {
        QMessageBox::warning(this, tr("Template Error"), errorMessage);
        return;
    }

    node.position.x = 80.0 * m_store.currentWorkflow().nodes.size();
    node.position.y = 120.0;
    m_workflowCanvas->addNode(node);
    saveWorkflow();
    m_uiCoordinator->appendLog(tr("Added node from template: %1").arg(nodeTemplate.name));
}

void MainWindow::connectSelectedNodes()
{
    if (!ensureWorkflowOpen()) {
        return;
    }

    if (!m_workflowCanvas->connectSelectedNodes()) {
        QMessageBox::information(this, tr("Create Edge"), tr("Select two nodes on the canvas first, or drag from an output port to an input port."));
        return;
    }

    saveWorkflow();
    m_uiCoordinator->appendLog(tr("Created edge between nodes."));
}

void MainWindow::importNodeTemplate()
{
    if (!ensureWorkspaceOpen()) {
        return;
    }

    const auto filePath = QFileDialog::getOpenFileName(
        this,
        tr("Import Node Template"),
        {},
        tr("Node Template JSON (*.json);;All files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }

    domain::NodeTemplate importedTemplate;
    QString errorMessage;
    if (!m_appContext.nodeTemplateController().importTemplateFile(filePath, &importedTemplate, &errorMessage)) {
        QMessageBox::warning(this, tr("Template Error"), errorMessage);
        return;
    }

    refreshWorkspaceExplorer();
    m_uiCoordinator->appendLog(tr("Imported node template: %1").arg(importedTemplate.name));
}

void MainWindow::runCurrentWorkflow()
{
    if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
        return;
    }
    if (m_store.workflowRunning()) {
        m_uiCoordinator->appendError(tr("A workflow is already running."));
        return;
    }

    QString flushError;
    domain::Workflow rootWorkflow;
    if (!m_appContext.canvasSessionController().prepareRootWorkflowFromView(
            m_workflowCanvas->workflow(),
            m_workflowCanvas->history(),
            rootWorkflow,
            &flushError)) {
        QMessageBox::warning(this, tr("Run Workflow"), flushError);
        return;
    }
    applyWorkspacePythonExecutable();
    if (m_appContext.pythonEnvironmentController().pythonExecutable().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Run Workflow"), tr("Select a Python interpreter for this workspace before running a workflow."));
        m_uiCoordinator->appendError(tr("Run blocked: no Python interpreter selected for this workspace."));
        return;
    }
    presentation::WorkflowRunPlan runPlan;
    QString runPlanError;
    if (!m_appContext.runController().prepareCurrentWorkflowRun(runPlan, &runPlanError)) {
        QMessageBox::warning(this, tr("Run Workflow"), runPlanError);
        return;
    }

    refreshWorkspaceExplorer();
    m_uiCoordinator->beginRun(m_store.currentWorkflow());
    m_uiCoordinator->appendLog(tr("Run started in background."));

    execution::WorkflowRunOptions runOptions;
    if (m_animateNodeStatusAction != nullptr && m_animateNodeStatusAction->isChecked()) {
        runOptions.nodeDispatchDelayMs = qMax(0, m_nodeDispatchDelayMs);
    }

    m_appContext.runController().runWorkflowAsync(
        runPlan.workflow,
        runOptions,
        runPlan.workspaceRootPath,
        runPlan.runRootPath,
        runPlan.artifactPath,
        this,
        [this, runPlan](execution::WorkflowExecutionResult result) {
            m_appContext.runController().finishRun(runPlan.workflowId);
            refreshWorkspaceExplorer();
            m_uiCoordinator->finishRun(result);

            QString runRecordError;
            if (!m_appContext.runController().saveRunRecord(runPlan, result, &runRecordError)) {
                m_uiCoordinator->appendError(tr("Could not save run record: %1").arg(runRecordError));
            }

            if (result.success) {
                m_uiCoordinator->appendLog(tr("Run succeeded: %1").arg(result.runId));
            } else {
                m_uiCoordinator->appendError(tr("Run finished with status %1: %2").arg(result.status, result.errors.join("; ")));
            }
            refreshWorkspaceExplorer();
        });
}

void MainWindow::cancelCurrentWorkflowRun()
{
    if (!m_store.workflowRunning()) {
        m_uiCoordinator->appendLog(tr("No workflow is currently running."));
        return;
    }

    m_appContext.runController().requestCancelCurrentRun();
    m_uiCoordinator->appendError(tr("Cancellation requested for the running workflow."));
}

void MainWindow::openPythonNodeEditor(const domain::Node& node)
{
    const domain::NodeConfigView nodeConfig(node.config);
    const auto language = nodeConfig.language();
    const auto isPythonCodeNode = NodeTypes::isPythonBacked(node.type);
    if (!isPythonCodeNode || language != "python") {
        QMessageBox::information(this, tr("Python Editor"), tr("Only Python-based starter, function, agent, and loop nodes can be edited here."));
        return;
    }

    auto* dialog = new ui::PythonNodeEditorDialog(
        node.name,
        node.description,
        node.runtime.timeoutMs,
        node.type,
        node.config,
        nodeConfig.code(),
        application::PythonCodeTemplates::defaultCodeForNodeType(node.type),
        this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);

    // Keep the editor modeless. Saving is deferred below so Qt does not mutate the canvas while a graphics-item double-click event is still unwinding.
    connect(dialog, &ui::PythonNodeEditorDialog::nodeSaved, this,
        [this, nodeId = node.nodeId](const QString& name, const QString& description, int timeoutMs, const QString& code, const QJsonObject& configPatch) {
        // The save button/close prompt is also inside a UI event. Defer canvas mutation to the next event turn.
        QTimer::singleShot(0, this, [this, nodeId, name, description, timeoutMs, code, configPatch]() {
            QString errorMessage;
            if (m_appContext.canvasNavigationController().isInsideSubsystem()) {
                auto workflow = m_workflowCanvas->workflow();
                if (!application::WorkflowService().updateNodeDetails(
                        workflow,
                        nodeId,
                        name,
                        description,
                        timeoutMs,
                        code,
                        configPatch,
                        &errorMessage)) {
                    QMessageBox::warning(this, tr("Save Python Code"), errorMessage);
                    return;
                }

                m_appContext.canvasSessionController().syncCurrentView(workflow);
                for (const auto& updatedNode : workflow.nodes) {
                    if (updatedNode.nodeId == nodeId) {
                        m_workflowCanvas->updateNode(updatedNode);
                        m_uiCoordinator->appendLog(tr("Saved Python code for node: %1").arg(updatedNode.name));
                        break;
                    }
                }
                return;
            }

            if (!m_appContext.workflowController().updateNodeDetails(
                    nodeId,
                    name,
                    description,
                    timeoutMs,
                    code,
                    configPatch,
                    &errorMessage)) {
                QMessageBox::warning(this, tr("Save Python Code"), errorMessage);
                return;
            }

            for (const auto& updatedNode : m_store.currentWorkflow().nodes) {
                if (updatedNode.nodeId == nodeId) {
                    m_workflowCanvas->updateNode(updatedNode);
                    m_uiCoordinator->appendLog(tr("Saved Python code for node: %1").arg(updatedNode.name));
                    break;
                }
            }
        });
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::retitleSubsystemNode(const domain::Node& node)
{
    if (!m_appContext.subsystemService().isSubsystemNode(node)) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Retitle Subsystem Node"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* formLayout = new QFormLayout();

    auto* titleEdit = new QLineEdit(node.name, &dialog);
    auto* descriptionEdit = new QPlainTextEdit(node.description, &dialog);
    descriptionEdit->setFixedHeight(72);
    formLayout->addRow(tr("Title"), titleEdit);
    formLayout->addRow(tr("Description"), descriptionEdit);
    layout->addLayout(formLayout);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    auto updatedNode = node;
    const auto newTitle = titleEdit->text().trimmed();
    if (newTitle.isEmpty()) {
        QMessageBox::warning(this, tr("Retitle Node"), tr("Subsystem title cannot be empty."));
        return;
    }
    updatedNode.name = newTitle;
    updatedNode.description = descriptionEdit->toPlainText();

    domain::Workflow embeddedWorkflow;
    QString errorMessage;
    if (!m_appContext.subsystemService().loadSubsystemWorkflow(
            updatedNode,
            embeddedWorkflow,
            &errorMessage)) {
        QMessageBox::warning(this, tr("Retitle Node"), errorMessage);
        return;
    }

    embeddedWorkflow.name = updatedNode.name;
    embeddedWorkflow.description = updatedNode.description;
    if (!m_appContext.subsystemService().saveSubsystemWorkflow(
            updatedNode,
            embeddedWorkflow,
            &errorMessage)) {
        QMessageBox::warning(this, tr("Retitle Node"), errorMessage);
        return;
    }

    if (!m_workflowCanvas->updateNode(updatedNode)) {
        QMessageBox::warning(this, tr("Retitle Node"), tr("Could not update the selected subsystem node."));
        return;
    }

    const auto updatedWorkflow = m_workflowCanvas->workflow();
    m_appContext.canvasSessionController().syncCurrentView(updatedWorkflow);

    m_uiCoordinator->selectNode(updatedNode);
    updateCanvasBreadcrumb();
    m_uiCoordinator->appendLog(tr("Retitled subsystem node: %1").arg(updatedNode.name));
}

void MainWindow::refreshWorkspaceExplorer()
{
    if (m_store.currentWorkspace().rootPath.isEmpty()) {
        return;
    }

    const auto snapshot = m_appContext.workspaceBrowserController().snapshot();
    for (const auto& errorMessage : snapshot.errors) {
        m_uiCoordinator->appendError(errorMessage);
    }

    ui::WorkspaceExplorerViewModel viewModel;
    viewModel.workspaceName = snapshot.workspaceName;
    for (int index = 0; index < snapshot.workflowNames.size(); ++index) {
        ui::WorkspaceExplorerItemViewModel item;
        item.name = snapshot.workflowNames.at(index);
        if (index < snapshot.workflowIds.size()) {
            item.id = snapshot.workflowIds.at(index);
            item.running = snapshot.runningWorkflowIds.contains(item.id);
        }
        viewModel.workflows.append(item);
    }
    for (int index = 0; index < snapshot.templateNames.size(); ++index) {
        ui::WorkspaceExplorerItemViewModel item;
        item.name = snapshot.templateNames.at(index);
        if (index < snapshot.templateIds.size()) {
            item.id = snapshot.templateIds.at(index);
        }
        viewModel.templates.append(item);
    }
    for (int index = 0; index < snapshot.runNames.size(); ++index) {
        ui::WorkspaceExplorerItemViewModel item;
        item.name = snapshot.runNames.at(index);
        if (index < snapshot.runIds.size()) {
            item.id = snapshot.runIds.at(index);
        }
        viewModel.runs.append(item);
    }
    m_uiCoordinator->renderWorkspaceExplorer(viewModel);
}

void MainWindow::applyWorkspacePythonExecutable()
{
    m_appContext.pythonEnvironmentController().applyCurrentWorkspacePythonExecutable();
    updatePythonStatus();
}

void MainWindow::updatePythonStatus()
{
    const auto pythonExecutable = m_appContext.pythonEnvironmentController().pythonExecutable().trimmed();
    if (m_uiCoordinator != nullptr) {
        m_uiCoordinator->setPythonExecutableStatus(pythonExecutable);
    }
}

void MainWindow::configureNodeDispatchDelay()
{
    bool accepted = false;
    const int delayMs = QInputDialog::getInt(
        this,
        tr("Node Step Delay"),
        tr("Delay between completed node and next node (ms)"),
        m_nodeDispatchDelayMs,
        0,
        5000,
        10,
        &accepted);
    if (!accepted) {
        return;
    }

    m_nodeDispatchDelayMs = delayMs;
    statusBar()->showMessage(tr("Node step delay: %1 ms").arg(m_nodeDispatchDelayMs), 3000);
}

bool MainWindow::ensureWorkspaceOpen()
{
    if (!m_store.currentWorkspace().rootPath.isEmpty()) {
        return true;
    }

    QMessageBox::information(this, tr("Workspace Required"), tr("Create or open a workspace first."));
    return false;
}

bool MainWindow::ensureWorkflowOpen()
{
    if (!m_store.currentWorkflow().workflowId.isEmpty()) {
        return true;
    }

    QMessageBox::information(this, tr("Workflow Required"), tr("Create or load a workflow first."));
    return false;
}

void MainWindow::resetInspectorAndOutput()
{
    if (m_uiCoordinator != nullptr) {
        m_uiCoordinator->resetForWorkflowChange();
    }
}

void MainWindow::resetInspectorView()
{
    if (m_uiCoordinator != nullptr) {
        m_uiCoordinator->clearNodeSelection();
    }
}

void MainWindow::applyCachedNodeStatusesForWorkflow(const QString& workflowId)
{
    if (m_uiCoordinator != nullptr) {
        m_uiCoordinator->applyCachedNodeStatuses(workflowId);
    }
}

void MainWindow::addNodeFromTemplateIdAt(const QString& templateId, const QPointF& scenePos)
{
    if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
        return;
    }

    domain::NodeTemplate nodeTemplate;
    domain::Node node;
    QString errorMessage;

    if (!m_appContext.nodeTemplateController().createNodeFromWorkspaceTemplate(
            templateId,
            node,
            &nodeTemplate,
            &errorMessage)) {
        QMessageBox::warning(this, tr("Node Template"), errorMessage);
        return;
    }

    node.position.x = scenePos.x();
    node.position.y = scenePos.y();

    m_workflowCanvas->addNode(node);
    saveWorkflow();

    m_uiCoordinator->appendLog(
        tr("Added node from template: %1").arg(nodeTemplate.name));
}

} // namespace vws
