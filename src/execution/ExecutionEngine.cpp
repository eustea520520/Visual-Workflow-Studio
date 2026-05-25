#include "execution/ExecutionEngine.h"

#include "execution/ExecutionRunState.h"
#include "execution/ExecutionOutputRouter.h"
#include "execution/ExecutionPlanner.h"
#include "execution/InputMerger.h"
#include "execution/LoopNodeExecutor.h"
#include "execution/NestedWorkflowRunner.h"
#include "execution/NodeExecutionRequestBuilder.h"
#include "execution/NodeReadinessTracker.h"
#include "execution/NodeResultStatus.h"
#include "execution/NodeTaskRunner.h"
#include "execution/ExecutionSchedulingPolicy.h"
#include "execution/SubsystemNodeExecutor.h"
#include "execution/ThreadTrace.h"
#include "execution/WorkerPool.h"
#include "domain/NodeConfigKeys.h"
#include "domain/NodeTypes.h"
#include "workers/WorkerRegistry.h"

#include <QDir>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>
#include <QSet>
#include <QThread>
#include <QWaitCondition>
#include <QUuid>

#include <functional>

namespace vws::execution {

ExecutionEngine::ExecutionEngine(workers::WorkerRegistry& workerRegistry)
    : m_workerRegistry(workerRegistry)
{
    m_runPool.setMaxThreadCount(1);
}

ExecutionEventBus& ExecutionEngine::eventBus()
{
    return m_eventBus;
}

const ExecutionEventBus& ExecutionEngine::eventBus() const
{
    return m_eventBus;
}

QString ExecutionEngine::statusText() const
{
    return QString("Idle; registered worker types: %1")
        .arg(m_workerRegistry.registeredTypes().join(", "));
}

WorkflowExecutionResult ExecutionEngine::runWorkflow(
    const domain::Workflow& workflow,
    const QString& workspacePath,
    const QString& runPath,
    const QString& artifactPath)
{
    return runWorkflow(workflow, WorkflowRunOptions{}, workspacePath, runPath, artifactPath);
}

WorkflowExecutionResult ExecutionEngine::runWorkflow(
    const domain::Workflow& workflow,
    const WorkflowRunOptions& options,
    const QString& workspacePath,
    const QString& runPath,
    const QString& artifactPath)
{
    const bool usingExternalRunId = !options.runIdOverride.trimmed().isEmpty();
    ExecutionRunState runState(
        usingExternalRunId
            ? options.runIdOverride.trimmed()
            : QUuid::createUuid().toString(QUuid::WithoutBraces),
        workflow.nodes.size());
    auto& result = runState.result();
    m_cancellationState.beginRun(result.runId);
    auto isRunCancelRequested = [&]() {
        return m_cancellationState.isCancelRequested()
            || (options.cancelPredicate && options.cancelPredicate());
    };
    const auto effectiveRunPath = runPath.trimmed().isEmpty()
        ? QString{}
        : (usingExternalRunId ? runPath : QDir(runPath).filePath(result.runId));
    auto publishThreadTrace = [&](const QString& nodeId, const QString& phase) {
        const auto trace = currentThreadTraceInfo();
        m_eventBus.publishThreadTrace(result.runId, nodeId, phase, trace.threadId, trace.threadName);
    };

    runState.setWorkflowStatus(WorkflowStatus::Validating);
    m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::Validating);
    publishThreadTrace({}, QStringLiteral("Workflow validation started"));

