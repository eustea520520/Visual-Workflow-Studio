#include "presentation/controllers/WorkflowController.h"

#include "application/WorkflowService.h"
#include "presentation/state/AppStore.h"

#include <QDateTime>

namespace vws::presentation {

WorkflowController::WorkflowController(application::WorkflowService& workflowService, AppStore& store)
    : m_workflowService(workflowService)
    , m_store(store)
{
}

void WorkflowController::createWorkflow(const QString& name)
{
    auto workflow = m_workflowService.createEmptyWorkflow(m_store.currentWorkspace().id, name);
    m_store.setCurrentWorkflow(workflow, application::WorkflowDocument::ChangeState::Dirty);
    m_store.resetForWorkflowChange();
}

QList<domain::Workflow> WorkflowController::listWorkflows(QString* errorMessage) const
{
    return m_workflowService.listWorkflows(m_store.currentWorkspace().rootPath, errorMessage);
}

bool WorkflowController::loadWorkflowFile(const QString& filePath, QString* errorMessage)
{
    domain::Workflow workflow;
    if (!m_workflowService.loadWorkflow(filePath, workflow, errorMessage)) {
        return false;
    }

    if (workflow.workspaceId.isEmpty()) {
        workflow.workspaceId = m_store.currentWorkspace().id;
    }

    if (m_store.hasOpenWorkflowDocument(workflow.workflowId)
        && m_store.activateOpenWorkflowDocument(workflow.workflowId)) {
        m_store.resetForWorkflowChange();
        return true;
    }

    m_store.setCurrentWorkflow(workflow);
    m_store.resetForWorkflowChange();
    return true;
}

bool WorkflowController::loadWorkflowFromWorkspace(const QString& workflowId, QString* errorMessage)
{
    if (m_store.activateOpenWorkflowDocument(workflowId)) {
        m_store.resetForWorkflowChange();
        return true;
    }

    domain::Workflow workflow;
    if (!m_workflowService.loadWorkflowFromWorkspace(
            m_store.currentWorkspace().rootPath,
            workflowId,
            workflow,
            errorMessage)) {
        return false;
    }

    if (workflow.workspaceId.isEmpty()) {
        workflow.workspaceId = m_store.currentWorkspace().id;
    }
    m_store.setCurrentWorkflow(workflow);
    m_store.resetForWorkflowChange();
    return true;
}

bool WorkflowController::workflowSnapshot(const QString& workflowId, domain::Workflow& workflow, QString* errorMessage) const
{
    const auto normalizedWorkflowId = workflowId.trimmed();
    if (normalizedWorkflowId.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Workflow id is empty.");
        }
        return false;
    }

    if (m_store.openWorkflowSnapshot(normalizedWorkflowId, &workflow)) {
        return true;
    }

    const auto loaded = m_workflowService.loadWorkflowFromWorkspace(
        m_store.currentWorkspace().rootPath,
        normalizedWorkflowId,
        workflow,
        errorMessage);
    if (loaded && workflow.workspaceId.isEmpty()) {
        workflow.workspaceId = m_store.currentWorkspace().id;
    }
    return loaded;
}

bool WorkflowController::renameWorkflow(const QString& workflowId, const QString& newName, QString* errorMessage)
{
    const auto normalizedName = newName.trimmed();
    if (normalizedName.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Workflow name cannot be empty.");
        }
        return false;
    }

    domain::Workflow workflow;
    if (!workflowSnapshot(workflowId, workflow, errorMessage)) {
        return false;
    }

    workflow.name = normalizedName;
    workflow.updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!m_workflowService.saveWorkflowToWorkspace(
            m_store.currentWorkspace().rootPath,
            workflow,
            errorMessage)) {
        return false;
    }

    m_store.replaceOpenWorkflowDocument(workflow);
    return true;
}

bool WorkflowController::loadWorkflowSnapshotForRun(
    const domain::RunRecord& record,
    domain::Workflow& workflowSnapshot,
    bool* usedCurrentWorkflowFile,
    QString* errorMessage) const
{
    if (usedCurrentWorkflowFile != nullptr) {
        *usedCurrentWorkflowFile = false;
    }

    if (!record.workflowSnapshotPath.trimmed().isEmpty()) {
        if (m_workflowService.loadWorkflow(record.workflowSnapshotPath, workflowSnapshot, errorMessage)) {
            return true;
        }
        return false;
    }

    if (record.workflowId.trimmed().isEmpty()) {
        return false;
    }

    const auto loaded = m_workflowService.loadWorkflowFromWorkspace(
        m_store.currentWorkspace().rootPath,
        record.workflowId,
        workflowSnapshot,
        errorMessage);
    if (loaded && usedCurrentWorkflowFile != nullptr) {
        *usedCurrentWorkflowFile = true;
    }
    return loaded;
}

bool WorkflowController::saveCurrentWorkflow(QString* errorMessage)
{
    const auto saved = m_workflowService.saveWorkflowToWorkspace(
        m_store.currentWorkspace().rootPath,
        m_store.currentWorkflow(),
        errorMessage);
    if (saved) {
        m_store.markCurrentWorkflowSaved();
    }
    return saved;
}

bool WorkflowController::replaceCurrentWorkflowAndSave(const domain::Workflow& workflow, QString* errorMessage)
{
    m_store.setCurrentWorkflow(workflow, application::WorkflowDocument::ChangeState::Dirty);
    return saveCurrentWorkflow(errorMessage);
}

void WorkflowController::syncCurrentWorkflowFromView(const domain::Workflow& workflow)
{
    m_store.updateCurrentWorkflowFromView(workflow);
}

bool WorkflowController::updateNodeDetails(
    const QString& nodeId,
    const QString& name,
    const QString& description,
    int timeoutMs,
    const QString& code,
    const QJsonObject& configPatch,
    QString* errorMessage)
{
    const auto updated = m_workflowService.updateNodeDetails(
        m_store.currentWorkflow(),
        nodeId,
        name,
        description,
        timeoutMs,
        code,
        configPatch,
        errorMessage);
    if (updated) {
        m_store.workflowDocument().markDirty();
    }
    return updated;
}

} // namespace vws::presentation
