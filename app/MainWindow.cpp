#include "MainWindow.h"

#include "AppContext.h"
#include "application/NodeTemplateService.h"
#include "application/RunService.h"
#include "application/WorkflowService.h"
#include "application/WorkspaceService.h"
#include "domain/NodeTemplate.h"
#include "execution/ExecutionEngine.h"
#include "ui/canvas/WorkflowCanvas.h"
#include "ui/editor/PythonNodeEditorDialog.h"
#include "ui/editor/PythonCodeTemplates.h"
#include "ui/inspector/NodeInspector.h"
#include "ui/output/OutputPanel.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/CommandBar.h"
#include "ui/widgets/IconSquareButton.h"
#include "ui/workspace/WorkspaceExplorer.h"

#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedLayout>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

namespace vws {

MainWindow::MainWindow(AppContext& appContext, QWidget* parent)
    : QMainWindow(parent)
    , m_appContext(appContext)
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

    // Main panels are independent widgets; MainWindow only wires them together.
    m_workspaceExplorer = new ui::WorkspaceExplorer(this);
    m_workflowCanvas = new ui::WorkflowCanvas(this);
    m_nodeInspector = new ui::NodeInspector(this);
    m_outputPanel = new ui::OutputPanel(this);
    m_canvasOverlay = buildCanvasOverlay();

    auto* canvasHost = new QWidget(this);
    canvasHost->setObjectName(QStringLiteral("canvasHost"));
    auto* canvasStack = new QStackedLayout(canvasHost);
    canvasStack->setStackingMode(QStackedLayout::StackAll);
    canvasStack->setContentsMargins(0, 0, 0, 0);
    canvasStack->addWidget(m_workflowCanvas);
    canvasStack->addWidget(m_canvasOverlay);

    // UI widgets emit intent; application services and the execution engine do the work.
    connect(m_workflowCanvas, &ui::WorkflowCanvas::nodeSelected, this, [this](const domain::Node& node) {
        m_nodeInspector->displayNode(node);
        updateSelectedNodeStatus(node);
    });
    connect(m_workflowCanvas, &ui::WorkflowCanvas::nodeDoubleClicked, this, &MainWindow::openPythonNodeEditor);
    connect(m_workflowCanvas, &ui::WorkflowCanvas::saveRequested, this, &MainWindow::saveWorkflow);
    connect(m_workflowCanvas, &ui::WorkflowCanvas::workflowChanged, this, [this](const domain::Workflow& workflow) {
        m_currentWorkflow = workflow;
    });
    connect(m_workspaceExplorer, &ui::WorkspaceExplorer::workflowActivated, this, &MainWindow::openWorkflowById);
    connect(&m_appContext.executionEngine().eventBus(), &execution::ExecutionEventBus::nodeStatusChanged, this,
        [this](const QString& runId, const QString& nodeId, const QString& status) {
            m_workflowCanvas->setNodeStatus(nodeId, status);
            m_outputPanel->recordNodeStatus(runId, nodeId, status);
        });
    connect(&m_appContext.executionEngine().eventBus(), &execution::ExecutionEventBus::workflowStatusChanged, this,
        [this](const QString& runId, const QString& status) {
            m_outputPanel->recordWorkflowStatus(runId, status);
        });
    connect(&m_appContext.executionEngine().eventBus(), &execution::ExecutionEventBus::nodeOutputReady, this,
        [this](const QString& runId, const QString& nodeId, const QJsonObject& outputs) {
            m_outputPanel->recordNodeOutput(runId, nodeId, outputs);
        });
    connect(&m_appContext.executionEngine().eventBus(), &execution::ExecutionEventBus::nodeError, this,
        [this](const QString& runId, const QString& nodeId, const QString& message) {
            m_outputPanel->recordNodeError(runId, nodeId, message);
        });
    connect(&m_appContext.executionEngine().eventBus(), &execution::ExecutionEventBus::threadTrace, this,
        [this](const QString& runId, const QString& nodeId, const QString& phase, const QString& threadId, const QString& threadName) {
            m_outputPanel->recordThreadTrace(runId, nodeId, phase, threadId, threadName);
        });

