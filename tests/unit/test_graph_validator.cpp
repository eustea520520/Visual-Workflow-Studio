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

    auto duplicateSlotWorkflow = slotWorkflow;
    auto duplicateEdge = duplicateSlotWorkflow.edges[0];
    duplicateEdge.edgeId = "edge-duplicate-slot";
    duplicateSlotWorkflow.edges.append(duplicateEdge);
    const auto duplicateSlotResult = validator.validate(duplicateSlotWorkflow);
    if (const auto result = expect(!duplicateSlotResult.valid, "Multiple edges writing one input slot should be invalid")) {
        return result;
    }

    auto mixedPortWorkflow = slotWorkflow;
    auto wholePortEdge = mixedPortWorkflow.edges[0];
    wholePortEdge.edgeId = "edge-whole-port";
    wholePortEdge.fromSlot = -1;
    wholePortEdge.toSlot = -1;
    mixedPortWorkflow.edges.append(wholePortEdge);
    const auto mixedPortResult = validator.validate(mixedPortWorkflow);
    if (const auto result = expect(!mixedPortResult.valid, "Whole-port and slot-level edges on one input port should not mix")) {
        return result;
    }

    QTextStream(stdout) << "graph validator tests passed" << Qt::endl;
    return 0;
}
