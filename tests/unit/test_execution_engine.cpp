#include "application/PythonCodeTemplates.h"
#include "application/WorkflowService.h"
#include "application/subsystem/SubsystemService.h"
#include "execution/ExecutionEngine.h"
#include "workers/MockNodeWorker.h"
#include "workers/PythonNodeWorker.h"
#include "workers/WorkerRegistry.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTextStream>

#include <chrono>
#include <memory>
#include <thread>

namespace {

const QString kPythonExecutable = "C:/Users/19272/anaconda3/python.exe";
const QString kWorkerScript = "python/python_worker.py";

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

class LoopWorker final : public vws::workers::INodeWorker {
public:
    explicit LoopWorker(bool failOnSecondIteration = false)
        : m_failOnSecondIteration(failOnSecondIteration)
    {
    }

    QString type() const override
    {
        return "loop";
    }

    vws::execution::NodeExecutionResult execute(const vws::execution::NodeExecutionRequest& request) override
    {
        vws::execution::NodeExecutionResult result;
        result.runId = request.runId;
        result.nodeId = request.nodeId;
        const auto loop = request.context.value("loop").toObject();
        const int iter = loop.value("iter").toInt(1);
        if (m_failOnSecondIteration && iter == 2) {
            result.success = false;
            result.errorMessage = "Loop iteration failed";
            result.stderrText = result.errorMessage;
            return result;
        }
        result.success = true;
        const auto originalInput = request.inputs.value("input").toArray();
        result.outputs = {
            {"output", QJsonArray{originalInput.isEmpty()
                ? QJsonValue(QJsonObject{})
                : QJsonValue(QJsonObject{
                    {"iter", iter},
                    {"seed", originalInput.first().toObject().value("seed").toInt()},
                    {"previous_body_output_seen", !loop.value("previous_body_output").isNull()},
                })}},
        };
        result.stdoutText = QString("Loop generated body input iter: %1").arg(iter);
        return result;
    }