    // Canvas items are painted manually, so they refresh when the theme changes.
    connect(m_themeManager, &ui::ThemeManager::themeChanged, this, [this](ui::AppTheme) {
        if (m_workflowCanvas != nullptr) {
            m_workflowCanvas->refreshTheme();
        }
        updateCanvasOverlay();
    });

    // Compact icon command bar replaces the earlier text-heavy toolbar.
    buildCommandBar();

    // Splitters keep the workspace browser, canvas, inspector, and output panel resizable.
    auto* horizontalSplitter = new QSplitter(Qt::Horizontal, this);
    horizontalSplitter->setObjectName(QStringLiteral("mainHorizontalSplitter"));
    horizontalSplitter->addWidget(m_workspaceExplorer);
    horizontalSplitter->addWidget(canvasHost);
    horizontalSplitter->addWidget(m_nodeInspector);
    horizontalSplitter->setStretchFactor(0, 0);
    horizontalSplitter->setStretchFactor(1, 1);
    horizontalSplitter->setStretchFactor(2, 0);
    horizontalSplitter->setSizes({280, 880, 340});

    auto* verticalSplitter = new QSplitter(Qt::Vertical, this);
    verticalSplitter->setObjectName(QStringLiteral("mainVerticalSplitter"));
    verticalSplitter->addWidget(horizontalSplitter);
    verticalSplitter->addWidget(m_outputPanel);
    verticalSplitter->setStretchFactor(0, 1);
    verticalSplitter->setStretchFactor(1, 0);
    verticalSplitter->setSizes({650, 300});

    auto* centralWrapper = new QWidget(this);
    centralWrapper->setObjectName(QStringLiteral("centralWrapper"));
    auto* centralLayout = new QVBoxLayout(centralWrapper);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_commandBar);
    centralLayout->addWidget(verticalSplitter, 1);
    setCentralWidget(centralWrapper);

    // Status bar shows selected-node runtime info and the workspace Python interpreter.
    m_timeoutStatusLabel = new QLabel(tr("Timeout: -"), this);
    m_timeoutStatusLabel->setObjectName("timeoutStatus");
    m_pythonStatusLabel = new QLabel(tr("Python: not selected"), this);
    m_pythonStatusLabel->setObjectName("pythonStatus");
    statusBar()->addPermanentWidget(m_timeoutStatusLabel);
    statusBar()->addPermanentWidget(m_pythonStatusLabel, 1);
}

void MainWindow::buildCommandBar()
{
    m_commandBar = new ui::CommandBar(this);
    m_commandBar->setWorkspaceInfo(tr("Visual Workflow Studio"));

    m_commandBar->addActionButton(QIcon(":/icons/workspace-new.svg"),
        m_newWorkspaceAction, ui::IconSquareButton::Role::Secondary);
    m_commandBar->addActionButton(QIcon(":/icons/workspace-open.svg"),
        m_openWorkspaceAction, ui::IconSquareButton::Role::Secondary);
    m_commandBar->addActionButton(QIcon(":/icons/python.svg"),
        m_selectPythonAction, ui::IconSquareButton::Role::Secondary);
    m_commandBar->addSeparator();
    m_commandBar->addActionButton(QIcon(":/icons/workflow-new.svg"),
        m_newWorkflowAction, ui::IconSquareButton::Role::Secondary);
    m_commandBar->addActionButton(QIcon(":/icons/save.svg"),
        m_saveWorkflowAction, ui::IconSquareButton::Role::Secondary);
    m_commandBar->addSeparator();
    m_commandBar->addActionButton(QIcon(":/icons/template-save.svg"),
        m_saveTemplateAction, ui::IconSquareButton::Role::Secondary);
    m_commandBar->addActionButton(QIcon(":/icons/link.svg"),
        m_connectNodesAction, ui::IconSquareButton::Role::Secondary);
    m_commandBar->addActionButton(QIcon(":/icons/import.svg"),
        m_importTemplateAction, ui::IconSquareButton::Role::Secondary);
    m_commandBar->addSeparator();
    m_commandBar->addActionButton(QIcon(":/icons/run.svg"),
        m_runAction, ui::IconSquareButton::Role::Primary);
    m_commandBar->addActionButton(QIcon(":/icons/stop.svg"),
        m_cancelRunAction, ui::IconSquareButton::Role::Danger);

    auto* themeButton = m_commandBar->addButton(
        QIcon(":/icons/theme.svg"), tr("Toggle Light / Dark Theme"),
        ui::IconSquareButton::Role::Ghost);
    connect(themeButton, &QPushButton::clicked, m_themeManager, &ui::ThemeManager::toggleTheme);
}

