#include "application/WorkflowService.h"
#include "execution/GraphValidator.h"

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

vws::domain::Workflow loadWorkflow(vws::application::WorkflowService& service, const QString& filePath)
{
    QString errorMessage;
    vws::domain::Workflow workflow;
    if (!service.loadWorkflow(filePath, workflow, &errorMessage)) {
        fail(QString("Failed to load fixture %1: %2").arg(filePath, errorMessage));
    }
    return workflow;
}

void setIoDimension(vws::domain::Node& node, int inputDimension, int outputDimension)
{
    node.ioSpec.inputs.clear();
    node.ioSpec.outputs.clear();
    if (!node.inputPorts.isEmpty()) {
        vws::domain::PortDimensionSpec input;
        input.portName = "input";
        input.dimension = inputDimension;
        node.ioSpec.inputs.append(input);
    }
    if (!node.outputPorts.isEmpty()) {
        vws::domain::PortDimensionSpec output;
        output.portName = "output";
        output.dimension = outputDimension;
        node.ioSpec.outputs.append(output);
    }
}

vws::domain::Node makeNode(const QString& id, const QString& type, QStringList inputs, QStringList outputs)
{
    vws::domain::Node node;
    node.nodeId = id;
    node.name = id;
    node.type = type;
    node.inputPorts = std::move(inputs);
    node.outputPorts = std::move(outputs);
    return node;
}

vws::domain::Edge makeEdge(
    const QString& id,
    const QString& from,
    const QString& fromPort,
    const QString& to,
    const QString& toPort)
{
    vws::domain::Edge edge;
    edge.edgeId = id;
    edge.fromNode = from;
    edge.fromPort = fromPort;
    edge.toNode = to;
    edge.toPort = toPort;
    return edge;
}

