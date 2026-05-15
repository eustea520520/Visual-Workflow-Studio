#include "MainWindow.h"

#include "AppContext.h"
#include "application/NodeFactory.h"
#include "application/PythonCodeTemplates.h"
#include "domain/NodeTemplate.h"
#include "domain/NodeConfigView.h"
#include "domain/NodeTypes.h"
#include "presentation/controllers/NodeTemplateController.h"
#include "presentation/controllers/PythonEnvironmentController.h"
#include "presentation/controllers/RunController.h"
#include "presentation/controllers/WorkspaceBrowserController.h"
#include "presentation/controllers/WorkflowController.h"
#include "presentation/controllers/WorkspaceController.h"
#include "presentation/models/WorkflowDisplayModel.h"
#include "presentation/state/AppStore.h"
#include "ui/canvas/WorkflowCanvas.h"
#include "ui/editor/PythonNodeEditorDialog.h"
#include "ui/inspector/NodeInspector.h"
#include "ui/main/MainWindowLayoutBuilder.h"
#include "ui/output/OutputPanel.h"
#include "ui/output/OutputPanelViewModel.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/CommandBar.h"
#include "ui/widgets/EmptyStateOverlay.h"
#include "ui/workspace/WorkspaceExplorer.h"
#include "ui/workspace/WorkspaceExplorerViewModel.h"

#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScopedValueRollback>
#include <QStatusBar>
#include <QTimer>

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

