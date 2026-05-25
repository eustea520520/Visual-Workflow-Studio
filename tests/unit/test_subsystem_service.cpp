#include "application/subsystem/SubsystemService.h"
#include "application/subsystem/SubsystemTypes.h"
#include "domain/NodeConfigKeys.h"
#include "domain/NodeTypes.h"
#include "domain/WorkflowJsonParser.h"
#include "domain/WorkflowSchema.h"

#include <QCoreApplication>
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

vws::domain::Node makeFunctionNode(const QString& id, const QString& name)
{
    vws::domain::Node node;
    node.nodeId = id;
    node.name = name;
    node.type = vws::domain::NodeTypes::Function;
    node.inputPorts = {"input"};
    node.outputPorts = {"output"};
    vws::domain::PortDimensionSpec input;
    input.portName = "input";
    input.dimension = 3;
    input.itemLabels = {"a", "b", "c"};
    node.ioSpec.inputs.append(input);
    vws::domain::PortDimensionSpec output;
    output.portName = "output";
    output.dimension = 2;
    output.itemLabels = {"x", "y"};
    node.ioSpec.outputs.append(output);
    return node;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    vws::application::SubsystemService service;
    vws::domain::NodePosition position;
    position.x = 10;
    position.y = 20;
    auto subsystem = service.createSubsystemNode("workspace-id", "Subsystem A", position);

    if (const auto result = expect(subsystem.type == vws::domain::NodeTypes::Subsystem,
            "createSubsystemNode should create subsystem type")) {
        return result;
    }
    if (const auto result = expect(subsystem.inputPorts.isEmpty() && subsystem.outputPorts.isEmpty(),
            "New empty subsystem should have no external ports")) {
        return result;
    }
    if (const auto result = expect(subsystem.config.contains(vws::domain::NodeConfigKeys::SubsystemWorkflow),
            "New subsystem should contain embedded workflow JSON")) {
        return result;
    }
    if (const auto result = expect(
            subsystem.config.value(vws::domain::NodeConfigKeys::SubsystemWorkflow).toObject()
                    .value("schema_version").toInt()
                == vws::domain::CurrentWorkflowSchemaVersion,
            "New subsystem embedded workflow should use current workflow schema")) {
        return result;
    }

    vws::domain::Workflow child;
    QString errorMessage;
    if (!service.loadSubsystemWorkflow(subsystem, child, &errorMessage)) {
        return fail(errorMessage);
    }
    child.nodes.append(makeFunctionNode("inner", "Inner Node"));
    if (!service.saveSubsystemWorkflow(subsystem, child, &errorMessage)) {
        return fail(errorMessage);
    }

    if (const auto result = expect(subsystem.inputPorts == QStringList{"in_inner_input"},
            "Boundary inference should expose a stable entry input port key")) {
        return result;
    }
    if (const auto result = expect(subsystem.outputPorts == QStringList{"out_inner_output"},
            "Boundary inference should expose a stable exit output port key")) {
        return result;
    }
    if (const auto result = expect(subsystem.ioSpec.inputs.first().dimension == 3
            && subsystem.ioSpec.outputs.first().dimension == 2,
            "Boundary inference should inherit internal port dimensions")) {
        return result;
    }
    const auto boundary = vws::application::SubsystemBoundary::fromJson(
        subsystem.config.value(vws::domain::NodeConfigKeys::SubsystemBoundary).toObject());
    if (const auto result = expect(boundary.inputs.size() == 1
            && boundary.inputs.first().externalPort == "in_inner_input"
            && boundary.inputs.first().displayName == "Inner Node(input)"
            && boundary.outputs.size() == 1
            && boundary.outputs.first().externalPort == "out_inner_output"
            && boundary.outputs.first().displayName == "Inner Node(output)",
            "Boundary ports should separate stable external keys from display labels")) {
        return result;
    }

    child.nodes.first().name = "Renamed Inner";
    if (!service.saveSubsystemWorkflow(subsystem, child, &errorMessage)) {
        return fail(errorMessage);
    }
    const auto renamedBoundary = vws::application::SubsystemBoundary::fromJson(
        subsystem.config.value(vws::domain::NodeConfigKeys::SubsystemBoundary).toObject());
    if (const auto result = expect(subsystem.inputPorts == QStringList{"in_inner_input"}
            && subsystem.outputPorts == QStringList{"out_inner_output"}
            && renamedBoundary.inputs.first().displayName == "Renamed Inner(input)"
            && renamedBoundary.outputs.first().displayName == "Renamed Inner(output)",
            "Internal node rename should update display labels without changing external port keys")) {
        return result;
    }

    vws::domain::Workflow reloadedChild;
    if (!service.loadSubsystemWorkflow(subsystem, reloadedChild, &errorMessage)) {
        return fail(errorMessage);
    }
    if (const auto result = expect(reloadedChild.nodes.size() == 1 && reloadedChild.nodes.first().nodeId == "inner",
            "Embedded subsystem workflow should survive save/load through node config")) {
        return result;
    }

    auto oldSchemaSubsystem = service.createSubsystemNode("workspace-id", "Old Schema", position);
    auto oldSchemaWorkflowObject = oldSchemaSubsystem.config
        .value(vws::domain::NodeConfigKeys::SubsystemWorkflow)
        .toObject();
    oldSchemaWorkflowObject.insert("schema_version", 1);
    oldSchemaSubsystem.config.insert(vws::domain::NodeConfigKeys::SubsystemWorkflow, oldSchemaWorkflowObject);
    vws::domain::Workflow rejectedChild;
    if (const auto result = expect(!service.loadSubsystemWorkflow(oldSchemaSubsystem, rejectedChild, &errorMessage)
            && errorMessage == vws::domain::WorkflowJsonParser::unreadableWorkspaceMessage(),
            "SubsystemService should reject embedded workflows using old schema")) {
        return result;
    }

    auto portLevelSubsystem = service.createSubsystemNode("workspace-id", "Port Level", position);
    vws::domain::Workflow portLevelChild;
    auto source = makeFunctionNode("source", "Source");
    source.type = vws::domain::NodeTypes::Starter;
    source.inputPorts.clear();
    source.ioSpec.inputs.clear();
    auto target = makeFunctionNode("target", "Target");
    target.inputPorts = {"left", "right"};
    target.ioSpec.inputs.clear();
    vws::domain::PortDimensionSpec leftSpec;
    leftSpec.portName = "left";
    leftSpec.dimension = 1;
    leftSpec.itemLabels = {"left"};
    target.ioSpec.inputs.append(leftSpec);
    vws::domain::PortDimensionSpec rightSpec;
    rightSpec.portName = "right";
    rightSpec.dimension = 2;
    rightSpec.itemLabels = {"r1", "r2"};
    target.ioSpec.inputs.append(rightSpec);
    vws::domain::Edge internalEdge;
    internalEdge.edgeId = "source-target-left";
    internalEdge.fromNode = "source";
    internalEdge.fromPort = "output";
    internalEdge.toNode = "target";
    internalEdge.toPort = "left";
    portLevelChild.nodes = {source, target};
    portLevelChild.edges = {internalEdge};
    if (!service.saveSubsystemWorkflow(portLevelSubsystem, portLevelChild, &errorMessage)) {
        return fail(errorMessage);
    }
    if (const auto result = expect(portLevelSubsystem.inputPorts == QStringList{"in_target_right"}
            && portLevelSubsystem.ioSpec.inputs.first().dimension == 2,
            "Boundary inference should expose unconnected input ports at port level, not node level")) {
        return result;
    }

    QTextStream(stdout) << "subsystem service tests passed" << Qt::endl;
    return 0;
}
