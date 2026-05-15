#include "presentation/controllers/RunController.h"

#include "application/RunService.h"
#include "execution/ExecutionEngine.h"
#include "infrastructure/FileSystemUtils.h"
#include "presentation/state/AppStore.h"

#include <QDir>

namespace vws::presentation {

RunController::RunController(
    execution::ExecutionEngine& executionEngine,
    application::RunService& runService,
    AppStore& store,
    QObject* parent)
    : QObject(parent)
    , m_executionEngine(executionEngine)
    , m_runService(runService)
    , m_store(store)
{
    connect(&m_executionEngine.eventBus(), &execution::ExecutionEventBus::nodeStatusChanged, this,
        [this](const QString& runId, const QString& nodeId, const QString& status) {
            const auto workflowId = workflowIdForRun(runId);
            m_store.rememberRunWorkflow(runId, workflowId);
            m_store.cacheNodeStatus(workflowId, nodeId, status);
            emit nodeStatusChanged(runId, workflowId, nodeId, status);
        });

    connect(&m_executionEngine.eventBus(), &execution::ExecutionEventBus::workflowStatusChanged, this,
        [this](const QString& runId, const QString& status) {
            const auto workflowId = workflowIdForRun(runId);
            m_store.rememberRunWorkflow(runId, workflowId);
            emit workflowStatusChanged(runId, workflowId, status);
        });

    connect(&m_executionEngine.eventBus(), &execution::ExecutionEventBus::nodeOutputReady, this,
        [this](const QString& runId, const QString& nodeId, const QJsonObject& outputs) {
            const auto workflowId = workflowIdForRun(runId);
            m_store.rememberRunWorkflow(runId, workflowId);
            emit nodeOutputReady(runId, workflowId, nodeId, outputs);
        });

    connect(&m_executionEngine.eventBus(), &execution::ExecutionEventBus::nodeError, this,
        [this](const QString& runId, const QString& nodeId, const QString& message) {
            const auto workflowId = workflowIdForRun(runId);
            m_store.rememberRunWorkflow(runId, workflowId);
            emit nodeError(runId, workflowId, nodeId, message);
        });

    connect(&m_executionEngine.eventBus(), &execution::ExecutionEventBus::threadTrace, this,
        [this](const QString& runId, const QString& nodeId, const QString& phase, const QString& threadId, const QString& threadName) {
            const auto workflowId = workflowIdForRun(runId);
            m_store.rememberRunWorkflow(runId, workflowId);
            emit threadTrace(runId, workflowId, nodeId, phase, threadId, threadName);
        });
}

void RunController::prepareRun(const QString& workflowId)
{
    m_store.setActiveRunWorkflowId(workflowId);
    m_store.clearNodeStatusesForWorkflow(workflowId);
    m_store.setWorkflowRunning(workflowId, true);
    m_store.workflowRunning() = true;
    m_store.clearNodeOutputs();
}

void RunController::finishRun(const QString& workflowId)
{
    m_store.workflowRunning() = false;
    m_store.setWorkflowRunning(workflowId, false);
    m_store.clearActiveRunWorkflowId();
}

bool RunController::prepareCurrentWorkflowRun(WorkflowRunPlan& plan, QString* errorMessage)
{
    const auto& workflow = m_store.currentWorkflow();
    const auto& workspace = m_store.currentWorkspace();
    if (workspace.rootPath.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "No workspace is open.";
        }
        return false;
    }
    if (workflow.workflowId.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "No workflow is open.";
        }
        return false;
    }

    plan.workflowId = workflow.workflowId;
    plan.workflow = workflow;
    plan.workspaceRootPath = workspace.rootPath;
    plan.workspaceId = workspace.id;
    plan.runRootPath = QDir(workspace.rootPath).filePath("runs");
    plan.artifactPath = QDir(workspace.rootPath).filePath("artifacts");

    if (!infrastructure::FileSystemUtils::ensureDirectory(plan.artifactPath, errorMessage)) {
        return false;
    }

    prepareRun(plan.workflowId);
    return true;
}

QList<application::RunListEntry> RunController::recentRunEntries() const
{
    return m_runService.recentRunEntries(m_store.currentWorkspace().rootPath);
}

void RunController::runWorkflowAsync(
    const domain::Workflow& workflow,
    const QString& workspacePath,
    const QString& runPath,
    const QString& artifactPath,
    QObject* receiver,
    std::function<void(execution::WorkflowExecutionResult)> onFinished)
{
    m_executionEngine.runWorkflowAsync(
        workflow,
        workspacePath,
        runPath,
        artifactPath,
        receiver,
        std::move(onFinished));
}

bool RunController::saveRunRecord(
    const QString& workspaceRootPath,
    const QString& workspaceId,
    const domain::Workflow& workflow,
    const execution::WorkflowExecutionResult& result,
    QString* errorMessage)
{
    return m_runService.saveRunRecord(
        workspaceRootPath,
        workspaceId,
        workflow,
        result,
        errorMessage);
}

bool RunController::saveRunRecord(
    const WorkflowRunPlan& plan,
    const execution::WorkflowExecutionResult& result,
    QString* errorMessage)
{
    return saveRunRecord(
        plan.workspaceRootPath,
        plan.workspaceId,
        plan.workflow,
        result,
        errorMessage);
}

bool RunController::loadRunRecord(
    const QString& runId,
    domain::RunRecord& record,
    QString* errorMessage) const
{
    return m_runService.loadRunRecord(
        m_store.currentWorkspace().rootPath,
        runId,
        record,
        errorMessage);
}

bool RunController::loadRunRecordWithNodeOutputs(
    const QString& runId,
    domain::RunRecord& record,
    QHash<QString, QJsonObject>& nodeOutputsByNodeId,
    QString* errorMessage) const
{
    nodeOutputsByNodeId.clear();
    if (!loadRunRecord(runId, record, errorMessage)) {
        return false;
    }

    for (const auto& nodeRun : record.nodeRuns) {
        QJsonObject outputObject;
        if (loadNodeOutputObject(nodeRun, outputObject, nullptr)) {
            nodeOutputsByNodeId.insert(nodeRun.nodeId, outputObject.value("outputs").toObject());
        }
    }

    return true;
}

bool RunController::loadNodeOutputObject(
    const domain::NodeRunRecord& nodeRun,
    QJsonObject& object,
    QString* errorMessage) const
{
    return m_runService.loadNodeOutputObject(nodeRun, object, errorMessage);
}

void RunController::requestCancelCurrentRun()
{
    m_executionEngine.requestCancelCurrentRun();
}

QString RunController::workflowIdForRun(const QString& runId) const
{
    return m_store.workflowIdForRun(runId);
}

} // namespace vws::presentation