void MainWindow::buildActions()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* workflowMenu = menuBar()->addMenu(tr("&Workflow"));
    auto* viewMenu = menuBar()->addMenu(tr("&View"));

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
    workflowMenu->addSeparator();
    m_runAction = workflowMenu->addAction(tr("Run Workflow"));
    m_cancelRunAction = workflowMenu->addAction(tr("Cancel Run"));

    m_toggleThemeAction = viewMenu->addAction(tr("Toggle Light / Dark Theme"));

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
    connect(m_runAction, &QAction::triggered, this, &MainWindow::runCurrentWorkflow);
    connect(m_cancelRunAction, &QAction::triggered, this, &MainWindow::cancelCurrentWorkflowRun);
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
        m_runAction,
        m_cancelRunAction,
        m_toggleThemeAction,
    });
    m_commandBar = layout.commandBar;
    m_workspaceExplorer = layout.workspaceExplorer;
    m_workflowCanvas = layout.workflowCanvas;
    m_nodeInspector = layout.nodeInspector;
    m_outputPanel = layout.outputPanel;
    m_canvasOverlay = layout.canvasOverlay;
    setCentralWidget(layout.centralWidget);

    // UI widgets emit intent; application services and the execution engine do the work.
    connect(m_workflowCanvas, &ui::WorkflowCanvas::nodeSelected, this, [this](const domain::Node& node) {
        m_store.selectedNodeId() = node.nodeId;
        m_nodeInspector->displayNode(node, m_store.nodeOutputsByNodeId().value(node.nodeId));
        updateSelectedNodeStatus(node);
    });
    connect(m_workflowCanvas, &ui::WorkflowCanvas::nodeSelectionCleared, this, [this]() {
        m_store.selectedNodeId().clear();
        if (m_nodeInspector != nullptr) {
            m_nodeInspector->clear();
        }
        if (m_timeoutStatusLabel != nullptr) {
            m_timeoutStatusLabel->setText(tr("Timeout: -"));
        }
    });
    connect(m_workflowCanvas, &ui::WorkflowCanvas::nodeDoubleClicked, this, &MainWindow::openPythonNodeEditor);
    connect(m_workflowCanvas, &ui::WorkflowCanvas::saveRequested, this, &MainWindow::saveWorkflow);
    connect(m_workflowCanvas, &ui::WorkflowCanvas::workflowChanged, this, [this](const domain::Workflow& workflow) {
        if (m_renderingWorkflow) {
            return;
        }
        m_appContext.workflowController().syncCurrentWorkflowFromView(workflow);
    });
    connect(m_workspaceExplorer, &ui::WorkspaceExplorer::workflowActivated, this, &MainWindow::openWorkflowById);
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
    connect(m_workflowCanvas, &ui::WorkflowCanvas::nodeTemplateDropped,
        this, &MainWindow::addNodeFromTemplateIdAt);
    connect(&m_appContext.runController(), &presentation::RunController::nodeStatusChanged, this,
        [this](const QString& runId, const QString& workflowId, const QString& nodeId, const QString& status) {
            if (m_store.currentWorkflow().workflowId == workflowId) {
                m_workflowCanvas->setNodeStatus(nodeId, status);
                m_outputPanel->recordNodeStatus(runId, nodeId, status);
            }
        });
    connect(&m_appContext.runController(), &presentation::RunController::workflowStatusChanged, this,
        [this](const QString& runId, const QString& workflowId, const QString& status) {
            if (m_store.currentWorkflow().workflowId == workflowId) {
                m_outputPanel->recordWorkflowStatus(runId, status);
            }
        });
    connect(&m_appContext.runController(), &presentation::RunController::nodeOutputReady, this,
        [this](const QString& runId, const QString& workflowId, const QString& nodeId, const QJsonObject& outputs) {
            if (m_store.currentWorkflow().workflowId != workflowId) {
                return;
            }

            m_store.nodeOutputsByNodeId().insert(nodeId, outputs);
            m_outputPanel->recordNodeOutput(runId, nodeId, outputs);

            if (m_store.selectedNodeId() == nodeId) {
                const auto selected = m_workflowCanvas->selectedNode();
                if (selected.has_value() && selected->nodeId == nodeId) {
                    m_nodeInspector->displayNode(selected.value(), outputs);
                }
            }
        });
    connect(&m_appContext.runController(), &presentation::RunController::nodeError, this,
        [this](const QString& runId, const QString& workflowId, const QString& nodeId, const QString& message) {
            if (m_store.currentWorkflow().workflowId == workflowId) {
                m_outputPanel->recordNodeError(runId, nodeId, message);
            }
        });
    connect(&m_appContext.runController(), &presentation::RunController::threadTrace, this,
        [this](const QString& runId, const QString& workflowId, const QString& nodeId, const QString& phase, const QString& threadId, const QString& threadName) {
            if (m_store.currentWorkflow().workflowId == workflowId) {
                m_outputPanel->recordThreadTrace(runId, nodeId, phase, threadId, threadName);
            }
        });

    // Canvas items are painted manually, so they refresh when the theme changes.
    connect(m_themeManager, &ui::ThemeManager::themeChanged, this, [this](ui::AppTheme) {
        if (m_workflowCanvas != nullptr) {
            m_workflowCanvas->refreshTheme();
        }
        updateCanvasOverlay();
    });

    // Status bar shows selected-node runtime info and the workspace Python interpreter.
    m_timeoutStatusLabel = new QLabel(tr("Timeout: -"), this);
    m_timeoutStatusLabel->setObjectName("timeoutStatus");
    m_pythonStatusLabel = new QLabel(tr("Python: not selected"), this);
    m_pythonStatusLabel->setObjectName("pythonStatus");
    statusBar()->addPermanentWidget(m_timeoutStatusLabel);
    statusBar()->addPermanentWidget(m_pythonStatusLabel, 1);
}

void MainWindow::applyInitialTheme()
{
    m_themeManager->applyTheme(ui::AppTheme::Light);
}

void MainWindow::renderCurrentWorkflowOnCanvas()
{
    if (m_workflowCanvas == nullptr) {
        return;
    }

    QScopedValueRollback<bool> guard(m_renderingWorkflow, true);
    m_workflowCanvas->setWorkflow(m_store.currentWorkflow());
}

