#include "application/NodeTemplateService.h"
#include "application/RunService.h"
#include "application/WorkflowService.h"
#include "application/WorkspaceService.h"
#include "presentation/controllers/NodeTemplateController.h"
#include "presentation/controllers/PythonEnvironmentController.h"
#include "presentation/controllers/WorkspaceBrowserController.h"
#include "presentation/controllers/WorkflowController.h"
#include "presentation/models/WorkflowDisplayModel.h"
#include "presentation/controllers/WorkspaceController.h"
#include "presentation/state/AppStore.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << message << Qt::endl;
    return 1;
}

int expect(bool condition, const QString& message)
{
    return condition ? 0 : fail(message);
}

} // namespace

int main()
{
    vws::presentation::AppStore store;

    store.setActiveRunWorkflowId("workflow-a");
    store.rememberRunWorkflow("run-a", "workflow-a");
    store.cacheNodeStatus("workflow-a", "node-a", "running");
    store.setWorkflowRunning("workflow-a", true);
    if (const auto check = expect(store.workflowIdForRun("run-a") == "workflow-a",
            "AppStore should resolve run id to workflow id")) {
        return check;
    }
    if (const auto check = expect(store.nodeStatusesByWorkflowId().value("workflow-a").value("node-a") == "running",
            "AppStore should own cached node status by workflow")) {
        return check;
    }
    if (const auto check = expect(store.runningWorkflowIds().contains("workflow-a"),
            "AppStore should track running workflow ids")) {
        return check;
    }

    QTemporaryDir workspaceDir;
    if (const auto check = expect(workspaceDir.isValid(), "Temporary workspace should be valid")) {
        return check;
    }

    vws::application::WorkspaceService workspaceService;
    vws::application::WorkflowService workflowService;
    vws::application::NodeTemplateService nodeTemplateService;
    vws::application::RunService runService;
    vws::presentation::WorkspaceController workspaceController(workspaceService, store);
    QString appliedPythonExecutable;
    vws::presentation::PythonEnvironmentController pythonEnvironmentController(
        workspaceService,
        store,
        [&appliedPythonExecutable](const QString& pythonExecutable) {
            appliedPythonExecutable = pythonExecutable;
        });
    vws::presentation::WorkflowController workflowController(workflowService, store);
    vws::presentation::NodeTemplateController nodeTemplateController(nodeTemplateService, store);
    vws::presentation::WorkspaceBrowserController workspaceBrowserController(
        workflowService,
        nodeTemplateService,
        runService,
        store);

    QString errorMessage;
    if (const auto check = expect(workspaceController.createWorkspace(workspaceDir.path(), "Presentation Test", &errorMessage),
            QString("WorkspaceController should create workspace: %1").arg(errorMessage))) {
        return check;
    }
    if (const auto check = expect(store.currentWorkspace().rootPath == workspaceDir.path(),
            "WorkspaceController should update AppStore current workspace")) {
        return check;
    }
    if (const auto check = expect(store.currentWorkflow().workflowId.isEmpty(),
            "Opening a workspace should clear current workflow state")) {
        return check;
    }
    if (const auto check = expect(pythonEnvironmentController.updatePythonExecutable("C:/Python/test/python.exe", &errorMessage),
            QString("PythonEnvironmentController should persist interpreter path: %1").arg(errorMessage))) {
        return check;
    }
    if (const auto check = expect(
            pythonEnvironmentController.pythonExecutable() == "C:/Python/test/python.exe"
                && appliedPythonExecutable == "C:/Python/test/python.exe",
            "PythonEnvironmentController should persist and apply the workspace interpreter")) {
        return check;
    }

    workflowController.createWorkflow("Workflow A");
    if (const auto check = expect(store.currentWorkflow().name == "Workflow A",
            "WorkflowController should create current workflow in AppStore")) {
        return check;
    }
    if (const auto check = expect(store.workflowDocument().isDirty(),
            "Newly created workflow should be dirty until saved")) {
        return check;
    }
    if (const auto check = expect(workflowController.saveCurrentWorkflow(&errorMessage),
            QString("WorkflowController should save current workflow: %1").arg(errorMessage))) {
        return check;
    }
    if (const auto check = expect(!store.workflowDocument().isDirty(),
            "Saving current workflow should mark the document clean")) {
        return check;
    }

    auto canvasSnapshot = store.currentWorkflowSnapshot();
    canvasSnapshot.name = "Workflow A Edited On Canvas";
    workflowController.syncCurrentWorkflowFromView(canvasSnapshot);
    if (const auto check = expect(store.workflowDocument().isDirty()
            && store.currentWorkflow().name == "Workflow A Edited On Canvas",
            "Synchronizing a canvas snapshot should update and dirty the document")) {
        return check;
    }
    const auto workflowId = store.currentWorkflow().workflowId;
    if (const auto check = expect(workflowController.saveCurrentWorkflow(&errorMessage),
            QString("WorkflowController should save edited current workflow: %1").arg(errorMessage))) {
        return check;
    }
    if (const auto check = expect(workflowController.loadWorkflowFromWorkspace(workflowId, &errorMessage),
            QString("WorkflowController should reload saved workflow: %1").arg(errorMessage))) {
        return check;
    }
    if (const auto check = expect(!store.workflowDocument().isDirty(),
            "Loading a workflow from persistence should leave the document clean")) {
        return check;
    }

    vws::domain::Node templateSourceNode;
    templateSourceNode.nodeId = "node-template-source";
    templateSourceNode.name = "Reusable Node";
    templateSourceNode.type = "function";
    templateSourceNode.inputPorts = {"input"};
    templateSourceNode.outputPorts = {"output"};

    vws::domain::NodeTemplate savedTemplate;
    if (const auto check = expect(nodeTemplateController.saveTemplateFromNode(
            templateSourceNode,
            "Reusable Template",
            &savedTemplate,
            &errorMessage),
            QString("NodeTemplateController should save selected node as template: %1").arg(errorMessage))) {
        return check;
    }
    if (const auto check = expect(!savedTemplate.templateId.isEmpty(),
            "Saved node template should have a template id")) {
        return check;
    }

    errorMessage.clear();
    const auto templates = nodeTemplateController.listTemplates(&errorMessage);
    if (const auto check = expect(templates.size() == 1 && templates.first().templateId == savedTemplate.templateId,
            QString("NodeTemplateController should list saved templates: %1").arg(errorMessage))) {
        return check;
    }

    vws::domain::Node nodeFromWorkspaceTemplate;
    vws::domain::NodeTemplate sourceTemplate;
    if (const auto check = expect(nodeTemplateController.createNodeFromWorkspaceTemplate(
            savedTemplate.templateId,
            nodeFromWorkspaceTemplate,
            &sourceTemplate,
            &errorMessage),
            QString("NodeTemplateController should create node from workspace template: %1").arg(errorMessage))) {
        return check;
    }
    if (const auto check = expect(nodeFromWorkspaceTemplate.templateId == savedTemplate.templateId
            && nodeFromWorkspaceTemplate.nodeId != templateSourceNode.nodeId,
            "Creating a node from template should create a fresh node id and preserve template id")) {
        return check;
    }

    const auto templatePath = QDir(workspaceDir.path()).filePath(
        QString("node_templates/%1.json").arg(savedTemplate.templateId));
    if (const auto check = expect(QFileInfo::exists(templatePath),
            "Saved node template file should exist")) {
        return check;
    }

    vws::domain::Node importedNode;
    if (const auto check = expect(nodeTemplateController.createNodeFromTemplateFile(
            templatePath,
            importedNode,
            nullptr,
            &errorMessage),
            QString("NodeTemplateController should create node from template file: %1").arg(errorMessage))) {
        return check;
    }
    if (const auto check = expect(importedNode.name == savedTemplate.name,
            "Creating a node from template file should use the template name")) {
        return check;
    }

    const auto browserSnapshot = workspaceBrowserController.snapshot();
    if (const auto check = expect(
            browserSnapshot.workflowIds.contains(workflowId)
                && browserSnapshot.templateIds.contains(savedTemplate.templateId)
                && browserSnapshot.errors.isEmpty(),
            "WorkspaceBrowserController should build browser rows from the current workspace")) {
        return check;
    }
    auto workflowForDisplay = store.currentWorkflow();
    workflowForDisplay.nodes.append(templateSourceNode);
    const auto displayModel = vws::presentation::WorkflowDisplayModelBuilder::build(workflowForDisplay);
    if (const auto check = expect(
            displayModel.workflowName == workflowForDisplay.name
                && displayModel.nodeNamesById.value(templateSourceNode.nodeId) == templateSourceNode.name,
            "WorkflowDisplayModelBuilder should expose workflow output-panel labels")) {
        return check;
    }

    QTextStream(stdout) << "presentation state tests passed" << Qt::endl;
    return 0;
}
