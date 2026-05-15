#include "execution/ExecutionEngine.h"

#include "execution/ExecutionRunState.h"
#include "execution/ExecutionOutputRouter.h"
#include "execution/ExecutionPlanner.h"
#include "execution/InputMerger.h"
#include "execution/NodeExecutionRequestBuilder.h"
#include "execution/NodeReadinessTracker.h"
#include "execution/NodeResultStatus.h"
#include "execution/NodeTaskRunner.h"
#include "execution/ExecutionSchedulingPolicy.h"
#include "execution/ThreadTrace.h"
#include "execution/WorkerPool.h"
#include "workers/WorkerRegistry.h"

#include <QDir>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>
#include <QSet>
#include <QWaitCondition>
#include <QUuid>

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
    ExecutionRunState runState(QUuid::createUuid().toString(QUuid::WithoutBraces), workflow.nodes.size());
    auto& result = runState.result();
    m_cancellationState.beginRun(result.runId);
    const auto effectiveRunPath = runPath.trimmed().isEmpty()
        ? QString{}
        : QDir(runPath).filePath(result.runId);
    auto publishThreadTrace = [&](const QString& nodeId, const QString& phase) {
        const auto trace = currentThreadTraceInfo();
        m_eventBus.publishThreadTrace(result.runId, nodeId, phase, trace.threadId, trace.threadName);
    };

    runState.setWorkflowStatus(WorkflowStatus::Validating);
    m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::Validating);
    publishThreadTrace({}, QStringLiteral("Workflow validation started"));

    ExecutionPlanner planner;
    const auto executionPlan = planner.plan(workflow);
    if (!executionPlan.valid) {
        result.success = false;
        runState.setWorkflowStatus(WorkflowStatus::Failed);
        result.errors = executionPlan.errors;
        m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::Failed);
        m_cancellationState.finishRun(result.runId);
        return result;
    }

    const auto& indexes = executionPlan.indexes;

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
        if (m_cancellationState.isCancelRequested()) {
            return;
        }
        const auto currentStatus = runState.nodeStatus(nodeId);
        if (!schedulingPolicy.canStillBeScheduled(currentStatus)) {
            return;
        }
        if (!schedulingPolicy.isStarterNode(indexes.nodesById.value(nodeId)) && !readiness.isReady(nodeId, indexes, runState.completedEdgeData())) {
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

        if (m_cancellationState.isCancelRequested() && runState.activeTasks() == 0) {
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

    submitNode = [&](const QString& nodeId) {
        const auto request = requestBuilder.build(nodeId, indexes, runState.completedEdgeData(), inputMerger);
        // scheduleIfReadyLocked already owns the mutex; this scope must not lock it again.
        publishThreadTrace(nodeId, QStringLiteral("Node task queued"));

        workerPool.submit([&, request]() {
            publishThreadTrace(request.nodeId, QStringLiteral("Node worker thread started"));
            {
                QMutexLocker locker(&mutex);
                setStatusLocked(request.nodeId, NodeStatus::Running);
            }

            auto nodeResult = nodeTaskRunner.execute(request);

            publishThreadTrace(request.nodeId, QStringLiteral("Node worker thread finished"));

            QMutexLocker locker(&mutex);
            runState.recordNodeResult(request.nodeId, nodeResult);
            runState.decrementActiveTasks();

            if (m_cancellationState.isCancelRequested()) {
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
                const auto failureStatus = statusForFailedNodeResult(nodeResult);
                runState.appendError(nodeResult.errorMessage);
                finishOneLocked(request.nodeId, failureStatus);
                m_eventBus.publishNodeError(result.runId, request.nodeId, nodeResult.errorMessage);
                skipDescendantsLocked(request.nodeId);
                maybeFinishLocked();
                return;
            }

            finishOneLocked(request.nodeId, NodeStatus::Succeeded);
            m_eventBus.publishNodeOutputReady(result.runId, request.nodeId, nodeResult.outputs);

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
            if (schedulingPolicy.isStarterNode(node)) {
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
    QPointer<QObject> guardedReceiver(receiver);
    m_runPool.start(QRunnable::create([this, workflow, workspacePath, runPath, artifactPath, guardedReceiver, onFinished = std::move(onFinished)]() mutable {
        auto result = runWorkflow(workflow, workspacePath, runPath, artifactPath);
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