void MainWindow::clearCanvasWorkflowView()
{
    if (m_workflowCanvas == nullptr) {
        return;
    }

    QScopedValueRollback<bool> guard(m_renderingWorkflow, true);
    m_workflowCanvas->clearWorkflow();
}

void MainWindow::updateCanvasOverlay()
{
    if (m_canvasOverlay == nullptr) {
        return;
    }

    if (m_store.currentWorkspace().rootPath.isEmpty()) {
        if (m_commandBar != nullptr) {
            m_commandBar->setWorkspaceInfo(tr("Visual Workflow Studio"));
            m_commandBar->setWorkflowInfo(tr("No workspace"));
        }
        m_canvasOverlay->render(ui::EmptyStateOverlay::Mode::NoWorkspace);
        return;
    }

    if (m_commandBar != nullptr) {
        m_commandBar->setWorkspaceInfo(m_store.currentWorkspace().name);
    }

    if (m_store.currentWorkflow().workflowId.isEmpty()) {
        if (m_commandBar != nullptr) {
            m_commandBar->setWorkflowInfo(tr("No workflow"));
        }
        m_canvasOverlay->render(ui::EmptyStateOverlay::Mode::NoWorkflow);
        return;
    }

    if (m_commandBar != nullptr) {
        m_commandBar->setWorkflowInfo(m_store.currentWorkflow().name);
    }

    m_canvasOverlay->render(ui::EmptyStateOverlay::Mode::Hidden);
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
    resetInspectorAndOutput();
    applyWorkspacePythonExecutable();
    refreshWorkspaceExplorer();
    updateCanvasOverlay();
    m_outputPanel->appendStdout(tr("Created workspace: %1").arg(m_store.currentWorkspace().rootPath));
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
    resetInspectorAndOutput();
    applyWorkspacePythonExecutable();
    refreshWorkspaceExplorer();
    updateCanvasOverlay();
    m_outputPanel->appendStdout(tr("Opened workspace: %1").arg(m_store.currentWorkspace().rootPath));
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
    m_outputPanel->appendStdout(tr("Selected Python interpreter: %1").arg(selectedPath));
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

    m_appContext.workflowController().createWorkflow(workflowName);
    renderCurrentWorkflowOnCanvas();
    resetInspectorAndOutput();
    applyCachedNodeStatusesForWorkflow(m_store.currentWorkflow().workflowId);
    updateCanvasOverlay();

    saveWorkflow();
    m_outputPanel->appendStdout(tr("Created workflow: %1").arg(m_store.currentWorkflow().name));
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

    QString errorMessage;
    if (!m_appContext.workflowController().loadWorkflowFile(filePath, &errorMessage)) {
        QMessageBox::warning(this, tr("Workflow Error"), errorMessage);
        return;
    }

    renderCurrentWorkflowOnCanvas();
    resetInspectorAndOutput();
    applyCachedNodeStatusesForWorkflow(m_store.currentWorkflow().workflowId);
    updateCanvasOverlay();
    m_outputPanel->appendStdout(tr("Loaded workflow: %1").arg(m_store.currentWorkflow().name));
}

void MainWindow::openWorkflowById(const QString& workflowId)
{
    if (!ensureWorkspaceOpen() || workflowId.trimmed().isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_appContext.workflowController().loadWorkflowFromWorkspace(workflowId, &errorMessage)) {
        QMessageBox::warning(this, tr("Workflow Error"), errorMessage);
        return;
    }

    renderCurrentWorkflowOnCanvas();
    resetInspectorAndOutput();
    applyCachedNodeStatusesForWorkflow(m_store.currentWorkflow().workflowId);
    updateCanvasOverlay();
    m_outputPanel->appendStdout(tr("Loaded workflow: %1").arg(m_store.currentWorkflow().name));
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
    m_store.setCurrentWorkflow(workflowSnapshot);
    renderCurrentWorkflowOnCanvas();
    updateCanvasOverlay();

    m_outputPanel->clearRun();
    m_store.nodeOutputsByNodeId().clear();
    m_store.nodeOutputsByNodeId() = nodeOutputsByNodeId;
    const auto displayModel = presentation::WorkflowDisplayModelBuilder::build(m_store.currentWorkflow());
    m_outputPanel->render({displayModel.workflowName, displayModel.nodeNamesById});

    for (const auto& nodeRun : record.nodeRuns) {
        m_workflowCanvas->setNodeStatus(nodeRun.nodeId, nodeRun.status);
    }

    m_outputPanel->showRunRecord(record, m_store.nodeOutputsByNodeId());

    if (const auto selected = m_workflowCanvas->selectedNode(); selected.has_value()) {
        m_store.selectedNodeId() = selected->nodeId;
        m_nodeInspector->displayNode(
            selected.value(),
            m_store.nodeOutputsByNodeId().value(selected->nodeId));
    }

    m_outputPanel->appendStdout(
        tr("Loaded run record: %1").arg(record.id));
}