vws::domain::Workflow makeValidLoopWorkflow()
{
    vws::domain::Workflow workflow;
    workflow.workflowId = "loop-workflow";
    workflow.workspaceId = "workspace";
    auto starter = makeNode("starter", "starter", {}, {"output"});
    auto loop = makeNode("loop", "loop", {"input"}, {"output"});
    loop.config.insert("loop_iterations", 3);
    auto body = makeNode("body", "function", {"input"}, {"output"});
    auto sink = makeNode("sink", "function", {"input"}, {"output"});
    workflow.nodes = {starter, loop, body, sink};
    workflow.edges = {
        makeEdge("edge-starter-loop", "starter", "output", "loop", "input"),
        makeEdge("edge-loop-body", "loop", "output", "body", "input"),
        makeEdge("edge-body-sink", "body", "output", "sink", "input"),
    };
    return workflow;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Validate graph errors for missing nodes, duplicate ids, invalid ports, cycles, and Starter rules.
    vws::application::WorkflowService service;
    vws::execution::GraphValidator validator;

    const auto simpleWorkflow = loadWorkflow(service, "tests/fixtures/simple_workflow.json");
    const auto simpleResult = validator.validate(simpleWorkflow);
    if (const auto result = expect(simpleResult.valid, "Simple workflow should be valid")) {
        return result;
    }

    auto missingNodeWorkflow = simpleWorkflow;
    missingNodeWorkflow.edges[0].toNode = "missing-node";
    const auto missingNodeResult = validator.validate(missingNodeWorkflow);
    if (const auto result = expect(!missingNodeResult.valid, "Missing node reference should be invalid")) {
        return result;
    }

    auto duplicateNodeWorkflow = simpleWorkflow;
    duplicateNodeWorkflow.nodes[1].nodeId = duplicateNodeWorkflow.nodes[0].nodeId;
    const auto duplicateNodeResult = validator.validate(duplicateNodeWorkflow);
    if (const auto result = expect(!duplicateNodeResult.valid, "Duplicate node id should be invalid")) {
        return result;
    }

    auto invalidPortWorkflow = simpleWorkflow;
    invalidPortWorkflow.edges[0].toPort = "missing-port";
    const auto invalidPortResult = validator.validate(invalidPortWorkflow);
    if (const auto result = expect(!invalidPortResult.valid, "Invalid input port should be invalid")) {
        return result;
    }

    auto invalidStarterWorkflow = simpleWorkflow;
    invalidStarterWorkflow.nodes[0].type = "starter";
    invalidStarterWorkflow.nodes[0].inputPorts = {"input"};
    const auto invalidStarterResult = validator.validate(invalidStarterWorkflow);
    if (const auto result = expect(!invalidStarterResult.valid,
            "Starter nodes should reject input ports")) {
        return result;
    }

    auto noStarterWorkflow = simpleWorkflow;
    noStarterWorkflow.nodes[0].type = "function";
    noStarterWorkflow.nodes[0].inputPorts = {"input"};
    const auto noStarterResult = validator.validate(noStarterWorkflow);
    if (const auto result = expect(!noStarterResult.valid, "Workflow without a Starter node should be invalid")) {
        return result;
    }
    const auto subsystemNoStarterResult = validator.validate(
        noStarterWorkflow,
        vws::execution::GraphValidationMode::SubsystemWorkflow);
    if (const auto result = expect(subsystemNoStarterResult.valid,
            "Subsystem workflow should allow implicit entry nodes without a Starter node")) {
        return result;
    }

    auto zeroInputSubsystemWorkflow = simpleWorkflow;
    zeroInputSubsystemWorkflow.nodes[0].type = "subsystem";
    zeroInputSubsystemWorkflow.nodes[0].inputPorts.clear();
    zeroInputSubsystemWorkflow.nodes[0].outputPorts = {"output"};
    const auto zeroInputSubsystemResult = validator.validate(zeroInputSubsystemWorkflow);
    if (const auto result = expect(zeroInputSubsystemResult.valid,
            "Top-level workflow should allow a zero-input Subsystem node as a graph entry")) {
        return result;
    }

    auto starterIncomingWorkflow = simpleWorkflow;
    vws::domain::Edge incomingStarterEdge;
    incomingStarterEdge.edgeId = "edge-b-a";
    incomingStarterEdge.fromNode = "node-b";
    incomingStarterEdge.fromPort = "output";
    incomingStarterEdge.toNode = "node-a";
    incomingStarterEdge.toPort = "input";
    starterIncomingWorkflow.nodes[0].inputPorts = {"input"};
    starterIncomingWorkflow.edges.append(incomingStarterEdge);
    const auto starterIncomingResult = validator.validate(starterIncomingWorkflow);
    if (const auto result = expect(!starterIncomingResult.valid,
            "Starter node with incoming edges should be invalid")) {
        return result;
    }

    auto isolatedNodeWorkflow = simpleWorkflow;
    vws::domain::Node isolatedNode;
    isolatedNode.nodeId = "isolated";
    isolatedNode.name = "isolated";
    isolatedNode.type = "function";
    isolatedNode.inputPorts = {"input"};
    isolatedNode.outputPorts = {"output"};
    isolatedNodeWorkflow.nodes.append(isolatedNode);
    const auto isolatedNodeResult = validator.validate(isolatedNodeWorkflow);
    if (const auto result = expect(!isolatedNodeResult.valid,
            "Isolated non-starter node should be invalid because every path must start from a Starter node")) {
        return result;
    }
    if (const auto result = expect(!isolatedNodeResult.errors.isEmpty(),
            "Isolated non-starter node should produce an error")) {
        return result;
    }

    auto cyclicWorkflow = simpleWorkflow;
    vws::domain::Edge cycleEdge;
    cycleEdge.edgeId = "edge-b-a";
    cycleEdge.fromNode = "node-b";
    cycleEdge.fromPort = "output";
    cycleEdge.toNode = "node-a";
    cycleEdge.toPort = "input";
    cyclicWorkflow.nodes[0].inputPorts.append("input");
    cyclicWorkflow.edges.append(cycleEdge);

    const auto cyclicResult = validator.validate(cyclicWorkflow);
    if (const auto result = expect(!cyclicResult.valid, "Cycle should be invalid")) {
        return result;
    }
    const auto subsystemCyclicResult = validator.validate(
        cyclicWorkflow,
        vws::execution::GraphValidationMode::SubsystemWorkflow);
    if (const auto result = expect(!subsystemCyclicResult.valid,
            "Subsystem workflow should still reject cycles")) {
        return result;
    }

    auto slotWorkflow = simpleWorkflow;
    setIoDimension(slotWorkflow.nodes[0], 1, 3);
    setIoDimension(slotWorkflow.nodes[1], 3, 1);
    slotWorkflow.edges[0].fromSlot = 1;
    slotWorkflow.edges[0].toSlot = 2;
    const auto slotResult = validator.validate(slotWorkflow);
    if (const auto result = expect(slotResult.valid, "Valid slot-level edge should pass graph validation")) {
        return result;
    }

    auto outOfRangeSlotWorkflow = slotWorkflow;
    outOfRangeSlotWorkflow.edges[0].fromSlot = 3;
    const auto outOfRangeSlotResult = validator.validate(outOfRangeSlotWorkflow);
    if (const auto result = expect(!outOfRangeSlotResult.valid, "fromSlot beyond output dimension should be invalid")) {
        return result;
    }

    auto outOfRangeTargetSlotWorkflow = slotWorkflow;
    outOfRangeTargetSlotWorkflow.edges[0].toSlot = 3;
    const auto outOfRangeTargetSlotResult = validator.validate(outOfRangeTargetSlotWorkflow);
    if (const auto result = expect(!outOfRangeTargetSlotResult.valid, "toSlot beyond input dimension should be invalid")) {
        return result;
    }

    auto duplicateSlotWorkflow = slotWorkflow;
    auto duplicateEdge = duplicateSlotWorkflow.edges[0];
    duplicateEdge.edgeId = "edge-duplicate-slot";
    duplicateSlotWorkflow.edges.append(duplicateEdge);
    const auto duplicateSlotResult = validator.validate(duplicateSlotWorkflow);
    if (const auto result = expect(!duplicateSlotResult.valid, "Multiple edges writing one input slot should be invalid")) {
        return result;
    }

    auto negativeSlotWorkflow = slotWorkflow;
    negativeSlotWorkflow.edges[0].fromSlot = -1;
    const auto negativeSlotResult = validator.validate(negativeSlotWorkflow);
    if (const auto result = expect(!negativeSlotResult.valid, "Negative source edge slots should be invalid")) {
        return result;
    }

    auto negativeTargetSlotWorkflow = slotWorkflow;
    negativeTargetSlotWorkflow.edges[0].toSlot = -1;
    const auto negativeTargetSlotResult = validator.validate(negativeTargetSlotWorkflow);
    if (const auto result = expect(!negativeTargetSlotResult.valid, "Negative target edge slots should be invalid")) {
        return result;
    }

    const auto subsystemNegativeSlotResult = validator.validate(
        negativeTargetSlotWorkflow,
        vws::execution::GraphValidationMode::SubsystemWorkflow);
    if (const auto result = expect(!subsystemNegativeSlotResult.valid,
            "Subsystem workflow validation should still reject negative edge slots")) {
        return result;
    }

    const auto validLoopResult = validator.validate(makeValidLoopWorkflow());
    if (const auto result = expect(validLoopResult.valid,
            QString("Valid single Loop workflow should pass: %1").arg(validLoopResult.errors.join("; ")))) {
        return result;
    }

    auto noBodyLoop = makeValidLoopWorkflow();
    noBodyLoop.edges.removeAt(1);
    const auto noBodyLoopResult = validator.validate(noBodyLoop);
    if (const auto result = expect(!noBodyLoopResult.valid, "Loop without a direct body node should be invalid")) {
        return result;
    }

    auto missingMaxLoop = makeValidLoopWorkflow();
    missingMaxLoop.nodes[1].config.remove("loop_iterations");
    const auto missingMaxLoopResult = validator.validate(missingMaxLoop);
    if (const auto result = expect(!missingMaxLoopResult.valid, "Loop without loop_iterations should be invalid")) {
        return result;
    }

    auto multiBodyLoop = makeValidLoopWorkflow();
    multiBodyLoop.nodes.append(makeNode("body-2", "function", {"input"}, {"output"}));
    multiBodyLoop.edges.append(makeEdge("edge-loop-body-2", "loop", "output", "body-2", "input"));
    const auto multiBodyLoopResult = validator.validate(multiBodyLoop);
    if (const auto result = expect(!multiBodyLoopResult.valid, "Loop with multiple direct body nodes should be invalid")) {
        return result;
    }

    auto externalInputLoop = makeValidLoopWorkflow();
    externalInputLoop.nodes.append(makeNode("outside", "starter", {}, {"output"}));
    externalInputLoop.edges.append(makeEdge("edge-outside-body", "outside", "output", "body", "input"));
    externalInputLoop.edges.last().toSlot = 1;
    setIoDimension(externalInputLoop.nodes[2], 2, 1);
    const auto externalInputLoopResult = validator.validate(externalInputLoop);
    if (const auto result = expect(!externalInputLoopResult.valid, "Loop body receiving external input should be invalid")) {
        return result;
    }

    auto nestedLoop = makeValidLoopWorkflow();
    nestedLoop.nodes[2].type = "loop";
    nestedLoop.nodes[2].config.insert("loop_iterations", 2);
    const auto nestedLoopResult = validator.validate(nestedLoop);
    if (const auto result = expect(!nestedLoopResult.valid, "Loop body cannot be another Loop node")) {
        return result;
    }

    QTextStream(stdout) << "graph validator tests passed" << Qt::endl;
    return 0;
}
