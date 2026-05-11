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
#include <QToolBar>
#include <QVBoxLayout>

namespace vws {

MainWindow::MainWindow(AppContext& appContext, QWidget* parent)
    : QMainWindow(parent)
    , m_appContext(appContext)
{
    setWindowTitle("Visual Workflow Studio");
    buildActions();
    buildLayout();
    updateCanvasOverlay();
}

void MainWindow::buildActions()
{
    // 菜单动作只负责收集用户输入，再转给 application service。
    // 这样 UI 不直接知道 JSON 怎么存、模板怎么导入，业务规则仍然留在 application 层。
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* workflowMenu = menuBar()->addMenu(tr("&Workflow"));

    auto* newWorkspaceAction = fileMenu->addAction(tr("New Workspace"));
    auto* openWorkspaceAction = fileMenu->addAction(tr("Open Workspace"));
    auto* selectPythonAction = fileMenu->addAction(tr("Select Python Interpreter"));
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction(tr("Exit"));

    auto* newWorkflowAction = workflowMenu->addAction(tr("New Workflow"));
    auto* loadWorkflowAction = workflowMenu->addAction(tr("Load Workflow"));
    auto* saveWorkflowAction = workflowMenu->addAction(tr("Save Workflow"));
    workflowMenu->addSeparator();
    auto* saveTemplateAction = workflowMenu->addAction(tr("Save Selected Node As Template"));
    auto* addNodeFromTemplateAction = workflowMenu->addAction(tr("Add Node From Template"));
    auto* connectNodesAction = workflowMenu->addAction(tr("Connect Selected Nodes"));
    auto* importTemplateAction = workflowMenu->addAction(tr("Import Node Template"));
    workflowMenu->addSeparator();
    auto* runAction = workflowMenu->addAction(tr("Run Workflow"));
    auto* cancelRunAction = workflowMenu->addAction(tr("Cancel Run"));

    auto* toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->addAction(newWorkspaceAction);
    toolbar->addAction(openWorkspaceAction);
    toolbar->addAction(selectPythonAction);
    toolbar->addSeparator();
    toolbar->addAction(newWorkflowAction);
    toolbar->addAction(saveWorkflowAction);
    toolbar->addSeparator();
    toolbar->addAction(saveTemplateAction);
    toolbar->addAction(connectNodesAction);
    toolbar->addAction(importTemplateAction);
    toolbar->addSeparator();
    toolbar->addAction(runAction);
    toolbar->addAction(cancelRunAction);

    connect(newWorkspaceAction, &QAction::triggered, this, &MainWindow::createWorkspace);
    connect(openWorkspaceAction, &QAction::triggered, this, &MainWindow::openWorkspace);
    connect(selectPythonAction, &QAction::triggered, this, &MainWindow::selectPythonInterpreter);
    connect(newWorkflowAction, &QAction::triggered, this, &MainWindow::createWorkflow);
    connect(loadWorkflowAction, &QAction::triggered, this, &MainWindow::loadWorkflow);
    connect(saveWorkflowAction, &QAction::triggered, this, &MainWindow::saveWorkflow);
    connect(saveTemplateAction, &QAction::triggered, this, &MainWindow::saveSelectedNodeAsTemplate);
    connect(addNodeFromTemplateAction, &QAction::triggered, this, &MainWindow::addNodeFromTemplate);
    connect(connectNodesAction, &QAction::triggered, this, &MainWindow::connectSelectedNodes);
    connect(importTemplateAction, &QAction::triggered, this, &MainWindow::importNodeTemplate);
    connect(runAction, &QAction::triggered, this, &MainWindow::runCurrentWorkflow);
    connect(cancelRunAction, &QAction::triggered, this, &MainWindow::cancelCurrentWorkflowRun);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
}

void MainWindow::buildLayout()
{
    // 四个主要区域各自是独立控件：
    // 左：工作区资源树；中：工作流画布；右：节点配置；下：运行输出。
    m_workspaceExplorer = new ui::WorkspaceExplorer(this);
    m_workflowCanvas = new ui::WorkflowCanvas(this);
    m_nodeInspector = new ui::NodeInspector(this);
    m_outputPanel = new ui::OutputPanel(this);
    m_canvasOverlay = buildCanvasOverlay();

    auto* canvasHost = new QWidget(this);
    auto* canvasStack = new QStackedLayout(canvasHost);
    canvasStack->setStackingMode(QStackedLayout::StackAll);
    canvasStack->setContentsMargins(0, 0, 0, 0);
    canvasStack->addWidget(m_workflowCanvas);
    canvasStack->addWidget(m_canvasOverlay);

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

    auto* horizontalSplitter = new QSplitter(Qt::Horizontal, this);
    horizontalSplitter->addWidget(m_workspaceExplorer);
    horizontalSplitter->addWidget(canvasHost);
    horizontalSplitter->addWidget(m_nodeInspector);
    horizontalSplitter->setStretchFactor(0, 0);
    horizontalSplitter->setStretchFactor(1, 1);
    horizontalSplitter->setStretchFactor(2, 0);
    horizontalSplitter->setSizes({260, 880, 320});

    auto* verticalSplitter = new QSplitter(Qt::Vertical, this);
    verticalSplitter->addWidget(horizontalSplitter);
    verticalSplitter->addWidget(m_outputPanel);
    verticalSplitter->setStretchFactor(0, 1);
    verticalSplitter->setStretchFactor(1, 0);
    verticalSplitter->setSizes({650, 250});

    setCentralWidget(verticalSplitter);

    m_timeoutStatusLabel = new QLabel(tr("Timeout: -"), this);
    m_pythonStatusLabel = new QLabel(tr("Python: not selected"), this);
    statusBar()->addPermanentWidget(m_timeoutStatusLabel);
    statusBar()->addPermanentWidget(m_pythonStatusLabel, 1);
}

QWidget* MainWindow::buildCanvasOverlay()
{
    auto* overlay = new QWidget(this);
    overlay->setAutoFillBackground(true);
    overlay->setStyleSheet(
        "QWidget { background: rgba(210, 210, 210, 190); }"
        "QLabel { color: white; font-size: 20px; font-weight: 600; background: transparent; }"
        "QPushButton { padding: 8px 18px; background: #f8fafc; color: #111827; border: 1px solid #cbd5e1; border-radius: 4px; }"
        "QPushButton:hover { background: #e2e8f0; }");

    m_canvasOverlayTitle = new QLabel(overlay);
    m_canvasOverlayTitle->setAlignment(Qt::AlignCenter);

    m_overlayPrimaryButton = new QPushButton(overlay);
    m_overlaySecondaryButton = new QPushButton(overlay);

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

    disconnect(m_overlayPrimaryButton, nullptr, this, nullptr);
    disconnect(m_overlaySecondaryButton, nullptr, this, nullptr);

    if (m_currentWorkspace.rootPath.isEmpty()) {
        m_canvasOverlayTitle->setText(tr("No workspace is open"));
        m_overlayPrimaryButton->setText(tr("New Workspace"));
        m_overlaySecondaryButton->setText(tr("Open Workspace"));
        connect(m_overlayPrimaryButton, &QPushButton::clicked, this, &MainWindow::createWorkspace);
        connect(m_overlaySecondaryButton, &QPushButton::clicked, this, &MainWindow::openWorkspace);
        m_canvasOverlay->show();
        m_canvasOverlay->raise();
        return;
    }

    if (m_currentWorkflow.workflowId.isEmpty()) {
        m_canvasOverlayTitle->setText(tr("No workflow is open"));
        m_overlayPrimaryButton->setText(tr("New Workflow"));
        m_overlaySecondaryButton->setText(tr("Open Workflow"));
        connect(m_overlayPrimaryButton, &QPushButton::clicked, this, &MainWindow::createWorkflow);
        connect(m_overlaySecondaryButton, &QPushButton::clicked, this, &MainWindow::loadWorkflow);
        m_canvasOverlay->show();
        m_canvasOverlay->raise();
        return;
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

    // 用 modeless 窗口而不是 exec()。exec() 会在 QGraphicsItem 的双击事件尚未返回时
    // 开启嵌套事件循环；如果用户在这个嵌套循环里点 Save，我们会反过来更新刚才被
    // 双击的图元，容易触发 Qt 图元事件栈重入。Starter 节点连续创建/保存时的闪退
    // 就是这个时序问题暴露出来的。
    connect(dialog, &ui::PythonNodeEditorDialog::nodeSaved, this,
        [this, nodeId = node.nodeId](const QString& name, const QString& description, const QString& code, const QJsonObject& configPatch) {
        // Save 按钮/关闭确认框本身也在处理 UI 事件。把真正的 Canvas 更新推迟到
        // 下一轮事件循环，可以避免在按钮事件或关闭事件栈里直接修改 QGraphicsScene。
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