void MainWindow::saveWorkflow()
{
    if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
        return;
    }

    m_appContext.workflowController().syncCurrentWorkflowFromView(m_workflowCanvas->workflow());

    QString errorMessage;
    if (!m_appContext.workflowController().saveCurrentWorkflow(&errorMessage)) {
        QMessageBox::warning(this, tr("Workflow Error"), errorMessage);
        return;
    }

    refreshWorkspaceExplorer();
    m_outputPanel->appendStdout(tr("Saved workflow: %1").arg(m_store.currentWorkflow().name));
}

void MainWindow::saveSelectedNodeAsTemplate()
{
    if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
        return;
    }

    m_appContext.workflowController().syncCurrentWorkflowFromView(m_workflowCanvas->workflow());
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
    m_outputPanel->appendStdout(tr("Saved node template: %1").arg(nodeTemplate.name));
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
    m_outputPanel->appendStdout(tr("Added node from template: %1").arg(nodeTemplate.name));
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
    m_outputPanel->appendStdout(tr("Created edge between nodes."));
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
    m_outputPanel->appendStdout(tr("Imported node template: %1").arg(importedTemplate.name));
}

void MainWindow::runCurrentWorkflow()
{
    if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
        return;
    }
    if (m_store.workflowRunning()) {
        m_outputPanel->appendStderr(tr("A workflow is already running."));
        return;
    }

    m_appContext.workflowController().syncCurrentWorkflowFromView(m_workflowCanvas->workflow());
    applyWorkspacePythonExecutable();
    if (m_appContext.pythonEnvironmentController().pythonExecutable().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Run Workflow"), tr("Select a Python interpreter for this workspace before running a workflow."));
        m_outputPanel->appendStderr(tr("Run blocked: no Python interpreter selected for this workspace."));
        return;
    }
    presentation::WorkflowRunPlan runPlan;
    QString runPlanError;
    if (!m_appContext.runController().prepareCurrentWorkflowRun(runPlan, &runPlanError)) {
        QMessageBox::warning(this, tr("Run Workflow"), runPlanError);
        return;
    }

    refreshWorkspaceExplorer();
    m_outputPanel->clearRun();
    if (const auto selected = m_workflowCanvas->selectedNode(); selected.has_value()) {
        m_store.selectedNodeId() = selected->nodeId;
        m_nodeInspector->displayNode(selected.value(), {});
    }
    const auto displayModel = presentation::WorkflowDisplayModelBuilder::build(m_store.currentWorkflow());
    m_outputPanel->render({displayModel.workflowName, displayModel.nodeNamesById});

    m_outputPanel->appendStdout(tr("Run started in background."));

    m_appContext.runController().runWorkflowAsync(
        runPlan.workflow,
        runPlan.workspaceRootPath,
        runPlan.runRootPath,
        runPlan.artifactPath,
        this,
        [this, runPlan](execution::WorkflowExecutionResult result) {
            m_appContext.runController().finishRun(runPlan.workflowId);
            refreshWorkspaceExplorer();
            m_outputPanel->showExecutionResult(result);

            QString runRecordError;
            if (!m_appContext.runController().saveRunRecord(runPlan, result, &runRecordError)) {
                m_outputPanel->appendStderr(tr("Could not save run record: %1").arg(runRecordError));
            }

            if (result.success) {
                m_outputPanel->appendStdout(tr("Run succeeded: %1").arg(result.runId));
            } else {
                m_outputPanel->appendStderr(tr("Run finished with status %1: %2").arg(result.status, result.errors.join("; ")));
            }
            refreshWorkspaceExplorer();
        });
}

