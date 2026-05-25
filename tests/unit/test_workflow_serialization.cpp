#include "application/WorkflowEditService.h"
#include "application/WorkflowService.h"
#include "domain/WorkflowJsonParser.h"
#include "domain/WorkflowSchema.h"
#include "infrastructure/JsonUtils.h"

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

    vws::domain::Edge defaultEdge;
    if (const auto result = expect(defaultEdge.fromSlot == 0 && defaultEdge.toSlot == 0,
            "New Edge objects should default both slots to 0")) {
        return result;
    }

    defaultEdge.edgeId = "edge-default";
    defaultEdge.fromNode = "a";
    defaultEdge.fromPort = "output";
    defaultEdge.toNode = "b";
    defaultEdge.toPort = "input";
    const auto defaultEdgeJson = defaultEdge.toJson();
    if (const auto result = expect(defaultEdgeJson.contains("from_slot")
            && defaultEdgeJson.contains("to_slot")
            && defaultEdgeJson.value("from_slot").toInt(-1) == 0
            && defaultEdgeJson.value("to_slot").toInt(-1) == 0,
            "Edge JSON should always write explicit from_slot/to_slot fields")) {
        return result;
    }

    auto oldMissingSlotWorkflowJson = QJsonObject{
        {"schema_version", vws::domain::CurrentWorkflowSchemaVersion},
        {"workflow_id", "legacy-missing-slots"},
        {"nodes", QJsonArray{}},
        {"edges", QJsonArray{QJsonObject{
            {"edge_id", "legacy-missing-slots"},
            {"from_node", "a"},
            {"from_port", "output"},
            {"to_node", "b"},
            {"to_port", "input"},
        }}},
    };
    const auto oldMissingSlotParse = vws::domain::WorkflowJsonParser::parseStrict(oldMissingSlotWorkflowJson);
    if (const auto result = expect(!oldMissingSlotParse.success
            && oldMissingSlotParse.errors.join("\n") == vws::domain::WorkflowJsonParser::unreadableWorkspaceMessage(),
            "Workflow JSON without edge slot fields should be rejected")) {
        return result;
    }

    auto oldNegativeSlotWorkflowJson = oldMissingSlotWorkflowJson;
    oldNegativeSlotWorkflowJson["workflow_id"] = "legacy-negative-slots";
    oldNegativeSlotWorkflowJson["edges"] = QJsonArray{QJsonObject{
        {"edge_id", "legacy-negative-slots"},
        {"from_node", "a"},
        {"from_port", "output"},
        {"from_slot", -1},
        {"to_node", "b"},
        {"to_port", "input"},
        {"to_slot", -1},
    }};
    const auto oldNegativeSlotParse = vws::domain::WorkflowJsonParser::parseStrict(oldNegativeSlotWorkflowJson);
    if (const auto result = expect(!oldNegativeSlotParse.success
            && oldNegativeSlotParse.errors.join("\n") == vws::domain::WorkflowJsonParser::unreadableWorkspaceMessage(),
            "Workflow JSON with negative edge slots should be rejected")) {
        return result;
    }

    auto missingSchemaWorkflowJson = oldMissingSlotWorkflowJson;
    missingSchemaWorkflowJson.remove("schema_version");
    const auto missingSchemaParse = vws::domain::WorkflowJsonParser::parseStrict(missingSchemaWorkflowJson);
    if (const auto result = expect(!missingSchemaParse.success
            && missingSchemaParse.errors.join("\n") == vws::domain::WorkflowJsonParser::unreadableWorkspaceMessage(),
            "Workflow JSON without schema_version should be rejected")) {
        return result;
    }

    auto oldSchemaWorkflowJson = oldMissingSlotWorkflowJson;
    oldSchemaWorkflowJson["schema_version"] = 1;
    const auto oldSchemaParse = vws::domain::WorkflowJsonParser::parseStrict(oldSchemaWorkflowJson);
    if (const auto result = expect(!oldSchemaParse.success
            && oldSchemaParse.errors.join("\n") == vws::domain::WorkflowJsonParser::unreadableWorkspaceMessage(),
            "Workflow JSON with schema_version 1 should be rejected")) {
        return result;
    }

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
    if (const auto result = expect(simpleWorkflow.edges.first().fromSlot == 0 && simpleWorkflow.edges.first().toSlot == 0,
            "Workflow edges should use slot 0 by default")) {
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

    const auto oldSchemaPath = QDir(tempDir.path()).filePath("old_schema_workflow.json");
    if (!vws::infrastructure::JsonUtils::writeObjectToFile(oldSchemaPath, oldSchemaWorkflowJson, &errorMessage)) {
        return fail(QString("Failed to write old schema workflow fixture: %1").arg(errorMessage));
    }
    vws::domain::Workflow rejectedWorkflow;
    if (const auto result = expect(!service.loadWorkflow(oldSchemaPath, rejectedWorkflow, &errorMessage)
            && errorMessage == vws::domain::WorkflowJsonParser::unreadableWorkspaceMessage(),
            "WorkflowService should reject old schema workflow files with a clear error")) {
        return result;
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
    if (const auto result = expect(!oldJsonNode.toJson().contains("size"),
            "Nodes without an explicit canvas size should not serialize a default size")) {
        return result;
    }

    auto resizedNode = oldJsonNode;
    resizedNode.size.width = 260.0;
    resizedNode.size.height = 132.0;
    const auto resizedNodeJson = resizedNode.toJson();
    const auto resizedNodeRoundTrip = vws::domain::Node::fromJson(resizedNodeJson);
    if (const auto result = expect(resizedNodeJson.value("size").toObject().value("width").toDouble() == 260.0
            && resizedNodeRoundTrip.size.width == 260.0
            && resizedNodeRoundTrip.size.height == 132.0,
            "Explicit node canvas size should survive JSON roundtrip")) {
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