void MainWindow::applyInitialTheme()
{
    m_themeManager->applyTheme(ui::AppTheme::Light);
}

QWidget* MainWindow::buildCanvasOverlay()
{
    auto* overlay = new QWidget(this);
    overlay->setObjectName("canvasOverlay");
    overlay->setAutoFillBackground(true);
    // Background color will be set by applyInitialTheme() and updated on theme change.
    // QSS for this widget is not used; we set the palette directly for overlay transparency.

    m_canvasOverlayTitle = new QLabel(overlay);
    m_canvasOverlayTitle->setObjectName("canvasOverlayTitle");
    m_canvasOverlayTitle->setAlignment(Qt::AlignCenter);

    m_overlayPrimaryButton = new QPushButton(overlay);
    m_overlayPrimaryButton->setObjectName("overlayPrimaryButton");
    m_overlayPrimaryButton->setProperty("buttonRole", "primary");

    m_overlaySecondaryButton = new QPushButton(overlay);
    m_overlaySecondaryButton->setObjectName("overlaySecondaryButton");
    m_overlaySecondaryButton->setProperty("buttonRole", "secondary");

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_overlayPrimaryButton);
    buttonLayout->addWidget(m_overlaySecondaryButton);
    buttonLayout->addStretch(1);

    auto* layout = new QVBoxLayout(overlay);
    layout->addStretch(1);
    layout->addWidget(m_canvasOverlayTitle);
    layout->addSpacing(12);
    layout->addLayout(buttonLayout);
    layout->addStretch(1);

    return overlay;
}

void MainWindow::updateCanvasOverlay()
{
    if (m_canvasOverlay == nullptr) {
        return;
    }

    // Update overlay background from theme (QSS handles the rest)
    if (m_themeManager != nullptr) {
        const auto overlayBg = m_themeManager->color("overlay-bg");
        m_canvasOverlay->setStyleSheet(
            QStringLiteral("QWidget#canvasOverlay { background: %1; }")
                .arg(overlayBg.name(QColor::HexArgb)));
    }

    disconnect(m_overlayPrimaryButton, nullptr, this, nullptr);
    disconnect(m_overlaySecondaryButton, nullptr, this, nullptr);

    if (m_currentWorkspace.rootPath.isEmpty()) {
        m_canvasOverlayTitle->setText(tr("No workspace is open"));
        m_overlayPrimaryButton->setText(tr("New Workspace"));
        m_overlaySecondaryButton->setText(tr("Open Workspace"));
        connect(m_overlayPrimaryButton, &QPushButton::clicked, this, &MainWindow::createWorkspace);
        connect(m_overlaySecondaryButton, &QPushButton::clicked, this, &MainWindow::openWorkspace);
        if (m_commandBar != nullptr) {
            m_commandBar->setWorkspaceInfo(tr("Visual Workflow Studio"));
            m_commandBar->setWorkflowInfo(tr("No workspace"));
        }
        m_canvasOverlay->show();
        m_canvasOverlay->raise();
        return;
    }

    if (m_commandBar != nullptr) {
        m_commandBar->setWorkspaceInfo(m_currentWorkspace.name);
    }

    if (m_currentWorkflow.workflowId.isEmpty()) {
        m_canvasOverlayTitle->setText(tr("No workflow is open"));
        m_overlayPrimaryButton->setText(tr("New Workflow"));
        m_overlaySecondaryButton->setText(tr("Open Workflow"));
        connect(m_overlayPrimaryButton, &QPushButton::clicked, this, &MainWindow::createWorkflow);
        connect(m_overlaySecondaryButton, &QPushButton::clicked, this, &MainWindow::loadWorkflow);
        if (m_commandBar != nullptr) {
            m_commandBar->setWorkflowInfo(tr("No workflow"));
        }
        m_canvasOverlay->show();
        m_canvasOverlay->raise();
        return;
    }

    if (m_commandBar != nullptr) {
        m_commandBar->setWorkflowInfo(m_currentWorkflow.name);
    }

    m_canvasOverlay->hide();
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
    if (!m_appContext.workspaceService().createWorkspace(rootPath, workspaceName, m_currentWorkspace, &errorMessage)) {
        QMessageBox::warning(this, tr("Workspace Error"), errorMessage);
        return;
    }

    m_currentWorkflow = {};
    m_workflowCanvas->clearWorkflow();
    applyWorkspacePythonExecutable();
    refreshWorkspaceExplorer();
    updateCanvasOverlay();
    m_outputPanel->appendStdout(tr("Created workspace: %1").arg(m_currentWorkspace.rootPath));
}

