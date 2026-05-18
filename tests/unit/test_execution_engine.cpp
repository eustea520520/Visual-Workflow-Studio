#include "application/WorkflowService.h"
#include "execution/ExecutionEngine.h"
#include "workers/MockNodeWorker.h"
#include "workers/WorkerRegistry.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QTextStream>

#include <chrono>
#include <memory>
#include <thread>

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

vws::domain::Node makeNode(const QString& id, const QString& type = "function")
{
    vws::domain::Node node;
    node.nodeId = id;
    node.type = type;
    node.name = id;
    node.inputPorts = type == "starter" ? QStringList{} : QStringList{"input"};
    node.outputPorts = {"output"};
    node.config = {{"mock", true}};
    return node;
}

vws::domain::Edge makeEdge(const QString& id, const QString& from, const QString& to)
{
    vws::domain::Edge edge;
    edge.edgeId = id;
    edge.fromNode = from;
    edge.fromPort = "output";
    edge.toNode = to;
    edge.toPort = "input";
    return edge;
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

struct EngineFixture {
    vws::application::WorkflowService workflowService;
    vws::workers::WorkerRegistry registry;
    vws::execution::ExecutionEngine engine;

    EngineFixture()
        : engine(registry)
    {
        registry.registerWorker(std::make_shared<vws::workers::MockNodeWorker>("function"));
    }
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Verify simple chains, joins, failures, event delivery, and cancellation.
    EngineFixture fixture;
    auto aliasWorker = std::make_shared<vws::workers::MockNodeWorker>("function");
    fixture.registry.registerWorkerForType("starter", aliasWorker);
    fixture.registry.registerWorkerForType("agent", aliasWorker);
    if (const auto result = expect(fixture.registry.hasWorkerType("starter"),
            "WorkerRegistry should allow starter nodes to reuse a Python-style worker")) {
        return result;
    }
    if (const auto result = expect(fixture.registry.hasWorkerType("agent"),
            "WorkerRegistry should allow agent nodes to reuse a Python-style worker")) {
        return result;
    }

    QStringList nodeStatusEvents;
    QStringList workflowStatusEvents;
    QStringList threadTraceEvents;

    QObject::connect(&fixture.engine.eventBus(),
        &vws::execution::ExecutionEventBus::nodeStatusChanged,
        [&](const QString&, const QString& nodeId, const QString& status) {
            nodeStatusEvents.append(QString("%1:%2").arg(nodeId, status));
        });

    QObject::connect(&fixture.engine.eventBus(),
        &vws::execution::ExecutionEventBus::workflowStatusChanged,
        [&](const QString&, const QString& status) {
            workflowStatusEvents.append(status);
        });
    QObject::connect(&fixture.engine.eventBus(),
        &vws::execution::ExecutionEventBus::threadTrace,
        [&](const QString&, const QString& nodeId, const QString& phase, const QString& threadId, const QString&) {
            threadTraceEvents.append(QString("%1:%2:%3").arg(nodeId, phase, threadId));
        });

    const auto simpleWorkflow = loadWorkflow(fixture.workflowService, "tests/fixtures/simple_workflow.json");
    const auto simpleRun = fixture.engine.runWorkflow(simpleWorkflow);

    if (const auto result = expect(simpleRun.success, "Simple workflow execution should succeed")) {
        return result;
    }
    if (const auto result = expect(simpleRun.status == "Succeeded", "Simple workflow status should be Succeeded")) {
        return result;
    }
    if (const auto result = expect(simpleRun.nodeStatuses.value("node-a") == "Succeeded", "node-a should succeed")) {
        return result;
    }
    if (const auto result = expect(simpleRun.nodeStatuses.value("node-b") == "Succeeded", "node-b should succeed")) {
        return result;
    }
    if (const auto result = expect(simpleRun.nodeResults.value("node-b").outputs.value("inputs").toObject().contains("input"),
            "node-b should receive input from node-a")) {
        return result;
    }
    if (const auto result = expect(nodeStatusEvents.contains("node-a:Running"), "node-a Running event should be emitted")) {
        return result;
    }
    if (const auto result = expect(workflowStatusEvents.contains("Succeeded"), "Succeeded workflow event should be emitted")) {
        return result;
    }
    if (const auto result = expect(!threadTraceEvents.isEmpty(), "ExecutionEngine should emit thread trace events")) {
        return result;
    }

    const auto branchingWorkflow = loadWorkflow(fixture.workflowService, "tests/fixtures/branching_workflow.json");
    const auto branchingRun = fixture.engine.runWorkflow(branchingWorkflow);
    if (const auto result = expect(branchingRun.success, "Branching workflow execution should succeed")) {
        return result;
    }
    if (const auto result = expect(branchingRun.nodeStatuses.value("node-b") == "Succeeded", "node-b should succeed")) {
        return result;
    }
    if (const auto result = expect(branchingRun.nodeStatuses.value("node-c") == "Succeeded", "node-c should succeed")) {
        return result;
    }
    if (const auto result = expect(branchingRun.nodeStatuses.value("node-d") == "Succeeded", "node-d should succeed")) {
        return result;
    }

    auto failingWorkflow = simpleWorkflow;
    failingWorkflow.nodes[1].config.insert("mock_fail", true);
    const auto failingRun = fixture.engine.runWorkflow(failingWorkflow);
    if (const auto result = expect(!failingRun.success, "Failing workflow execution should fail")) {
        return result;
    }
    if (const auto result = expect(failingRun.status == "PartiallySucceeded", "Failing workflow with prior success should be PartiallySucceeded")) {
        return result;
    }
    if (const auto result = expect(failingRun.nodeStatuses.value("node-b") == "Failed", "node-b should fail")) {
        return result;
    }

    vws::domain::Workflow joinWorkflow;
    joinWorkflow.workflowId = "join-workflow";
    joinWorkflow.workspaceId = "workspace";
    joinWorkflow.nodes = {
        makeNode("starter", "starter"),
        makeNode("branch-b"),
        makeNode("branch-c"),
        makeNode("join"),
    };
    joinWorkflow.nodes[1].config.insert("mock_output", QJsonObject{{"branch", "b"}, {"value", 2}});
    joinWorkflow.nodes[2].config.insert("mock_output", QJsonObject{{"branch", "c"}, {"value", 3}});
    joinWorkflow.edges = {
        makeEdge("edge-starter-b", "starter", "branch-b"),
        makeEdge("edge-starter-c", "starter", "branch-c"),
        makeEdge("edge-b-join", "branch-b", "join"),
        makeEdge("edge-c-join", "branch-c", "join"),
    };
    const auto joinRun = fixture.engine.runWorkflow(joinWorkflow);
    if (const auto result = expect(joinRun.success, "Join workflow should succeed")) {
        return result;
    }
    if (const auto result = expect(nodeStatusEvents.contains("join:Waiting"),
            "Join node should enter Waiting before all upstream edges finish")) {
        return result;
    }
    const auto joinInput = joinRun.nodeResults.value("join").outputs.value("inputs").toObject().value("input").toArray();
    if (const auto result = expect(joinInput.size() == 2, "Join node should receive two merged output values")) {
        return result;
    }
    if (const auto result = expect(joinInput.at(0).toObject().value("branch").toString() == "b"
            && joinInput.at(1).toObject().value("branch").toString() == "c",
            "Merged input should be a list of upstream outputs in workflow edge order")) {
        return result;
    }

    vws::domain::Workflow slotWorkflow;
    slotWorkflow.workflowId = "slot-workflow";
    slotWorkflow.workspaceId = "workspace";
    slotWorkflow.nodes = {
        makeNode("slot-starter", "starter"),
        makeNode("slot-target"),
    };
    setIoDimension(slotWorkflow.nodes[0], 1, 3);
    setIoDimension(slotWorkflow.nodes[1], 3, 1);
    slotWorkflow.nodes[0].config.insert("mock_output", QJsonArray{"first", "second", "third"});
    auto slotEdge = makeEdge("edge-slot", "slot-starter", "slot-target");
    slotEdge.fromSlot = 1;
    slotEdge.toSlot = 2;
    slotWorkflow.edges = {slotEdge};
    const auto slotRun = fixture.engine.runWorkflow(slotWorkflow);
    if (const auto result = expect(slotRun.success, "Slot-level workflow should execute successfully")) {
        return result;
    }
    const auto slotInput = slotRun.nodeResults.value("slot-target").outputs.value("inputs").toObject().value("input").toArray();
    if (const auto result = expect(slotInput.size() == 3, "Slot-level input should be assembled as an array up to toSlot")) {
        return result;
    }
    if (const auto result = expect(slotInput.at(0).isNull()
            && slotInput.at(1).isNull()
            && slotInput.at(2).toString() == "second",
            "output[1] should be delivered to input[2] with earlier slots null")) {
        return result;
    }

    QStringList completionEvents;
    QObject::connect(&fixture.engine.eventBus(),
        &vws::execution::ExecutionEventBus::nodeStatusChanged,
        [&](const QString&, const QString& nodeId, const QString& status) {
            if (status == "Succeeded") {
                completionEvents.append(nodeId);
            }
        });
    auto slowFastWorkflow = joinWorkflow;
    slowFastWorkflow.workflowId = "slow-fast";
    slowFastWorkflow.nodes[1].nodeId = "slow";
    slowFastWorkflow.nodes[1].name = "slow";
    slowFastWorkflow.nodes[1].config.insert("mock_delay_ms", 250);
    slowFastWorkflow.nodes[2].nodeId = "fast";
    slowFastWorkflow.nodes[2].name = "fast";
    slowFastWorkflow.nodes[3].nodeId = "after";
    slowFastWorkflow.nodes[3].name = "after";
    slowFastWorkflow.edges = {
        makeEdge("edge-starter-slow", "starter", "slow"),
        makeEdge("edge-starter-fast", "starter", "fast"),
        makeEdge("edge-slow-after", "slow", "after"),
        makeEdge("edge-fast-after", "fast", "after"),
    };
    const auto slowFastRun = fixture.engine.runWorkflow(slowFastWorkflow);
    if (const auto result = expect(slowFastRun.success, "Slow/Fast workflow should succeed")) {
        return result;
    }
    if (const auto result = expect(completionEvents.indexOf("fast") >= 0
            && completionEvents.indexOf("slow") >= 0
            && completionEvents.indexOf("fast") < completionEvents.indexOf("slow"),
            "Fast branch should complete before slow branch")) {
        return result;
    }

    vws::domain::Workflow isolatedFailureWorkflow;
    isolatedFailureWorkflow.workflowId = "isolated-failure";
    isolatedFailureWorkflow.workspaceId = "workspace";
    isolatedFailureWorkflow.nodes = {
        makeNode("starter-1", "starter"),
        makeNode("fail"),
        makeNode("skipped-child"),
        makeNode("starter-2", "starter"),
        makeNode("x"),
        makeNode("y"),
    };
    isolatedFailureWorkflow.nodes[1].config.insert("mock_fail", true);
    isolatedFailureWorkflow.edges = {
        makeEdge("edge-s1-fail", "starter-1", "fail"),
        makeEdge("edge-fail-child", "fail", "skipped-child"),
        makeEdge("edge-s2-x", "starter-2", "x"),
        makeEdge("edge-x-y", "x", "y"),
    };
    const auto isolatedFailureRun = fixture.engine.runWorkflow(isolatedFailureWorkflow);
    if (const auto result = expect(isolatedFailureRun.status == "PartiallySucceeded",
            "Isolated failure should produce PartiallySucceeded")) {
        return result;
    }
    if (const auto result = expect(isolatedFailureRun.nodeStatuses.value("skipped-child") == "Skipped",
            "Downstream of failed node should be skipped")) {
        return result;
    }
    if (const auto result = expect(isolatedFailureRun.nodeStatuses.value("x") == "Succeeded"
            && isolatedFailureRun.nodeStatuses.value("y") == "Succeeded",
            "Independent starter branch should continue after another branch fails")) {
        return result;
    }

    vws::domain::Workflow cancelWorkflow;
    cancelWorkflow.workflowId = "cancel-workflow";
    cancelWorkflow.workspaceId = "workspace";
    cancelWorkflow.nodes = {
        makeNode("slow-starter", "starter"),
        makeNode("never-started"),
    };
    cancelWorkflow.nodes[0].config.insert("mock_delay_ms", 500);
    cancelWorkflow.edges = {
        makeEdge("edge-slow-next", "slow-starter", "never-started"),
    };

    vws::execution::WorkflowExecutionResult cancelRun;
    std::thread runner([&]() {
        cancelRun = fixture.engine.runWorkflow(cancelWorkflow);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    fixture.engine.requestCancelCurrentRun();
    runner.join();
    if (const auto result = expect(cancelRun.status == "Cancelled", "Cancelled run should finish as Cancelled")) {
        return result;
    }
    if (const auto result = expect(cancelRun.nodeStatuses.value("slow-starter") == "Cancelled"
            && cancelRun.nodeStatuses.value("never-started") == "Cancelled",
            "Cancelling a run should cancel running and pending nodes")) {
        return result;
    }

    QTextStream(stdout) << "execution engine tests passed" << Qt::endl;
    return 0;
}
