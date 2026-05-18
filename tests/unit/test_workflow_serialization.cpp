#include "application/WorkflowEditService.h"
#include "application/WorkflowService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
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

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Verify fixture JSON -> Workflow object -> temporary JSON file -> Workflow object.
    vws::application::WorkflowService service;

    QString errorMessage;
    vws::domain::Workflow simpleWorkflow;
    if (!service.loadWorkflow("tests/fixtures/simple_workflow.json", simpleWorkflow, &errorMessage)) {
        return fail(QString("Failed to load simple workflow: %1").arg(errorMessage));
    }

    if (const auto result = expect(simpleWorkflow.nodes.size() == 2, "Simple workflow should contain 2 nodes")) {
        return result;
    }
    if (const auto result = expect(simpleWorkflow.edges.size() == 1, "Simple workflow should contain 1 edge")) {
        return result;
    }
    if (const auto result = expect(simpleWorkflow.edges.first().fromSlot == -1 && simpleWorkflow.edges.first().toSlot == -1,
            "Old workflow edges without slot fields should default to -1")) {
        return result;
    }

    vws::domain::Workflow branchingWorkflow;
    if (!service.loadWorkflow("tests/fixtures/branching_workflow.json", branchingWorkflow, &errorMessage)) {
        return fail(QString("Failed to load branching workflow: %1").arg(errorMessage));
    }

    if (const auto result = expect(branchingWorkflow.nodes.size() == 4, "Branching workflow should contain 4 nodes")) {
        return result;
    }
    if (const auto result = expect(branchingWorkflow.edges.size() == 4, "Branching workflow should contain 4 edges")) {
        return result;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return fail("Could not create temporary directory");
    }

    const auto roundTripPath = QDir(tempDir.path()).filePath("roundtrip_workflow.json");
    if (!service.saveWorkflow(roundTripPath, branchingWorkflow, &errorMessage)) {
        return fail(QString("Failed to save workflow: %1").arg(errorMessage));
    }

    vws::domain::Workflow roundTripWorkflow;
    if (!service.loadWorkflow(roundTripPath, roundTripWorkflow, &errorMessage)) {
        return fail(QString("Failed to load roundtrip workflow: %1").arg(errorMessage));
    }

    if (const auto result = expect(roundTripWorkflow.workflowId == branchingWorkflow.workflowId,
            "Roundtrip workflow id should be preserved")) {
        return result;
    }
    if (const auto result = expect(roundTripWorkflow.nodes.size() == branchingWorkflow.nodes.size(),
            "Roundtrip node count should be preserved")) {
        return result;
    }
    if (const auto result = expect(roundTripWorkflow.edges.size() == branchingWorkflow.edges.size(),
            "Roundtrip edge count should be preserved")) {
        return result;
    }

    auto slotWorkflow = branchingWorkflow;
    slotWorkflow.edges[0].fromSlot = 1;
    slotWorkflow.edges[0].toSlot = 2;
    const auto slotRoundTripPath = QDir(tempDir.path()).filePath("slot_roundtrip_workflow.json");
    if (!service.saveWorkflow(slotRoundTripPath, slotWorkflow, &errorMessage)) {
        return fail(QString("Failed to save slot workflow: %1").arg(errorMessage));
    }
    vws::domain::Workflow slotRoundTripWorkflow;
    if (!service.loadWorkflow(slotRoundTripPath, slotRoundTripWorkflow, &errorMessage)) {
        return fail(QString("Failed to load slot workflow: %1").arg(errorMessage));
    }
    if (const auto result = expect(slotRoundTripWorkflow.edges.first().fromSlot == 1
            && slotRoundTripWorkflow.edges.first().toSlot == 2,
            "Workflow edge slots should survive JSON roundtrip")) {
        return result;
    }

    const auto oldJsonNode = vws::domain::Node::fromJson({
        {"node_id", "old_node"},
        {"type", "function"},
        {"name", "Old Node"},
        {"position", QJsonObject{{"x", 0}, {"y", 0}}},
        {"input_ports", QJsonArray{"input"}},
        {"output_ports", QJsonArray{"output"}},
    });
    if (const auto result = expect(oldJsonNode.rotationDegrees == 0,
            "Old workflow nodes without rotation_degrees should default to 0")) {
        return result;
    }

    auto rotatedNode = oldJsonNode;
    rotatedNode.rotationDegrees = 450;
    if (const auto result = expect(rotatedNode.toJson().value("rotation_degrees").toInt() == 90,
            "Serialized node rotation should be normalized")) {
        return result;
    }

    vws::domain::Workflow rotationWorkflow;
    rotationWorkflow.nodes.append(oldJsonNode);
    if (const auto result = expect(vws::application::WorkflowEditService::rotateNode(rotationWorkflow, "old_node", -90),
            "WorkflowEditService should rotate an existing node")) {
        return result;
    }
    if (const auto result = expect(rotationWorkflow.nodes.first().rotationDegrees == 270,
            "Negative rotation should normalize to 270 degrees")) {
        return result;
    }

    if (!service.saveWorkflowToWorkspace(tempDir.path(), branchingWorkflow, &errorMessage)) {
        return fail(QString("Failed to save workflow to workspace: %1").arg(errorMessage));
    }
    const auto workspaceWorkflowPath = service.workflowPath(tempDir.path(), branchingWorkflow);
    if (const auto result = expect(QFileInfo::exists(workspaceWorkflowPath),
            "saveWorkflowToWorkspace should create a workflow file")) {
        return result;
    }
    if (!service.deleteWorkflowFromWorkspace(tempDir.path(), branchingWorkflow.workflowId, &errorMessage)) {
        return fail(QString("Failed to delete workflow from workspace: %1").arg(errorMessage));
    }
    if (const auto result = expect(!QFileInfo::exists(workspaceWorkflowPath),
            "deleteWorkflowFromWorkspace should remove the workflow file")) {
        return result;
    }

    QTextStream(stdout) << "workflow serialization tests passed" << Qt::endl;
    return 0;
}