void MainWindow::openWorkspace()
{
    const auto rootPath = QFileDialog::getExistingDirectory(this, tr("Open Workspace Directory"));
    if (rootPath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_appContext.workspaceService().openWorkspace(rootPath, m_currentWorkspace, &errorMessage)) {
        QMessageBox::warning(this, tr("Workspace Error"), errorMessage);
        return;
    }

    m_currentWorkflow = {};
    m_workflowCanvas->clearWorkflow();
    applyWorkspacePythonExecutable();
    refreshWorkspaceExplorer();
    updateCanvasOverlay();
    m_outputPanel->appendStdout(tr("Opened workspace: %1").arg(m_currentWorkspace.rootPath));
}

void MainWindow::selectPythonInterpreter()
{
    if (!ensureWorkspaceOpen()) {
        return;
    }

    const auto currentPath = m_appContext.workspaceService().pythonExecutable(m_currentWorkspace);
    const auto selectedPath = QFileDialog::getOpenFileName(
        this,
        tr("Select Python Interpreter"),
        currentPath,
        tr("Python executable (python.exe);;All files (*.*)"));
    if (selectedPath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_appContext.workspaceService().updatePythonExecutable(m_currentWorkspace, selectedPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Workspace Error"), errorMessage);
        return;
    }

    applyWorkspacePythonExecutable();
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

    m_currentWorkflow = m_appContext.workflowService().createEmptyWorkflow(m_currentWorkspace.id, workflowName);
    m_workflowCanvas->setWorkflow(m_currentWorkflow);
    updateCanvasOverlay();

    saveWorkflow();
    m_outputPanel->appendStdout(tr("Created workflow: %1").arg(m_currentWorkflow.name));
}

void MainWindow::loadWorkflow()
{
    if (!ensureWorkspaceOpen()) {
        return;
    }

    const auto filePath = QFileDialog::getOpenFileName(
        this,
        tr("Load Workflow"),
        QDir(m_currentWorkspace.rootPath).filePath("workflows"),
        tr("Workflow JSON (*.json);;All files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_appContext.workflowService().loadWorkflow(filePath, m_currentWorkflow, &errorMessage)) {
        QMessageBox::warning(this, tr("Workflow Error"), errorMessage);
        return;
    }

    if (m_currentWorkflow.workspaceId.isEmpty()) {
        m_currentWorkflow.workspaceId = m_currentWorkspace.id;
    }
    m_workflowCanvas->setWorkflow(m_currentWorkflow);
    updateCanvasOverlay();
    m_outputPanel->appendStdout(tr("Loaded workflow: %1").arg(m_currentWorkflow.name));
}

void MainWindow::openWorkflowById(const QString& workflowId)
{
    if (!ensureWorkspaceOpen() || workflowId.trimmed().isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_appContext.workflowService().loadWorkflowFromWorkspace(
            m_currentWorkspace.rootPath,
            workflowId,
            m_currentWorkflow,
            &errorMessage)) {
        QMessageBox::warning(this, tr("Workflow Error"), errorMessage);
        return;
    }

    if (m_currentWorkflow.workspaceId.isEmpty()) {
        m_currentWorkflow.workspaceId = m_currentWorkspace.id;
    }
    m_workflowCanvas->setWorkflow(m_currentWorkflow);
    updateCanvasOverlay();
    m_outputPanel->appendStdout(tr("Loaded workflow: %1").arg(m_currentWorkflow.name));
}

void MainWindow::saveWorkflow()
{
    if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
        return;
    }

    m_currentWorkflow = m_workflowCanvas->workflow();

    QString errorMessage;
    if (!m_appContext.workflowService().saveWorkflowToWorkspace(m_currentWorkspace.rootPath, m_currentWorkflow, &errorMessage)) {
        QMessageBox::warning(this, tr("Workflow Error"), errorMessage);
        return;
    }

    refreshWorkspaceExplorer();
    m_outputPanel->appendStdout(tr("Saved workflow: %1").arg(m_currentWorkflow.name));
}

void MainWindow::saveSelectedNodeAsTemplate()
{
    if (!ensureWorkspaceOpen() || !ensureWorkflowOpen()) {
        return;
    }

    m_currentWorkflow = m_workflowCanvas->workflow();
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

    const auto nodeTemplate = m_appContext.nodeTemplateService().createTemplateFromNode(
        m_currentWorkspace.id,
        selectedNode.value(),
        templateName);

    QString errorMessage;
    if (!m_appContext.nodeTemplateService().saveTemplate(m_currentWorkspace.rootPath, nodeTemplate, &errorMessage)) {
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
        QDir(m_currentWorkspace.rootPath).filePath("node_templates"),
        tr("Node Template JSON (*.json);;All files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }

    domain::NodeTemplate nodeTemplate;
    QString errorMessage;
    if (!m_appContext.nodeTemplateService().loadTemplate(filePath, nodeTemplate, &errorMessage)) {
        QMessageBox::warning(this, tr("Template Error"), errorMessage);
        return;
    }

    auto node = m_appContext.nodeTemplateService().createNodeFromTemplate(nodeTemplate);
    node.position.x = 80.0 * m_currentWorkflow.nodes.size();
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
    if (!m_appContext.nodeTemplateService().importTemplateFile(filePath, m_currentWorkspace.rootPath, importedTemplate, &errorMessage)) {
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
    if (m_workflowRunning) {
        m_outputPanel->appendStderr(tr("A workflow is already running."));
        return;
    }

    m_currentWorkflow = m_workflowCanvas->workflow();
    applyWorkspacePythonExecutable();
    if (m_appContext.workspaceService().pythonExecutable(m_currentWorkspace).trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Run Workflow"), tr("Select a Python interpreter for this workspace before running a workflow."));
        m_outputPanel->appendStderr(tr("Run blocked: no Python interpreter selected for this workspace."));
        return;
    }
    m_outputPanel->clearRun();
    m_outputPanel->setWorkflowName(m_currentWorkflow.name);
    QHash<QString, QString> nodeNames;
    for (const auto& node : m_currentWorkflow.nodes) {
        nodeNames.insert(node.nodeId, node.name);
    }
    m_outputPanel->setNodeNames(nodeNames);

    const auto workflowToRun = m_currentWorkflow;
    const auto workspaceRootPath = m_currentWorkspace.rootPath;
    const auto workspaceId = m_currentWorkspace.id;
    const auto workflowId = m_currentWorkflow.workflowId;
    const auto runRootPath = QDir(workspaceRootPath).filePath("runs");
    const auto artifactPath = QDir(m_currentWorkspace.rootPath).filePath("artifacts");
    if (!QDir().mkpath(artifactPath)) {
        QMessageBox::warning(this, tr("Run Workflow"), tr("Could not create artifact directory: %1").arg(QDir::toNativeSeparators(artifactPath)));
        return;
    }
    m_workflowRunning = true;
    m_outputPanel->appendStdout(tr("Run started in background."));

    m_appContext.executionEngine().runWorkflowAsync(
        workflowToRun,
        workspaceRootPath,
        runRootPath,
        artifactPath,
        this,
        [this, workspaceRootPath, workspaceId, workflowId](execution::WorkflowExecutionResult result) {
            m_workflowRunning = false;
            m_outputPanel->showExecutionResult(result);

            QString runRecordError;
            if (!m_appContext.runService().saveRunRecord(
                    workspaceRootPath,
                    workspaceId,
                    workflowId,
                    result,
                    &runRecordError)) {
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
    if (!m_workflowRunning) {
        m_outputPanel->appendStdout(tr("No workflow is currently running."));
        return;
    }

    m_appContext.executionEngine().requestCancelCurrentRun();
    m_outputPanel->appendStderr(tr("Cancellation requested for the running workflow."));
}

void MainWindow::openPythonNodeEditor(const domain::Node& node)
{
    const auto language = node.config.value("language").toString("python");
    const auto nodeType = node.type.trimmed().toLower();
    const auto isPythonCodeNode = nodeType == "function" || nodeType == "starter" || nodeType == "agent";
    if (!isPythonCodeNode || language != "python") {
        QMessageBox::information(this, tr("Python Editor"), tr("Only Python-based starter, function, and agent nodes can be edited here."));
        return;
    }

    auto* dialog = new ui::PythonNodeEditorDialog(
        node.name,
        node.description,
        node.type,
        node.config,
        node.config.value("code").toString(),
        ui::PythonCodeTemplates::defaultCodeForNodeType(node.type),
        this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);

    // Keep the editor modeless. Saving is deferred below so Qt does not mutate the canvas while a graphics-item double-click event is still unwinding.
    connect(dialog, &ui::PythonNodeEditorDialog::nodeSaved, this,
        [this, nodeId = node.nodeId](const QString& name, const QString& description, const QString& code, const QJsonObject& configPatch) {
        // The save button/close prompt is also inside a UI event. Defer canvas mutation to the next event turn.
        QTimer::singleShot(0, this, [this, nodeId, name, description, code, configPatch]() {
            m_currentWorkflow = m_workflowCanvas->workflow();
            QString errorMessage;
            if (!m_appContext.workflowService().updateNodeDetails(m_currentWorkflow, nodeId, name, description, code, configPatch, &errorMessage)) {
                QMessageBox::warning(this, tr("Save Python Code"), errorMessage);
                return;
            }

            for (const auto& updatedNode : m_currentWorkflow.nodes) {
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
    if (m_currentWorkspace.rootPath.isEmpty()) {
        return;
    }

    QString errorMessage;
    const auto workflows = m_appContext.workflowService().listWorkflows(m_currentWorkspace.rootPath, &errorMessage);
    if (!errorMessage.isEmpty()) {
        m_outputPanel->appendStderr(errorMessage);
    }

    errorMessage.clear();
    const auto templates = m_appContext.nodeTemplateService().listTemplates(m_currentWorkspace.rootPath, &errorMessage);
    if (!errorMessage.isEmpty()) {
        m_outputPanel->appendStderr(errorMessage);
    }

    QStringList workflowNames;
    QStringList workflowIds;
    for (const auto& workflow : workflows) {
        workflowNames.append(workflow.name.isEmpty() ? workflow.workflowId : workflow.name);
        workflowIds.append(workflow.workflowId);
    }

    QStringList templateNames;
    for (const auto& nodeTemplate : templates) {
        templateNames.append(nodeTemplate.name.isEmpty() ? nodeTemplate.templateId : nodeTemplate.name);
    }

    const auto runNames = m_appContext.runService().recentRuns(m_currentWorkspace.rootPath);
    m_workspaceExplorer->setWorkspaceData(m_currentWorkspace.name, workflowNames, workflowIds, templateNames, runNames);
}

void MainWindow::applyWorkspacePythonExecutable()
{
    const auto pythonExecutable = m_appContext.workspaceService().pythonExecutable(m_currentWorkspace).trimmed();
    m_appContext.setPythonExecutable(pythonExecutable);
    updatePythonStatus();
}

void MainWindow::updatePythonStatus()
{
    if (m_pythonStatusLabel == nullptr) {
        return;
    }

    const auto pythonExecutable = m_appContext.workspaceService().pythonExecutable(m_currentWorkspace).trimmed();
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
    if (!m_currentWorkspace.rootPath.isEmpty()) {
        return true;
    }

    QMessageBox::information(this, tr("Workspace Required"), tr("Create or open a workspace first."));
    return false;
}

bool MainWindow::ensureWorkflowOpen()
{
    if (!m_currentWorkflow.workflowId.isEmpty()) {
        return true;
    }

    QMessageBox::information(this, tr("Workflow Required"), tr("Create or load a workflow first."));
    return false;
}

} // namespace vws