    ExecutionPlanner planner;
    const auto executionPlan = planner.plan(workflow, options.validationMode);
    if (!executionPlan.valid) {
        result.success = false;
        runState.setWorkflowStatus(WorkflowStatus::Failed);
        result.errors = executionPlan.errors;
        m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::Failed);
        m_cancellationState.finishRun(result.runId);
        return result;
    }

    const auto& indexes = executionPlan.indexes;
    QHash<QString, QString> loopBodyByLoopNodeId;
    QSet<QString> loopBodyNodeIds;
    for (const auto& node : workflow.nodes) {
        if (node.type.trimmed().toLower() != domain::NodeTypes::Loop) {
            continue;
        }
        const auto outgoingEdges = indexes.outgoingEdgesByNode.value(node.nodeId);
        if (!outgoingEdges.isEmpty()) {
            const auto bodyNodeId = outgoingEdges.first().toNode;
            loopBodyByLoopNodeId.insert(node.nodeId, bodyNodeId);
            loopBodyNodeIds.insert(bodyNodeId);
        }
    }

    QMutex mutex;
    QWaitCondition finishedCondition;
    NodeReadinessTracker readiness;
    InputMerger inputMerger;
    NodeExecutionRequestBuilder requestBuilder(result.runId, workspacePath, effectiveRunPath, artifactPath);
    NodeTaskRunner nodeTaskRunner(m_workerRegistry);
    ExecutionOutputRouter outputRouter;
    ExecutionSchedulingPolicy schedulingPolicy;
    WorkerPool workerPool;

    auto setStatusLocked = [&](const QString& nodeId, NodeStatus status) {
        runState.setNodeStatus(nodeId, status);
        m_eventBus.publishNodeStatusChanged(result.runId, nodeId, status);
    };

    auto finishOneLocked = [&](const QString& nodeId, NodeStatus status) {
        if (!runState.finishNode(nodeId, status)) {
            return;
        }
        m_eventBus.publishNodeStatusChanged(result.runId, nodeId, status);
    };

    auto waitAfterVisibleNodeStatus = [&]() {
        if (options.nodeDispatchDelayMs > 0 && !isRunCancelRequested()) {
            QThread::msleep(static_cast<unsigned long>(options.nodeDispatchDelayMs));
        }
    };

    auto shouldPauseAfterStatus = [](NodeStatus status) {
        return status == NodeStatus::Running
            || status == NodeStatus::Succeeded
            || status == NodeStatus::Failed
            || status == NodeStatus::Timeout
            || status == NodeStatus::Cancelled;
    };

    std::function<void(const QString&)> skipDescendantsLocked = [&](const QString& nodeId) {
        for (const auto& edge : indexes.outgoingEdgesByNode.value(nodeId)) {
            const auto childId = edge.toNode;
            if (!schedulingPolicy.canStillBeScheduled(runState.nodeStatus(childId))) {
                continue;
            }
            finishOneLocked(childId, NodeStatus::Skipped);
            skipDescendantsLocked(childId);
        }
    };

    std::function<void(const QString&)> submitNode;
    auto scheduleIfReadyLocked = [&](const QString& nodeId) {
        if (isRunCancelRequested()) {
            return;
        }
        const auto currentStatus = runState.nodeStatus(nodeId);
        if (!schedulingPolicy.canStillBeScheduled(currentStatus)) {
            return;
        }
        const auto node = indexes.nodesById.value(nodeId);
        if (loopBodyNodeIds.contains(nodeId)) {
            return;
        }
        const bool implicitEntryNode = options.allowImplicitEntryNodes
            && indexes.incomingEdgesByNode.value(nodeId).isEmpty();
        if (!schedulingPolicy.isTopLevelEntryNode(node) && !implicitEntryNode
            && !readiness.isReady(nodeId, indexes, runState.completedEdgeData())) {
            if (currentStatus == NodeStatus::Pending) {
                setStatusLocked(nodeId, NodeStatus::Waiting);
            }
            return;
        }
        setStatusLocked(nodeId, NodeStatus::Queued);
        runState.incrementActiveTasks();
        submitNode(nodeId);
    };

    auto maybeFinishLocked = [&]() {
        if (!runState.hasUnfinishedNodes()) {
            finishedCondition.wakeAll();
            return;
        }

        if (isRunCancelRequested() && runState.activeTasks() == 0) {
            for (const auto& node : workflow.nodes) {
                if (!schedulingPolicy.isTerminalStatus(runState.nodeStatus(node.nodeId))) {
                    finishOneLocked(node.nodeId, NodeStatus::Cancelled);
                }
            }
            finishedCondition.wakeAll();
            return;
        }

        // If no task is running and nodes are still pending, they are unreachable or blocked by skipped parents.
        // Mark them as skipped so the run can finish deterministically.
        if (runState.activeTasks() == 0) {
            for (const auto& node : workflow.nodes) {
                if (schedulingPolicy.canStillBeScheduled(runState.nodeStatus(node.nodeId))) {
                    finishOneLocked(node.nodeId, NodeStatus::Skipped);
                }
            }
            finishedCondition.wakeAll();
        }
    };

    NestedWorkflowRunner nestedWorkflowRunner(m_workerRegistry, m_eventBus, isRunCancelRequested);
    SubsystemNodeExecutor subsystemNodeExecutor(
        [&](const QString& outerRunId,
            const domain::Workflow& nestedWorkflow,
            const WorkflowRunOptions& nestedOptions,
            const QString& nestedWorkspacePath,
            const QString& nestedRunPath,
            const QString& nestedArtifactPath) {
            return nestedWorkflowRunner.run(
                outerRunId,
                nestedWorkflow,
                nestedOptions,
                nestedWorkspacePath,
                nestedRunPath,
                nestedArtifactPath);
        },
        isRunCancelRequested);

    submitNode = [&](const QString& nodeId) {
        const auto request = requestBuilder.build(
            nodeId,
            indexes,
            runState.completedEdgeData(),
            inputMerger,
            options.initialInputsByNodeId);
        // scheduleIfReadyLocked already owns the mutex; this scope must not lock it again.
        publishThreadTrace(nodeId, QStringLiteral("Node task queued"));

        workerPool.submit([&, request]() {
            publishThreadTrace(request.nodeId, QStringLiteral("Node worker thread started"));
            {
                QMutexLocker locker(&mutex);
                if (request.nodeType != domain::NodeTypes::Loop
                    || !loopBodyByLoopNodeId.contains(request.nodeId)) {
                    setStatusLocked(request.nodeId, NodeStatus::Running);
                }
            }
            if (request.nodeType != domain::NodeTypes::Loop
                || !loopBodyByLoopNodeId.contains(request.nodeId)) {
                waitAfterVisibleNodeStatus();
            }

            LoopNodeExecutionResult loopExecutionResult;
            const bool isLoopNode = request.nodeType == domain::NodeTypes::Loop
                && loopBodyByLoopNodeId.contains(request.nodeId);
            auto nodeResult = [&]() {
                if (request.nodeType == domain::NodeTypes::Subsystem) {
                    return subsystemNodeExecutor.execute(request, options.nodeDispatchDelayMs);
                }
                if (isLoopNode) {
                    const auto bodyNodeId = loopBodyByLoopNodeId.value(request.nodeId);
                    const auto bodyNode = indexes.nodesById.value(bodyNodeId);
                    QList<domain::Edge> loopToBodyEdges;
                    for (const auto& edge : indexes.outgoingEdgesByNode.value(request.nodeId)) {
                        if (edge.toNode == bodyNodeId) {
                            loopToBodyEdges.append(edge);
                        }
                    }

                    const LoopNodeExecutor loopExecutor;
                    loopExecutionResult = loopExecutor.execute(
                        request,
                        bodyNode,
                        loopToBodyEdges,
                        request.nodeConfig.value(domain::NodeConfigKeys::LoopIterations).toInt(0),
                        [&](const NodeExecutionRequest& loopIterationRequest) {
                            return nodeTaskRunner.execute(loopIterationRequest);
                        },
                        [&](const NodeExecutionRequest& bodyRequest) {
                            return bodyRequest.nodeType == domain::NodeTypes::Subsystem
                                ? subsystemNodeExecutor.execute(bodyRequest, options.nodeDispatchDelayMs)
                                : nodeTaskRunner.execute(bodyRequest);
                        },
                        [&](int iteration, const QString& nodeId, NodeStatus status) {
                            Q_UNUSED(iteration);
                            m_eventBus.publishNodeStatusChanged(result.runId, nodeId, status);
                            if (shouldPauseAfterStatus(status)) {
                                waitAfterVisibleNodeStatus();
                            }
                        },
                        [&]() { return isRunCancelRequested(); });
                    if (loopExecutionResult.success) {
                        return loopExecutionResult.loopResult;
                    }
                    NodeExecutionResult failedResult = loopExecutionResult.loopResult;
                    failedResult.runId = request.runId;
                    failedResult.nodeId = request.nodeId;
                    failedResult.success = false;
                    if (failedResult.errorMessage.isEmpty()) {
                        failedResult.errorMessage = loopExecutionResult.errorMessage;
                    }
                    return failedResult;
                }
                return nodeTaskRunner.execute(request);
            }();

            publishThreadTrace(request.nodeId, QStringLiteral("Node worker thread finished"));

            QMutexLocker locker(&mutex);
            runState.recordNodeResult(request.nodeId, nodeResult);
            if (!isLoopNode) {
                runState.appendDebugOutput(request.nodeId, nodeResult.stdoutText);
            }
            runState.decrementActiveTasks();

            if (isRunCancelRequested()) {
                if (nodeResult.errorMessage.isEmpty()) {
                    nodeResult.errorMessage = "Run was cancelled.";
                }
                runState.appendError(nodeResult.errorMessage);
                runState.recordNodeResult(request.nodeId, nodeResult);
                finishOneLocked(request.nodeId, NodeStatus::Cancelled);
                maybeFinishLocked();
                return;
            }

            if (!nodeResult.success) {
                if (isLoopNode) {
                    runState.appendDebugOutputs(loopExecutionResult.debugOutputs);
                    const auto bodyNodeId = loopBodyByLoopNodeId.value(request.nodeId);
                    if (!loopExecutionResult.bodyResult.nodeId.isEmpty()) {
                        runState.recordNodeResult(bodyNodeId, loopExecutionResult.bodyResult);
                        if (schedulingPolicy.canStillBeScheduled(runState.nodeStatus(bodyNodeId))) {
                            runState.finishNode(bodyNodeId, statusForFailedNodeResult(loopExecutionResult.bodyResult));
                        }
                    }
                }
                const auto failureStatus = statusForFailedNodeResult(nodeResult);
                runState.appendError(nodeResult.errorMessage);
                if (isLoopNode) {
                    runState.finishNode(request.nodeId, failureStatus);
                } else {
                    finishOneLocked(request.nodeId, failureStatus);
                }
                m_eventBus.publishNodeError(result.runId, request.nodeId, nodeResult.errorMessage);
                skipDescendantsLocked(request.nodeId);
                maybeFinishLocked();
                return;
            }

            if (isLoopNode) {
                const auto bodyNodeId = loopBodyByLoopNodeId.value(request.nodeId);
                runState.appendDebugOutputs(loopExecutionResult.debugOutputs);
                runState.finishNode(request.nodeId, NodeStatus::Succeeded);
                m_eventBus.publishNodeOutputReady(result.runId, request.nodeId, nodeResult.outputs);

                runState.recordNodeResult(bodyNodeId, loopExecutionResult.bodyResult);
                if (schedulingPolicy.canStillBeScheduled(runState.nodeStatus(bodyNodeId))) {
                    runState.finishNode(bodyNodeId, NodeStatus::Succeeded);
                }
                m_eventBus.publishNodeOutputReady(result.runId, bodyNodeId, loopExecutionResult.bodyResult.outputs);

                const auto routedOutputs = outputRouter.route(bodyNodeId, indexes, loopExecutionResult.bodyResult);
                for (const auto& packet : routedOutputs.packets) {
                    runState.completedEdgeData().insert(packet.edgeId, packet);
                }
                for (const auto& downstreamNodeId : routedOutputs.downstreamNodeIds) {
                    scheduleIfReadyLocked(downstreamNodeId);
                }
                maybeFinishLocked();
                return;
            }

            finishOneLocked(request.nodeId, NodeStatus::Succeeded);
            m_eventBus.publishNodeOutputReady(result.runId, request.nodeId, nodeResult.outputs);
            waitAfterVisibleNodeStatus();

            const auto routedOutputs = outputRouter.route(request.nodeId, indexes, nodeResult);
            for (const auto& packet : routedOutputs.packets) {
                runState.completedEdgeData().insert(packet.edgeId, packet);
            }

            for (const auto& downstreamNodeId : routedOutputs.downstreamNodeIds) {
                scheduleIfReadyLocked(downstreamNodeId);
            }
            maybeFinishLocked();
        });
    };

    {
        QMutexLocker locker(&mutex);
        for (const auto& node : workflow.nodes) {
            runState.initializeNode(node.nodeId);
            m_eventBus.publishNodeStatusChanged(result.runId, node.nodeId, NodeStatus::Pending);
        }

        runState.setWorkflowStatus(WorkflowStatus::Running);
        m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::Running);

        for (const auto& node : workflow.nodes) {
            const bool implicitEntryNode = options.allowImplicitEntryNodes
                && indexes.incomingEdgesByNode.value(node.nodeId).isEmpty();
            if (!loopBodyNodeIds.contains(node.nodeId)
                && (schedulingPolicy.isTopLevelEntryNode(node) || implicitEntryNode)) {
                scheduleIfReadyLocked(node.nodeId);
            }
        }
        maybeFinishLocked();
    }

    {
        QMutexLocker locker(&mutex);
        while (runState.hasUnfinishedNodes()) {
            finishedCondition.wait(&mutex);
        }
    }

    workerPool.waitForDone();
    publishThreadTrace({}, QStringLiteral("Workflow worker pool drained"));

    const auto finalStatus = runState.applyFinalWorkflowStatus();
    m_eventBus.publishWorkflowStatusChanged(result.runId, finalStatus);

    m_cancellationState.finishRun(result.runId);

    return result;
}

