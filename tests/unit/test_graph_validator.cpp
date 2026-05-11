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

    QTextStream(stdout) << "graph validator tests passed" << Qt::endl;
    return 0;
}
