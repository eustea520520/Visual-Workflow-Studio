#include "execution/ExecutionEngine.h"

#include "execution/DataPacket.h"
#include "execution/GraphIndexes.h"
#include "execution/GraphValidator.h"
#include "execution/InputMerger.h"
#include "execution/NodeExecutionRequest.h"
#include "execution/NodeReadinessTracker.h"
#include "execution/WorkerPool.h"
#include "workers/WorkerRegistry.h"

#include <QDir>
#include <QJsonValue>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>
#include <QSet>
#include <QThread>
#include <QWaitCondition>
#include <QUuid>

namespace vws::execution {

namespace {

bool isStarter(const domain::Node& node)
{
    return node.type == "starter";
}

QJsonValue extractOutputValue(const QJsonObject& outputs, const QString& fromPort)
{
    return outputs.contains(fromPort) ? outputs.value(fromPort) : QJsonValue(outputs);
}

NodeStatus statusForFailure(const NodeExecutionResult& result)
{
    const auto text = QString("%1\n%2").arg(result.errorMessage, result.stderrText).toLower();
    return text.contains("timed out") || text.contains("timeout")
        ? NodeStatus::Timeout
        : NodeStatus::Failed;
}

bool isTerminalStatus(NodeStatus status)
{
    return status == NodeStatus::Succeeded
        || status == NodeStatus::Failed
        || status == NodeStatus::Skipped
        || status == NodeStatus::Cancelled
        || status == NodeStatus::Timeout;
}

bool canStillBeScheduled(NodeStatus status)
{
    return status == NodeStatus::Pending || status == NodeStatus::Waiting;
}

QString currentThreadIdText()
{
    return QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16);
}

QString currentThreadName()
{
    const auto name = QThread::currentThread() != nullptr ? QThread::currentThread()->objectName() : QString();
    return name.trimmed().isEmpty() ? QStringLiteral("unnamed") : name;
}

} // namespace

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
    WorkflowExecutionResult result;
    result.runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QMutexLocker locker(&m_runControlMutex);
        m_cancelRequested.store(false);
        m_currentRunId = result.runId;
    }
    const auto effectiveRunPath = runPath.trimmed().isEmpty()
        ? QString{}
        : QDir(runPath).filePath(result.runId);

    result.status = workflowStatusToString(WorkflowStatus::Validating);
    m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::Validating);
    m_eventBus.publishThreadTrace(
        result.runId,
        {},
        QStringLiteral("Workflow validation started"),
        currentThreadIdText(),
        currentThreadName());

    GraphValidator validator;
    const auto validation = validator.validate(workflow);
    if (!validation.valid) {
        result.success = false;
        result.status = workflowStatusToString(WorkflowStatus::Failed);
        result.errors = validation.errors;
        m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::Failed);
        {
            QMutexLocker locker(&m_runControlMutex);
            if (m_currentRunId == result.runId) {
                m_currentRunId.clear();
            }
        }
        return result;
    }

    GraphIndexes indexes;
    indexes.build(workflow);

    QMutex mutex;
    QWaitCondition finishedCondition;
    QHash<QString, NodeStatus> nodeStates;
    QHash<QString, DataPacket> completedEdgeData;
    NodeReadinessTracker readiness;
    InputMerger inputMerger;
    WorkerPool workerPool;
    int unfinishedNodes = workflow.nodes.size();
    int activeTasks = 0;
    bool sawFailure = false;
    bool sawSuccess = false;
    bool sawCancellation = false;

    auto setStatusLocked = [&](const QString& nodeId, NodeStatus status) {
        nodeStates.insert(nodeId, status);
        result.nodeStatuses.insert(nodeId, nodeStatusToString(status));
        m_eventBus.publishNodeStatusChanged(result.runId, nodeId, status);
    };

    auto finishOneLocked = [&](const QString& nodeId, NodeStatus status) {
        if (isTerminalStatus(nodeStates.value(nodeId))) {
            return;
        }
        setStatusLocked(nodeId, status);
        --unfinishedNodes;
        if (status == NodeStatus::Succeeded) {
            sawSuccess = true;
        } else if (status == NodeStatus::Cancelled) {
            sawCancellation = true;
        } else {
            sawFailure = true;
        }
    };

    std::function<void(const QString&)> skipDescendantsLocked = [&](const QString& nodeId) {
        for (const auto& edge : indexes.outgoingEdgesByNode.value(nodeId)) {
            const auto childId = edge.toNode;
            if (!canStillBeScheduled(nodeStates.value(childId))) {
                continue;
            }
            finishOneLocked(childId, NodeStatus::Skipped);
            skipDescendantsLocked(childId);
        }
    };

    std::function<void(const QString&)> submitNode;
    auto scheduleIfReadyLocked = [&](const QString& nodeId) {
        if (m_cancelRequested.load()) {
            return;
        }
        const auto currentStatus = nodeStates.value(nodeId);
        if (!canStillBeScheduled(currentStatus)) {
            return;
        }
        if (!isStarter(indexes.nodesById.value(nodeId)) && !readiness.isReady(nodeId, indexes, completedEdgeData)) {
            if (currentStatus == NodeStatus::Pending) {
                setStatusLocked(nodeId, NodeStatus::Waiting);
            }
            return;
        }
        setStatusLocked(nodeId, NodeStatus::Queued);
        ++activeTasks;
        submitNode(nodeId);
    };

    auto maybeFinishLocked = [&]() {
        if (unfinishedNodes == 0) {
            finishedCondition.wakeAll();
            return;
        }

        if (m_cancelRequested.load() && activeTasks == 0) {
            for (const auto& node : workflow.nodes) {
                if (!isTerminalStatus(nodeStates.value(node.nodeId))) {
                    finishOneLocked(node.nodeId, NodeStatus::Cancelled);
                }
            }
            finishedCondition.wakeAll();
            return;
        }

        // 如果没有任何节点在运行，并且还剩 Pending 节点，说明它们不可达或依赖已被跳过。
        // 这些节点不能继续等待不存在的上游完成事件，应稳定标记为 Skipped。
        if (activeTasks == 0) {
            for (const auto& node : workflow.nodes) {
                if (canStillBeScheduled(nodeStates.value(node.nodeId))) {
                    finishOneLocked(node.nodeId, NodeStatus::Skipped);
                }
            }
            finishedCondition.wakeAll();
        }
    };

    submitNode = [&](const QString& nodeId) {
        NodeExecutionRequest request;
        // submitNode 只从 scheduleIfReadyLocked 调用，调用方已经持有 mutex。
        // 这里不能再次加锁，否则同一线程会在 QMutex 上自锁。
        const auto node = indexes.nodesById.value(nodeId);
        request.runId = result.runId;
        request.nodeId = node.nodeId;
        request.nodeType = node.type;
        request.nodeConfig = node.config;
        request.inputs = inputMerger.buildInputs(nodeId, indexes, completedEdgeData);
        request.workspacePath = workspacePath;
        request.runPath = effectiveRunPath;
        request.artifactPath = artifactPath;
        request.timeoutMs = node.runtime.timeoutMs;

        m_eventBus.publishThreadTrace(
            result.runId,
            nodeId,
            QStringLiteral("Node task queued"),
            currentThreadIdText(),
            currentThreadName());

        workerPool.submit([&, request]() {
            m_eventBus.publishThreadTrace(
                request.runId,
                request.nodeId,
                QStringLiteral("Node worker thread started"),
                currentThreadIdText(),
                currentThreadName());
            {
                QMutexLocker locker(&mutex);
                setStatusLocked(request.nodeId, NodeStatus::Running);
            }

            NodeExecutionResult nodeResult;
            const auto worker = m_workerRegistry.workerForType(request.nodeType);
            if (worker == nullptr) {
                nodeResult.runId = request.runId;
                nodeResult.nodeId = request.nodeId;
                nodeResult.success = false;
                nodeResult.errorMessage = QString("No worker registered for node type: %1").arg(request.nodeType);
            } else {
                nodeResult = worker->execute(request);
            }

            m_eventBus.publishThreadTrace(
                request.runId,
                request.nodeId,
                QStringLiteral("Node worker thread finished"),
                currentThreadIdText(),
                currentThreadName());

            QMutexLocker locker(&mutex);
            result.nodeResults.insert(request.nodeId, nodeResult);
            --activeTasks;

            if (m_cancelRequested.load()) {
                if (nodeResult.errorMessage.isEmpty()) {
                    nodeResult.errorMessage = "Run was cancelled.";
                }
                result.errors.append(nodeResult.errorMessage);
                result.nodeResults.insert(request.nodeId, nodeResult);
                finishOneLocked(request.nodeId, NodeStatus::Cancelled);
                maybeFinishLocked();
                return;
            }

            if (!nodeResult.success) {
                const auto failureStatus = statusForFailure(nodeResult);
                result.errors.append(nodeResult.errorMessage);
                finishOneLocked(request.nodeId, failureStatus);
                m_eventBus.publishNodeError(result.runId, request.nodeId, nodeResult.errorMessage);
                skipDescendantsLocked(request.nodeId);
                maybeFinishLocked();
                return;
            }

            finishOneLocked(request.nodeId, NodeStatus::Succeeded);
            m_eventBus.publishNodeOutputReady(result.runId, request.nodeId, nodeResult.outputs);

            QStringList downstreamNodes;
            for (const auto& edge : indexes.outgoingEdgesByNode.value(request.nodeId)) {
                completedEdgeData.insert(edge.edgeId, DataPacket{
                    edge.edgeId,
                    edge.fromNode,
                    edge.fromPort,
                    edge.toNode,
                    edge.toPort,
                    extractOutputValue(nodeResult.outputs, edge.fromPort),
                    nodeResult.artifacts,
                });
                downstreamNodes.append(edge.toNode);
            }
            downstreamNodes.removeDuplicates();

            for (const auto& downstreamNodeId : downstreamNodes) {
                scheduleIfReadyLocked(downstreamNodeId);
            }
            maybeFinishLocked();
        });
    };

    {
        QMutexLocker locker(&mutex);
        for (const auto& node : workflow.nodes) {
            nodeStates.insert(node.nodeId, NodeStatus::Pending);
            result.nodeStatuses.insert(node.nodeId, nodeStatusToString(NodeStatus::Pending));
            m_eventBus.publishNodeStatusChanged(result.runId, node.nodeId, NodeStatus::Pending);
        }

        result.status = workflowStatusToString(WorkflowStatus::Running);
        m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::Running);

        for (const auto& node : workflow.nodes) {
            if (isStarter(node)) {
                scheduleIfReadyLocked(node.nodeId);
            }
        }
        maybeFinishLocked();
    }

    {
        QMutexLocker locker(&mutex);
        while (unfinishedNodes > 0) {
            finishedCondition.wait(&mutex);
        }
    }

    workerPool.waitForDone();
    m_eventBus.publishThreadTrace(
        result.runId,
        {},
        QStringLiteral("Workflow worker pool drained"),
        currentThreadIdText(),
        currentThreadName());

    if (sawCancellation) {
        result.success = false;
        result.status = workflowStatusToString(WorkflowStatus::Cancelled);
        m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::Cancelled);
    } else if (!sawFailure && sawSuccess) {
        result.success = true;
        result.status = workflowStatusToString(WorkflowStatus::Succeeded);
        m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::Succeeded);
    } else if (sawSuccess && sawFailure) {
        result.success = false;
        result.status = workflowStatusToString(WorkflowStatus::PartiallySucceeded);
        m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::PartiallySucceeded);
    } else {
        result.success = false;
        result.status = workflowStatusToString(WorkflowStatus::Failed);
        m_eventBus.publishWorkflowStatusChanged(result.runId, WorkflowStatus::Failed);
    }

    {
        QMutexLocker locker(&m_runControlMutex);
        if (m_currentRunId == result.runId) {
            m_currentRunId.clear();
        }
    }

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
    QString runId;
    {
        QMutexLocker locker(&m_runControlMutex);
        runId = m_currentRunId;
        m_cancelRequested.store(true);
    }

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