void MainWindow::cancelCurrentWorkflowRun()
{
    if (!m_store.workflowRunning()) {
        m_outputPanel->appendStdout(tr("No workflow is currently running."));
        return;
    }

    m_appContext.runController().requestCancelCurrentRun();
    m_outputPanel->appendStderr(tr("Cancellation requested for the running workflow."));
}

void MainWindow::openPythonNodeEditor(const domain::Node& node)
{
    const domain::NodeConfigView nodeConfig(node.config);
    const auto language = nodeConfig.language();
    const auto isPythonCodeNode = NodeTypes::isPythonBacked(node.type);
    if (!isPythonCodeNode || language != "python") {
        QMessageBox::information(this, tr("Python Editor"), tr("Only Python-based starter, function, and agent nodes can be edited here."));
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
            m_appContext.workflowController().syncCurrentWorkflowFromView(m_workflowCanvas->workflow());
            QString errorMessage;
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
                    m_outputPanel->appendStdout(tr("Saved Python code for node: %1").arg(updatedNode.name));
                    break;
                }
            }
        });
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::refreshWorkspaceExplorer()
{
    if (m_store.currentWorkspace().rootPath.isEmpty()) {
        return;
    }

    const auto snapshot = m_appContext.workspaceBrowserController().snapshot();
    for (const auto& errorMessage : snapshot.errors) {
        m_outputPanel->appendStderr(errorMessage);
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
    m_workspaceExplorer->render(viewModel);
}

void MainWindow::applyWorkspacePythonExecutable()
{
    m_appContext.pythonEnvironmentController().applyCurrentWorkspacePythonExecutable();
    updatePythonStatus();
}

void MainWindow::updatePythonStatus()
{
    if (m_pythonStatusLabel == nullptr) {
        return;
    }

    const auto pythonExecutable = m_appContext.pythonEnvironmentController().pythonExecutable().trimmed();
    m_pythonStatusLabel->setText(pythonExecutable.isEmpty()
            ? tr("Python: not selected")
            : tr("Python: %1").arg(QDir::toNativeSeparators(pythonExecutable)));
}

void MainWindow::updateSelectedNodeStatus(const domain::Node& node)
{
    if (m_timeoutStatusLabel != nullptr) {
        m_timeoutStatusLabel->setText(tr("Timeout: %1 ms").arg(node.runtime.timeoutMs));
    }
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
    m_store.resetForWorkflowChange();

    if (m_nodeInspector != nullptr) {
        m_nodeInspector->clear();
    }

    if (m_outputPanel != nullptr) {
        m_outputPanel->clearRun();
    }

    if (m_timeoutStatusLabel != nullptr) {
        m_timeoutStatusLabel->setText(tr("Timeout: -"));
    }
}

void MainWindow::applyCachedNodeStatusesForWorkflow(const QString& workflowId)
{
    if (workflowId.trimmed().isEmpty() || m_workflowCanvas == nullptr) {
        return;
    }

    const auto statuses = m_store.nodeStatusesByWorkflowId().value(workflowId);
    for (auto it = statuses.cbegin(); it != statuses.cend(); ++it) {
        m_workflowCanvas->setNodeStatus(it.key(), it.value());
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

    m_outputPanel->appendStdout(
        tr("Added node from template: %1").arg(nodeTemplate.name));
}

} // namespace vws