    void cancel(const QString& executionId) override
    {
        Q_UNUSED(executionId);
    }

private:
    bool m_failOnSecondIteration = false;
};

vws::domain::Workflow makeLoopWorkflow(int iterations)
{
    vws::domain::Workflow workflow;
    workflow.workflowId = QString("loop-execution-%1").arg(iterations);
    workflow.workspaceId = "workspace";
    auto starter = makeNode("loop-starter", "starter");
    starter.config.insert("mock_output", QJsonObject{{"seed", 1}});
    auto loop = makeNode("loop", "loop");
    loop.outputPorts = {"output"};
    loop.config.insert("loop_iterations", iterations);
    auto body = makeNode("loop-body");
    auto sink = makeNode("loop-sink");
    workflow.nodes = {starter, loop, body, sink};
    workflow.edges = {
        makeEdge("edge-loop-starter-loop", "loop-starter", "loop"),
        makeEdge("edge-loop-body", "loop", "loop-body"),
        makeEdge("edge-body-sink", "loop-body", "loop-sink"),
    };
    return workflow;
}

vws::domain::Workflow makeLoopWorkflowWithBodyNode(vws::domain::Node bodyNode, int iterations)
{
    vws::domain::Workflow workflow;
    workflow.workflowId = QString("loop-body-%1").arg(bodyNode.nodeId);
    workflow.workspaceId = "workspace";
    auto starter = makeNode("loop-body-starter", "starter");
    starter.config.insert("mock_output", QJsonObject{{"seed", 1}});
    auto loop = makeNode("loop-body-loop", "loop");
    loop.outputPorts = {"output"};
    loop.config.insert("loop_iterations", iterations);
    auto sink = makeNode("loop-body-sink");
    workflow.nodes = {starter, loop, bodyNode, sink};
    workflow.edges = {
        makeEdge("edge-loop-body-starter-loop", "loop-body-starter", "loop-body-loop"),
        makeEdge("edge-loop-body-loop-body", "loop-body-loop", bodyNode.nodeId),
        makeEdge("edge-loop-body-body-sink", bodyNode.nodeId, "loop-body-sink"),
    };
    workflow.edges[1].toPort = bodyNode.inputPorts.value(0, "input");
    workflow.edges[2].fromPort = bodyNode.outputPorts.value(0, "output");
    return workflow;
}

vws::domain::Node makePythonNode(const QString& id, const QString& type, const QString& code)
{
    auto node = makeNode(id, type);
    node.config = {
        {"language", "python"},
        {"entry", "run"},
        {"code", code},
    };
    return node;
}

vws::domain::Workflow makeDefaultPythonLoopWorkflow()
{
    vws::domain::Workflow workflow;
    workflow.workflowId = "default-python-loop";
    workflow.workspaceId = "workspace";

    auto starter = makePythonNode(
        "python-starter",
        "starter",
        vws::application::PythonCodeTemplates::starterEmptyOutputCode());
    auto loop = makePythonNode(
        "python-loop",
        "loop",
        vws::application::PythonCodeTemplates::loopCode());
    loop.config.insert("loop_iterations", 1);
    auto body = makePythonNode(
        "python-body",
        "function",
        vws::application::PythonCodeTemplates::defaultFunctionCode());

    workflow.nodes = {starter, loop, body};
    workflow.edges = {
        makeEdge("edge-python-starter-loop", "python-starter", "python-loop"),
        makeEdge("edge-python-loop-body", "python-loop", "python-body"),
    };
    return workflow;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Verify simple chains, joins, failures, event delivery, and cancellation.
    EngineFixture fixture;
    auto aliasWorker = std::make_shared<vws::workers::MockNodeWorker>("function");
    fixture.registry.registerWorkerForType("starter", aliasWorker);
    fixture.registry.registerWorkerForType("agent", aliasWorker);
    fixture.registry.registerWorkerForType("loop", std::make_shared<LoopWorker>());
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

    vws::application::SubsystemService subsystemService;
    vws::domain::NodePosition subsystemPosition;
    auto subsystemNode = subsystemService.createSubsystemNode("workspace-test", "Nested", subsystemPosition);
    vws::domain::Workflow childWorkflow;
    childWorkflow.workflowId = "child-workflow";
    childWorkflow.workspaceId = "workspace-test";
    childWorkflow.name = "Child";
    childWorkflow.nodes = {makeNode("inner")};
    QString subsystemError;
    if (!subsystemService.saveSubsystemWorkflow(subsystemNode, childWorkflow, &subsystemError)) {
        return fail(subsystemError);
    }

    vws::domain::Workflow subsystemWorkflow;
    subsystemWorkflow.workflowId = "subsystem-parent";
    subsystemWorkflow.workspaceId = "workspace-test";
    subsystemWorkflow.name = "Subsystem Parent";
    auto subsystemStarter = makeNode("subsystem-starter", "starter");
    subsystemStarter.config.insert("mock_output", QJsonObject{{"value", 7}});
    auto subsystemSink = makeNode("subsystem-sink");
    subsystemWorkflow.nodes = {subsystemStarter, subsystemNode, subsystemSink};
    subsystemWorkflow.edges = {
        makeEdge("edge-starter-subsystem", "subsystem-starter", subsystemNode.nodeId),
        makeEdge("edge-subsystem-sink", subsystemNode.nodeId, "subsystem-sink"),
    };
    subsystemWorkflow.edges[0].toPort = subsystemNode.inputPorts.first();
    subsystemWorkflow.edges[1].fromPort = subsystemNode.outputPorts.first();
    const auto subsystemRun = fixture.engine.runWorkflow(subsystemWorkflow);
    if (const auto result = expect(subsystemRun.success,
            "Subsystem node should execute embedded workflow through existing engine")) {
        return result;
    }
    const auto sinkInputs = subsystemRun.nodeResults.value("subsystem-sink").outputs.value("inputs").toObject();
    if (const auto result = expect(sinkInputs.contains("input"),
            "Subsystem output should be routed to downstream node input")) {
        return result;
    }
    if (const auto result = expect(subsystemRun.nodeResults.value(subsystemNode.nodeId).stdoutText.contains("Mock node succeeded: inner"),
            "Subsystem node should aggregate stdout from internal nodes for output-panel debug display")) {
        return result;
    }
    if (const auto result = expect(nodeStatusEvents.contains("inner:Running"),
            "Subsystem child node status should be forwarded to the parent run event stream")) {
        return result;
    }

    auto sourceSubsystemNode = subsystemService.createSubsystemNode("workspace-test", "Source Nested", subsystemPosition);
    vws::domain::Workflow sourceChildWorkflow;
    sourceChildWorkflow.workflowId = "source-child-workflow";
    sourceChildWorkflow.workspaceId = "workspace-test";
    sourceChildWorkflow.name = "Source Child";
    auto innerStarter = makeNode("inner-starter", "starter");
    innerStarter.config.insert("mock_output", QJsonObject{{"value", 11}});
    sourceChildWorkflow.nodes = {innerStarter};
    if (!subsystemService.saveSubsystemWorkflow(sourceSubsystemNode, sourceChildWorkflow, &subsystemError)) {
        return fail(subsystemError);
    }

    vws::domain::Workflow zeroInputSubsystemWorkflow;
    zeroInputSubsystemWorkflow.workflowId = "zero-input-subsystem-parent";
    zeroInputSubsystemWorkflow.workspaceId = "workspace-test";
    zeroInputSubsystemWorkflow.name = "Zero Input Subsystem Parent";
    auto zeroInputSink = makeNode("zero-input-sink");
    zeroInputSubsystemWorkflow.nodes = {sourceSubsystemNode, zeroInputSink};
    zeroInputSubsystemWorkflow.edges = {
        makeEdge("edge-source-subsystem-sink", sourceSubsystemNode.nodeId, "zero-input-sink"),
    };
    zeroInputSubsystemWorkflow.edges[0].fromPort = sourceSubsystemNode.outputPorts.first();
    const auto zeroInputSubsystemRun = fixture.engine.runWorkflow(zeroInputSubsystemWorkflow);
    if (const auto result = expect(zeroInputSubsystemRun.success,
            "Zero-input Subsystem node should run as a top-level graph entry")) {
        return result;
    }
    if (const auto result = expect(zeroInputSubsystemRun.nodeStatuses.value(sourceSubsystemNode.nodeId) == "Succeeded",
            "Zero-input Subsystem entry should be scheduled and succeed")) {
        return result;
    }

    if (QFileInfo::exists(kPythonExecutable) && QFileInfo::exists(kWorkerScript)) {
        QTemporaryDir pythonLoopDir;
        if (!pythonLoopDir.isValid()) {
            return fail("Could not create temporary directory for default Python Loop test");
        }

        vws::workers::WorkerRegistry pythonRegistry;
        auto pythonWorker = std::make_shared<vws::workers::PythonNodeWorker>(kPythonExecutable, kWorkerScript);
        pythonRegistry.registerWorkerForType("starter", pythonWorker);
        pythonRegistry.registerWorkerForType("function", pythonWorker);
        pythonRegistry.registerWorkerForType("loop", pythonWorker);
        vws::execution::ExecutionEngine pythonEngine(pythonRegistry);
        const auto defaultPythonLoopRun = pythonEngine.runWorkflow(
            makeDefaultPythonLoopWorkflow(),
            pythonLoopDir.path(),
            QDir(pythonLoopDir.path()).filePath("runs"),
            QDir(pythonLoopDir.path()).filePath("artifacts"));

        if (const auto result = expect(defaultPythonLoopRun.success,
                QString("Default Python Loop workflow should finish without hanging or crashing: %1")
                    .arg(defaultPythonLoopRun.errors.join("; ")))) {
            return result;
        }
        if (const auto result = expect(defaultPythonLoopRun.nodeStatuses.value("python-loop") == "Succeeded"
                && defaultPythonLoopRun.nodeStatuses.value("python-body") == "Succeeded",
                "Default Python Loop and its body should both reach terminal success")) {
            return result;
        }
    }

    const auto singleIterationLoopRun = fixture.engine.runWorkflow(makeLoopWorkflow(1));
    if (const auto result = expect(singleIterationLoopRun.success,
            QString("Single Loop node should complete one iteration: %1")
                .arg(singleIterationLoopRun.errors.join("; ")))) {
        return result;
    }
    if (const auto result = expect(singleIterationLoopRun.nodeResults.value("loop")
                .metadata.value("loop_summary").toObject().value("iteration_count").toInt() == 1,
            "Loop node should report completed iterations in metadata")) {
        return result;
    }
    if (const auto result = expect(singleIterationLoopRun.nodeResults.value("loop-body")
                .stdoutText.contains("Mock node succeeded: loop-body"),
            "Loop body Function stdout should be visible in the parent run result")) {
        return result;
    }
    if (const auto result = expect(singleIterationLoopRun.nodeResults.value("loop")
                .stdoutText.contains("Loop generated body input iter: 1"),
            "Loop node stdout should be visible in the parent run result")) {
        return result;
    }
    QList<vws::execution::NodeDebugOutput> loopDebugOutputs;
    for (const auto& debugOutput : singleIterationLoopRun.debugOutputs) {
        if (debugOutput.nodeId == "loop" || debugOutput.nodeId == "loop-body") {
            loopDebugOutputs.append(debugOutput);
        }
    }
    if (const auto result = expect(loopDebugOutputs.size() >= 2
            && loopDebugOutputs.at(0).nodeId == "loop"
            && loopDebugOutputs.at(0).text.contains("iter: 1")
            && loopDebugOutputs.at(1).nodeId == "loop-body"
            && loopDebugOutputs.at(1).text.contains("Mock node succeeded: loop-body"),
            "Loop debug output should preserve execution order: loop node then body node")) {
        return result;
    }

    auto loopSubsystemNode = subsystemService.createSubsystemNode("workspace-test", "Loop Body Nested", subsystemPosition);
    vws::domain::Workflow loopSubsystemChild;
    loopSubsystemChild.workflowId = "loop-subsystem-child";
    loopSubsystemChild.workspaceId = "workspace-test";
    loopSubsystemChild.name = "Loop Subsystem Child";
    loopSubsystemChild.nodes = {makeNode("loop-inner")};
    if (!subsystemService.saveSubsystemWorkflow(loopSubsystemNode, loopSubsystemChild, &subsystemError)) {
        return fail(subsystemError);
    }
    const auto loopSubsystemRun = fixture.engine.runWorkflow(makeLoopWorkflowWithBodyNode(loopSubsystemNode, 5));
    if (const auto result = expect(loopSubsystemRun.success,
            QString("Loop with Subsystem body should complete fixed iterations: %1")
                .arg(loopSubsystemRun.errors.join("; ")))) {
        return result;
    }
    if (const auto result = expect(loopSubsystemRun.nodeResults.value(loopSubsystemNode.nodeId)
                .metadata.value("loop_summary").toObject().value("iteration_count").toInt() == 5,
            "Loop with Subsystem body should report completed iterations in metadata")) {
        return result;
    }
    if (const auto result = expect(loopSubsystemRun.nodeResults.value(loopSubsystemNode.nodeId)
                .stdoutText.contains("Mock node succeeded: loop-inner"),
            "Loop body Subsystem stdout should be visible in the parent run result")) {
        return result;
    }

    auto emptySubsystemNode = subsystemService.createSubsystemNode("workspace-test", "Empty Loop Body", subsystemPosition);
    emptySubsystemNode.inputPorts = {"input"};
    emptySubsystemNode.outputPorts = {"output"};
    const auto emptySubsystemLoopRun = fixture.engine.runWorkflow(makeLoopWorkflowWithBodyNode(emptySubsystemNode, 5));
    if (const auto result = expect(!emptySubsystemLoopRun.success,
            "Empty Subsystem inside a loop should fail cleanly")) {
        return result;
    }
    if (const auto result = expect(emptySubsystemLoopRun.errors.join("; ").contains("Subsystem workflow is empty"),
            "Empty Subsystem loop failure should explain that the embedded workflow is empty")) {
        return result;
    }

    auto slowSubsystemNode = subsystemService.createSubsystemNode("workspace-test", "Slow Loop Body", subsystemPosition);
    vws::domain::Workflow slowSubsystemChild;
    slowSubsystemChild.workflowId = "slow-loop-subsystem-child";
    slowSubsystemChild.workspaceId = "workspace-test";
    slowSubsystemChild.name = "Slow Loop Subsystem Child";
    auto slowInnerNode = makeNode("slow-loop-inner");
    slowInnerNode.config.insert("mock_delay_ms", 500);
    slowSubsystemChild.nodes = {slowInnerNode};
    if (!subsystemService.saveSubsystemWorkflow(slowSubsystemNode, slowSubsystemChild, &subsystemError)) {
        return fail(subsystemError);
    }
    vws::execution::WorkflowExecutionResult slowSubsystemCancelRun;
    std::thread slowSubsystemCancelRunner([&]() {
        slowSubsystemCancelRun = fixture.engine.runWorkflow(makeLoopWorkflowWithBodyNode(slowSubsystemNode, 100));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    fixture.engine.requestCancelCurrentRun();
    slowSubsystemCancelRunner.join();
    if (const auto result = expect(slowSubsystemCancelRun.status == "Cancelled",
            "Cancelling a loop run inside a Subsystem body should finish as Cancelled")) {
        return result;
    }

    const auto loopStatusOffset = nodeStatusEvents.size();
    const auto loopRun = fixture.engine.runWorkflow(makeLoopWorkflow(3));
    if (const auto result = expect(loopRun.success,
            QString("Loop workflow should succeed: %1").arg(loopRun.errors.join("; ")))) {
        return result;
    }
    if (const auto result = expect(loopRun.nodeStatuses.value("loop") == "Succeeded"
            && loopRun.nodeStatuses.value("loop-body") == "Succeeded",
            "Loop node and body node should be marked succeeded after loop execution")) {
        return result;
    }
    const auto loopStatusEvents = nodeStatusEvents.mid(loopStatusOffset);
    if (const auto result = expect(loopStatusEvents.count("loop:Running") >= 3
            && loopStatusEvents.count("loop-body:Running") >= 3,
            "Loop execution should publish per-iteration Running statuses for Loop and body nodes")) {
        return result;
    }
    QStringList expectedLoopBodySequence;
    for (int i = 0; i < 3; ++i) {
        expectedLoopBodySequence
            << "loop:Queued"
            << "loop:Running"
            << "loop:Succeeded"
            << "loop-body:Queued"
            << "loop-body:Running"
            << "loop-body:Succeeded";
    }
    int expectedStatusIndex = 0;
    for (const auto& event : loopStatusEvents) {
        if (expectedStatusIndex < expectedLoopBodySequence.size()
            && event == expectedLoopBodySequence.at(expectedStatusIndex)) {
            ++expectedStatusIndex;
        }
    }
    if (const auto result = expect(expectedStatusIndex == expectedLoopBodySequence.size(),
            "Loop execution should expose the same visible order as unrolled Loop/Body node pairs")) {
        return result;
    }
    const auto loopMetadata = loopRun.nodeResults.value("loop").metadata.value("loop_summary").toObject();
    const auto bodyMetadata = loopRun.nodeResults.value("loop-body").metadata.value("loop_summary").toObject();
    if (const auto result = expect(loopMetadata.value("iteration_count").toInt() == 3,
            "Loop node should report three completed iterations in metadata")) {
        return result;
    }
    if (const auto result = expect(bodyMetadata.value("history").toArray().size() == 3,
            "Loop body metadata history should contain one final body output per iteration")) {
        return result;
    }
    const auto loopSinkInput = loopRun.nodeResults.value("loop-sink").outputs.value("inputs").toObject().value("input").toArray();
    if (const auto result = expect(!loopSinkInput.isEmpty()
            && loopSinkInput.first().toObject().value("inputs").toObject().value("input").toArray().first().toObject().value("iter").toInt() == 3,
            "Downstream node should receive the last loop body business output")) {
        return result;
    }

    fixture.registry.registerWorkerForType("loop", std::make_shared<LoopWorker>(true));
    const auto loopFailureRun = fixture.engine.runWorkflow(makeLoopWorkflow(3));
    if (const auto result = expect(!loopFailureRun.success,
            "Loop should fail when one loop iteration fails")) {
        return result;
    }
    if (const auto result = expect(loopFailureRun.nodeStatuses.value("loop-sink") == "Skipped",
            "Downstream node should be skipped after loop iteration failure")) {
        return result;
    }
    fixture.registry.registerWorkerForType("loop", std::make_shared<LoopWorker>());

    auto loopCancelWorkflow = makeLoopWorkflow(100);
    loopCancelWorkflow.workflowId = "loop-cancel";
    loopCancelWorkflow.nodes[2].config.insert("mock_delay_ms", 500);
    vws::execution::WorkflowExecutionResult loopCancelRun;
    std::thread loopCancelRunner([&]() {
        loopCancelRun = fixture.engine.runWorkflow(loopCancelWorkflow);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    fixture.engine.requestCancelCurrentRun();
    loopCancelRunner.join();
    if (const auto result = expect(loopCancelRun.status == "Cancelled",
            "Cancelling a loop run should stop creating further loop iterations and finish as Cancelled")) {
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
    setIoDimension(joinWorkflow.nodes[3], 2, 1);
    joinWorkflow.nodes[1].config.insert("mock_output", QJsonObject{{"branch", "b"}, {"value", 2}});
    joinWorkflow.nodes[2].config.insert("mock_output", QJsonObject{{"branch", "c"}, {"value", 3}});
    joinWorkflow.edges = {
        makeEdge("edge-starter-b", "starter", "branch-b"),
        makeEdge("edge-starter-c", "starter", "branch-c"),
        makeEdge("edge-b-join", "branch-b", "join"),
        makeEdge("edge-c-join", "branch-c", "join"),
    };
    joinWorkflow.edges[3].toSlot = 1;
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
    slowFastWorkflow.edges[3].toSlot = 1;
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
