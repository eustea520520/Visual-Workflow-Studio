#include "application/NodeTemplateService.h"
#include "application/RunService.h"
#include "application/subsystem/SubsystemService.h"
#include "application/WorkflowService.h"
#include "application/WorkspaceService.h"
#include "presentation/controllers/CanvasNavigationController.h"
#include "presentation/controllers/CanvasSessionController.h"
#include "presentation/controllers/NodeTemplateController.h"
#include "presentation/controllers/PythonEnvironmentController.h"
#include "presentation/controllers/WorkspaceBrowserController.h"
#include "presentation/controllers/WorkflowController.h"
#include "presentation/controllers/WorkflowIoController.h"
#include "presentation/models/WorkflowDisplayModel.h"
#include "presentation/controllers/WorkspaceController.h"
#include "presentation/state/AppStore.h"
#include "domain/WorkflowJsonParser.h"
#include "infrastructure/JsonUtils.h"

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

vws::domain::PortDimensionSpec portSpec(const QString& portName, int dimension, const QStringList& labels = {})
{
    vws::domain::PortDimensionSpec spec;
    spec.portName = portName;
    spec.dimension = dimension;
    spec.itemLabels = labels;
    return spec;
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
    vws::presentation::WorkflowIoController workflowIoController;

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

    auto oldRunSnapshotJson = store.currentWorkflow().toJson();
    oldRunSnapshotJson.insert("schema_version", 1);
    const auto oldRunSnapshotPath = QDir(workspaceDir.path()).filePath("old_run_snapshot.json");
    if (const auto check = expect(vws::infrastructure::JsonUtils::writeObjectToFile(
            oldRunSnapshotPath,
            oldRunSnapshotJson,
            &errorMessage),
            QString("Should write old run snapshot fixture: %1").arg(errorMessage))) {
        return check;
    }
    vws::domain::RunRecord oldSnapshotRecord;
    oldSnapshotRecord.workflowSnapshotPath = oldRunSnapshotPath;
    oldSnapshotRecord.workflowId = store.currentWorkflow().workflowId;
    vws::domain::Workflow rejectedRunSnapshot;
    bool usedCurrentWorkflowFile = false;
    if (const auto check = expect(!workflowController.loadWorkflowSnapshotForRun(
            oldSnapshotRecord,
            rejectedRunSnapshot,
            &usedCurrentWorkflowFile,
            &errorMessage)
            && errorMessage == vws::domain::WorkflowJsonParser::unreadableWorkspaceMessage(),
            "WorkflowController should reject old-schema run snapshots instead of migrating them")) {
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

    auto unsavedWorkflowA = store.currentWorkflowSnapshot();
    unsavedWorkflowA.name = "Workflow A Unsaved In Memory";
    workflowController.syncCurrentWorkflowFromView(unsavedWorkflowA);
    if (const auto check = expect(store.workflowDocument().isDirty(),
            "Editing workflow A in memory should dirty its document")) {
        return check;
    }

    workflowController.createWorkflow("Workflow B");
    const auto workflowBId = store.currentWorkflow().workflowId;
    if (const auto check = expect(store.currentWorkflow().name == "Workflow B",
            "Creating workflow B should switch the active document")) {
        return check;
    }
    if (const auto check = expect(workflowController.loadWorkflowFromWorkspace(workflowId, &errorMessage),
            QString("WorkflowController should reactivate open unsaved workflow A: %1").arg(errorMessage))) {
        return check;
    }
    if (const auto check = expect(store.currentWorkflow().name == "Workflow A Unsaved In Memory"
            && store.workflowDocument().isDirty(),
            "Switching back to an already open workflow should restore its unsaved in-memory document")) {
        return check;
    }
    if (const auto check = expect(workflowController.loadWorkflowFromWorkspace(workflowBId, &errorMessage),
            QString("WorkflowController should switch back to open workflow B: %1").arg(errorMessage))) {
        return check;
    }
    if (const auto check = expect(store.currentWorkflow().workflowId == workflowBId,
            "Open workflow documents should be addressable without closing each other")) {
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

    vws::domain::Workflow ioWorkflow;
    ioWorkflow.workflowId = "io-workflow";
    vws::domain::Node sourceNode;
    sourceNode.nodeId = "source";
    sourceNode.type = "starter";
    sourceNode.outputPorts = {"output"};
    sourceNode.ioSpec.outputs.append(portSpec("output", 3, {"a", "b", "c"}));
    vws::domain::Node targetNode;
    targetNode.nodeId = "target";
    targetNode.type = "function";
    targetNode.inputPorts = {"input"};
    targetNode.outputPorts = {"output"};
    targetNode.ioSpec.inputs.append(portSpec("input", 1, {"only"}));
    ioWorkflow.nodes = {sourceNode, targetNode};
    vws::domain::Edge slotEdge;
    slotEdge.edgeId = "slot-edge";
    slotEdge.fromNode = "source";
    slotEdge.fromPort = "output";
    slotEdge.fromSlot = 0;
    slotEdge.toNode = "target";
    slotEdge.toPort = "input";
    slotEdge.toSlot = 0;
    ioWorkflow.edges = {slotEdge};
    const auto oneSlotSpecs = workflowIoController.visualSpecsForWorkflow(ioWorkflow);
    if (const auto check = expect(oneSlotSpecs.value("target").inputs.first().dimension == 1
            && oneSlotSpecs.value("target").inputs.first().itemLabels == QStringList({"only"}),
            "Slot edges should not expand downstream input dimensions or labels")) {
        return check;
    }

    ioWorkflow.edges[0].fromSlot = 1;
    ioWorkflow.edges[0].toSlot = 2;
    const auto slotSpecs = workflowIoController.visualSpecsForWorkflow(ioWorkflow);
    if (const auto check = expect(slotSpecs.value("target").inputs.first().dimension == 1
            && slotSpecs.value("target").inputs.first().itemLabels == QStringList({"only"}),
            "Slot-level edges should not expand downstream dimensions without vws input comments")) {
        return check;
    }

    const auto runtimeSpecs = workflowIoController.visualSpecsForWorkflow(ioWorkflow);
    if (const auto check = expect(runtimeSpecs.value("target").inputs.first().dimension == 1
            && runtimeSpecs.value("source").outputs.first().dimension == 3,
            "Runtime outputs should not reshape visual IO dimensions")) {
        return check;
    }

    ioWorkflow.nodes[1].ioSpec.inputs.first() = portSpec("input", 3, {"left", "middle", "right"});
    const auto commentedSpecs = workflowIoController.visualSpecsForWorkflow(ioWorkflow);
    if (const auto check = expect(commentedSpecs.value("target").inputs.first().dimension == 3
            && commentedSpecs.value("target").inputs.first().itemLabels == QStringList({"left", "middle", "right"}),
            "Only saved/comment-derived IO specs should define multi-slot inputs")) {
        return check;
    }

    workflowController.createWorkflow("Canvas Session Root");
    vws::application::SubsystemService subsystemService;
    vws::presentation::CanvasNavigationController canvasNavigationController(subsystemService);
    vws::presentation::CanvasSessionController canvasSessionController(
        store,
        workflowController,
        canvasNavigationController);

    vws::domain::NodePosition subsystemPosition;
    auto subsystemNode = subsystemService.createSubsystemNode(
        store.currentWorkspace().id,
        "Session Subsystem",
        subsystemPosition);
    vws::domain::Workflow childWorkflow;
    childWorkflow.workflowId = "session-child";
    childWorkflow.workspaceId = store.currentWorkspace().id;
    childWorkflow.name = "Session Child";
    vws::domain::Node childNode;
    childNode.nodeId = "session-child-node";
    childNode.type = "starter";
    childNode.outputPorts = {"output"};
    childWorkflow.nodes = {childNode};
    if (const auto check = expect(subsystemService.saveSubsystemWorkflow(
            subsystemNode,
            childWorkflow,
            &errorMessage),
            QString("SubsystemService should embed child workflow for canvas session test: %1").arg(errorMessage))) {
        return check;
    }

    auto rootCanvasWorkflow = store.currentWorkflowSnapshot();
    rootCanvasWorkflow.nodes = {subsystemNode};
    canvasSessionController.startRootSession();
    rootCanvasWorkflow.name = "Canvas Session Root Edited";
    canvasSessionController.syncCurrentView(rootCanvasWorkflow);
    if (const auto check = expect(store.currentWorkflow().name == "Canvas Session Root Edited"
            && store.workflowDocument().isDirty(),
            "CanvasSessionController should sync root canvas edits through WorkflowController")) {
        return check;
    }

    if (const auto check = expect(canvasSessionController.enterSubsystem(
            rootCanvasWorkflow,
            {},
            subsystemNode.nodeId,
            &errorMessage),
            QString("CanvasSessionController should enter subsystem: %1").arg(errorMessage))) {
        return check;
    }
    auto editedChildCanvasWorkflow = canvasNavigationController.currentWorkflow();
    editedChildCanvasWorkflow.name = "Session Child Edited";
    vws::domain::Workflow flushedRootWorkflow;
    if (const auto check = expect(canvasSessionController.prepareRootWorkflowFromView(
            editedChildCanvasWorkflow,
            {},
            flushedRootWorkflow,
            &errorMessage),
            QString("CanvasSessionController should flush subsystem edits to root workflow: %1").arg(errorMessage))) {
        return check;
    }
    vws::domain::Workflow reloadedChildWorkflow;
    if (const auto check = expect(subsystemService.loadSubsystemWorkflow(
            flushedRootWorkflow.nodes.first(),
            reloadedChildWorkflow,
            &errorMessage),
            QString("SubsystemService should reload flushed child workflow: %1").arg(errorMessage))) {
        return check;
    }
    if (const auto check = expect(reloadedChildWorkflow.name == "Session Child Edited",
            "CanvasSessionController should preserve subsystem canvas edits when preparing the root workflow")) {
        return check;
    }

    QTextStream(stdout) << "presentation state tests passed" << Qt::endl;
    return 0;
}
