#include "application/NodeTemplateService.h"
#include "application/WorkflowService.h"
#include "application/WorkspaceService.h"

#include <QCoreApplication>
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

vws::domain::Node sampleFunctionNode()
{
    vws::domain::Node node;
    node.nodeId = "node-read-csv";
    node.type = "function";
    node.name = "Read CSV";
    node.description = "Reusable CSV reader";
    node.inputPorts = {"input"};
    node.outputPorts = {"output"};
    node.config = {
        {"language", "python"},
        {"entry", "run"},
        {"code", "def run(inputs, context):\n    return {'outputs': inputs, 'artifacts': []}"},
    };
    return node;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Verify workspace directories, workflow persistence, template save/list, and template import.
    vws::application::WorkspaceService workspaceService;
    vws::application::WorkflowService workflowService;
    vws::application::NodeTemplateService templateService;

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return fail("Could not create temporary directory");
    }

    const auto workspaceAPath = QDir(tempDir.path()).filePath("workspace_a");
    const auto workspaceBPath = QDir(tempDir.path()).filePath("workspace_b");

    QString errorMessage;
    vws::domain::Workspace workspaceA;
    if (!workspaceService.createWorkspace(workspaceAPath, "Workspace A", workspaceA, &errorMessage)) {
        return fail(QString("Could not create workspace A: %1").arg(errorMessage));
    }

    if (const auto check = expect(QFileInfo::exists(QDir(workspaceAPath).filePath("workspace.json")),
            "workspace.json should be created")) {
        return check;
    }
    if (const auto check = expect(QFileInfo::exists(QDir(workspaceAPath).filePath("workflows")),
            "workflows directory should be created")) {
        return check;
    }
    if (const auto check = expect(QFileInfo::exists(QDir(workspaceAPath).filePath("node_templates")),
            "node_templates directory should be created")) {
        return check;
    }
    if (!workspaceService.updatePythonExecutable(workspaceA, "C:/Python/test/python.exe", &errorMessage)) {
        return fail(QString("Could not update workspace Python executable: %1").arg(errorMessage));
    }
    vws::domain::Workspace reopenedWorkspaceA;
    if (!workspaceService.openWorkspace(workspaceAPath, reopenedWorkspaceA, &errorMessage)) {
        return fail(QString("Could not reopen workspace A: %1").arg(errorMessage));
    }
    if (const auto check = expect(workspaceService.pythonExecutable(reopenedWorkspaceA) == "C:/Python/test/python.exe",
            "Workspace Python executable should persist in workspace.json")) {
        return check;
    }

    auto workflow = workflowService.createEmptyWorkflow(workspaceA.id, "Demo Workflow");
    workflow.nodes.append(sampleFunctionNode());
    if (!workflowService.saveWorkflowToWorkspace(workspaceA.rootPath, workflow, &errorMessage)) {
        return fail(QString("Could not save workflow: %1").arg(errorMessage));
    }

    vws::domain::Workflow loadedWorkflow;
    if (!workflowService.loadWorkflowFromWorkspace(workspaceA.rootPath, workflow.workflowId, loadedWorkflow, &errorMessage)) {
        return fail(QString("Could not load workflow: %1").arg(errorMessage));
    }
    if (const auto check = expect(loadedWorkflow.workflowId == workflow.workflowId,
            "Loaded workflow id should match saved workflow id")) {
        return check;
    }
    if (const auto check = expect(workflowService.listWorkflows(workspaceA.rootPath, &errorMessage).size() == 1,
            "Workspace A should list one workflow")) {
        return check;
    }

    const auto nodeTemplate = templateService.createTemplateFromNode(workspaceA.id, workflow.nodes.first(), "CSV Reader Template");
    if (!templateService.saveTemplate(workspaceA.rootPath, nodeTemplate, &errorMessage)) {
        return fail(QString("Could not save template: %1").arg(errorMessage));
    }
    const auto templatesA = templateService.listTemplates(workspaceA.rootPath, &errorMessage);
    if (const auto check = expect(templatesA.size() == 1, "Workspace A should list one template")) {
        return check;
    }
    if (const auto check = expect(templatesA.first().name == "CSV Reader Template",
            "Template name should be preserved")) {
        return check;
    }

    auto reusedWorkflow = workflowService.createEmptyWorkflow(workspaceA.id, "Template Reuse Workflow");
    reusedWorkflow.nodes.append(templateService.createNodeFromTemplate(templatesA.first(), "Reused CSV Reader"));
    if (!workflowService.saveWorkflowToWorkspace(workspaceA.rootPath, reusedWorkflow, &errorMessage)) {
        return fail(QString("Could not save reused workflow: %1").arg(errorMessage));
    }
    if (const auto check = expect(reusedWorkflow.nodes.first().nodeId != workflow.nodes.first().nodeId,
            "Reused node should be a new node instance")) {
        return check;
    }
    if (const auto check = expect(reusedWorkflow.nodes.first().type == workflow.nodes.first().type,
            "Reused node should preserve the template node type")) {
        return check;
    }
    if (const auto check = expect(reusedWorkflow.nodes.first().templateId == templatesA.first().templateId,
            "Reused node should keep a reference to the template id")) {
        return check;
    }
    if (const auto check = expect(workflowService.listWorkflows(workspaceA.rootPath, &errorMessage).size() == 2,
            "Workspace A should list both original and template reuse workflows")) {
        return check;
    }

    vws::domain::Workspace workspaceB;
    if (!workspaceService.createWorkspace(workspaceBPath, "Workspace B", workspaceB, &errorMessage)) {
        return fail(QString("Could not create workspace B: %1").arg(errorMessage));
    }

    vws::domain::NodeTemplate invalidTemplate;
    errorMessage.clear();
    if (const auto check = expect(!templateService.importTemplateFile(
            workflowService.workflowPath(workspaceA.rootPath, workflow),
            workspaceB.rootPath,
            invalidTemplate,
            &errorMessage),
            "Importing a workflow JSON as a node template should fail")) {
        return check;
    }
    if (const auto check = expect(errorMessage.contains("Workflow"),
            "Importing a workflow JSON should explain that the file is not a node template")) {
        return check;
    }

    const auto invalidTemplateFilePath = QDir(workspaceA.rootPath).filePath("node_templates/not_a_template.json");
    if (!workflowService.saveWorkflow(invalidTemplateFilePath, workflow, &errorMessage)) {
        return fail(QString("Could not write invalid template fixture: %1").arg(errorMessage));
    }

    errorMessage.clear();
    const auto templatesAfterInvalidFile = templateService.listTemplates(workspaceA.rootPath, &errorMessage);
    if (const auto check = expect(templatesAfterInvalidFile.size() == 1,
            "Listing templates should skip invalid JSON files and keep valid templates visible")) {
        return check;
    }
    if (const auto check = expect(templatesAfterInvalidFile.first().templateId == nodeTemplate.templateId,
            "Listing templates should preserve the valid template when another JSON file is invalid")) {
        return check;
    }
    if (const auto check = expect(errorMessage.contains("not_a_template.json"),
            "Listing templates should report skipped invalid template files")) {
        return check;
    }

    vws::domain::NodeTemplate importedTemplate;
    if (!templateService.importTemplateFile(
            templateService.templatePath(workspaceA.rootPath, nodeTemplate),
            workspaceB.rootPath,
            importedTemplate,
            &errorMessage)) {
        return fail(QString("Could not import template: %1").arg(errorMessage));
    }

    const auto templatesB = templateService.listTemplates(workspaceB.rootPath, &errorMessage);
    if (const auto check = expect(templatesB.size() == 1, "Workspace B should list one imported template")) {
        return check;
    }
    if (const auto check = expect(templatesB.first().templateId != nodeTemplate.templateId,
            "Imported template should receive a new template id")) {
        return check;
    }
    if (const auto check = expect(templatesB.first().workspaceId == workspaceB.id,
            "Imported template should belong to target workspace")) {
        return check;
    }

    QTextStream(stdout) << "workspace template tests passed" << Qt::endl;
    return 0;
}