void ExecutionEngine::runWorkflowAsync(
    const domain::Workflow& workflow,
    const QString& workspacePath,
    const QString& runPath,
    const QString& artifactPath,
    QObject* receiver,
    std::function<void(WorkflowExecutionResult)> onFinished)
{
    runWorkflowAsync(
        workflow,
        WorkflowRunOptions{},
        workspacePath,
        runPath,
        artifactPath,
        receiver,
        std::move(onFinished));
}

void ExecutionEngine::runWorkflowAsync(
    const domain::Workflow& workflow,
    const WorkflowRunOptions& options,
    const QString& workspacePath,
    const QString& runPath,
    const QString& artifactPath,
    QObject* receiver,
    std::function<void(WorkflowExecutionResult)> onFinished)
{
    QPointer<QObject> guardedReceiver(receiver);
    m_runPool.start(QRunnable::create([this, workflow, options, workspacePath, runPath, artifactPath, guardedReceiver, onFinished = std::move(onFinished)]() mutable {
        auto result = runWorkflow(workflow, options, workspacePath, runPath, artifactPath);
        if (guardedReceiver.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(guardedReceiver, [onFinished = std::move(onFinished), result = std::move(result)]() mutable {
            onFinished(std::move(result));
        }, Qt::QueuedConnection);
    }));
}

void ExecutionEngine::requestCancelCurrentRun()
{
    const auto runId = m_cancellationState.requestCancel();

    if (runId.isEmpty()) {
        return;
    }

    QSet<workers::INodeWorker*> notifiedWorkers;
    for (const auto& typeName : m_workerRegistry.registeredTypes()) {
        const auto worker = m_workerRegistry.workerForType(typeName);
        if (worker == nullptr || notifiedWorkers.contains(worker.get())) {
            continue;
        }
        notifiedWorkers.insert(worker.get());
        worker->cancel(runId);
    }
    m_eventBus.publishWorkflowStatusChanged(runId, WorkflowStatus::Cancelled);
}

} // namespace vws::execution
