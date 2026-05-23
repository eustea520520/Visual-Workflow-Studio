#include "presentation/state/AppStore.h"

namespace vws::presentation {

AppState& AppStore::state()
{
    return m_state;
}

const AppState& AppStore::state() const
{
    return m_state;
}

domain::Workspace& AppStore::currentWorkspace()
{
    return m_state.currentWorkspace;
}

const domain::Workspace& AppStore::currentWorkspace() const
{
    return m_state.currentWorkspace;
}

void AppStore::setCurrentWorkspace(const domain::Workspace& workspace)
{
    m_state.currentWorkspace = workspace;
}

domain::Workflow& AppStore::currentWorkflow()
{
    return m_state.workflowDocument.mutableWorkflow();
}

const domain::Workflow& AppStore::currentWorkflow() const
{
    return m_state.workflowDocument.workflow();
}

domain::Workflow AppStore::currentWorkflowSnapshot() const
{
    return m_state.workflowDocument.snapshot();
}

application::WorkflowDocument& AppStore::workflowDocument()
{
    return m_state.workflowDocument;
}

const application::WorkflowDocument& AppStore::workflowDocument() const
{
    return m_state.workflowDocument;
}

void AppStore::setCurrentWorkflow(
    const domain::Workflow& workflow,
    application::WorkflowDocument::ChangeState state)
{
    cacheCurrentWorkflowDocument();
    m_state.workflowDocument.replace(workflow, state);
    cacheCurrentWorkflowDocument();
}

void AppStore::updateCurrentWorkflowFromView(const domain::Workflow& workflow)
{
    m_state.workflowDocument.replaceFromView(workflow);
    cacheCurrentWorkflowDocument();
}

void AppStore::clearCurrentWorkflow()
{
    cacheCurrentWorkflowDocument();
    m_state.workflowDocument.clear();
}

void AppStore::cacheCurrentWorkflowDocument()
{
    if (!m_state.workflowDocument.hasWorkflow()) {
        return;
    }

    const auto workflowId = m_state.workflowDocument.workflow().workflowId.trimmed();
    if (!workflowId.isEmpty()) {
        m_state.openWorkflowDocumentsById.insert(workflowId, m_state.workflowDocument);
    }
}

bool AppStore::hasOpenWorkflowDocument(const QString& workflowId) const
{
    return m_state.openWorkflowDocumentsById.contains(workflowId.trimmed());
}

bool AppStore::openWorkflowSnapshot(const QString& workflowId, domain::Workflow* workflow) const
{
    const auto normalizedWorkflowId = workflowId.trimmed();
    if (normalizedWorkflowId.isEmpty() || workflow == nullptr) {
        return false;
    }

    if (m_state.workflowDocument.hasWorkflow()
        && m_state.workflowDocument.workflow().workflowId == normalizedWorkflowId) {
        *workflow = m_state.workflowDocument.snapshot();
        return true;
    }

    const auto it = m_state.openWorkflowDocumentsById.constFind(normalizedWorkflowId);
    if (it == m_state.openWorkflowDocumentsById.constEnd()) {
        return false;
    }

    *workflow = it.value().snapshot();
    return true;
}

void AppStore::replaceOpenWorkflowDocument(
    const domain::Workflow& workflow,
    application::WorkflowDocument::ChangeState state)
{
    const auto workflowId = workflow.workflowId.trimmed();
    if (workflowId.isEmpty()) {
        return;
    }

    application::WorkflowDocument document;
    document.replace(workflow, state);
    m_state.openWorkflowDocumentsById.insert(workflowId, document);
    if (m_state.workflowDocument.hasWorkflow()
        && m_state.workflowDocument.workflow().workflowId == workflowId) {
        m_state.workflowDocument = document;
    }
}

bool AppStore::activateOpenWorkflowDocument(const QString& workflowId)
{
    const auto normalizedWorkflowId = workflowId.trimmed();
    if (normalizedWorkflowId.isEmpty() || !m_state.openWorkflowDocumentsById.contains(normalizedWorkflowId)) {
        return false;
    }

    cacheCurrentWorkflowDocument();
    m_state.workflowDocument = m_state.openWorkflowDocumentsById.value(normalizedWorkflowId);
    return true;
}

void AppStore::removeOpenWorkflowDocument(const QString& workflowId)
{
    m_state.openWorkflowDocumentsById.remove(workflowId.trimmed());
}

void AppStore::markCurrentWorkflowSaved()
{
    m_state.workflowDocument.markSaved();
    cacheCurrentWorkflowDocument();
}

void AppStore::cacheWorkflowHistory(const QString& workflowId, const application::WorkflowHistory& history)
{
    const auto normalizedWorkflowId = workflowId.trimmed();
    if (!normalizedWorkflowId.isEmpty()) {
        m_state.workflowHistoriesById.insert(normalizedWorkflowId, history);
    }
}

bool AppStore::workflowHistory(const QString& workflowId, application::WorkflowHistory* history) const
{
    const auto normalizedWorkflowId = workflowId.trimmed();
    if (normalizedWorkflowId.isEmpty() || !m_state.workflowHistoriesById.contains(normalizedWorkflowId)) {
        return false;
    }

    if (history != nullptr) {
        *history = m_state.workflowHistoriesById.value(normalizedWorkflowId);
    }
    return true;
}

void AppStore::removeWorkflowHistory(const QString& workflowId)
{
    m_state.workflowHistoriesById.remove(workflowId.trimmed());
}

bool& AppStore::workflowRunning()
{
    return m_state.workflowRunning;
}

bool AppStore::workflowRunning() const
{
    return m_state.workflowRunning;
}

QHash<QString, QJsonObject>& AppStore::nodeOutputsByNodeId()
{
    return m_state.nodeOutputsByNodeId;
}

const QHash<QString, QJsonObject>& AppStore::nodeOutputsByNodeId() const
{
    return m_state.nodeOutputsByNodeId;
}

void AppStore::clearNodeOutputs()
{
    m_state.nodeOutputsByNodeId.clear();
}

QString& AppStore::selectedNodeId()
{
    return m_state.selectedNodeId;
}

const QString& AppStore::selectedNodeId() const
{
    return m_state.selectedNodeId;
}

void AppStore::clearSelection()
{
    m_state.selectedNodeId.clear();
}

QHash<QString, QHash<QString, QString>>& AppStore::nodeStatusesByWorkflowId()
{
    return m_state.nodeStatusesByWorkflowId;
}

const QHash<QString, QHash<QString, QString>>& AppStore::nodeStatusesByWorkflowId() const
{
    return m_state.nodeStatusesByWorkflowId;
}

void AppStore::clearNodeStatusesForWorkflow(const QString& workflowId)
{
    if (!workflowId.trimmed().isEmpty()) {
        m_state.nodeStatusesByWorkflowId[workflowId].clear();
    }
}

void AppStore::cacheNodeStatus(const QString& workflowId, const QString& nodeId, const QString& status)
{
    if (workflowId.trimmed().isEmpty() || nodeId.trimmed().isEmpty()) {
        return;
    }

    m_state.nodeStatusesByWorkflowId[workflowId].insert(nodeId, status);
}

QHash<QString, QString>& AppStore::workflowIdByRunId()
{
    return m_state.workflowIdByRunId;
}

const QHash<QString, QString>& AppStore::workflowIdByRunId() const
{
    return m_state.workflowIdByRunId;
}

void AppStore::rememberRunWorkflow(const QString& runId, const QString& workflowId)
{
    if (runId.trimmed().isEmpty() || workflowId.trimmed().isEmpty()) {
        return;
    }

    if (!m_state.workflowIdByRunId.contains(runId)) {
        m_state.workflowIdByRunId.insert(runId, workflowId);
    }
}

QString AppStore::workflowIdForRun(const QString& runId) const
{
    return m_state.workflowIdByRunId.value(runId, m_state.activeRunWorkflowId);
}

QSet<QString>& AppStore::runningWorkflowIds()
{
    return m_state.runningWorkflowIds;
}

const QSet<QString>& AppStore::runningWorkflowIds() const
{
    return m_state.runningWorkflowIds;
}

void AppStore::setWorkflowRunning(const QString& workflowId, bool running)
{
    if (workflowId.trimmed().isEmpty()) {
        return;
    }

    if (running) {
        m_state.runningWorkflowIds.insert(workflowId);
    } else {
        m_state.runningWorkflowIds.remove(workflowId);
    }
}

QString& AppStore::activeRunWorkflowId()
{
    return m_state.activeRunWorkflowId;
}

const QString& AppStore::activeRunWorkflowId() const
{
    return m_state.activeRunWorkflowId;
}

void AppStore::setActiveRunWorkflowId(const QString& workflowId)
{
    m_state.activeRunWorkflowId = workflowId;
}

void AppStore::clearActiveRunWorkflowId()
{
    m_state.activeRunWorkflowId.clear();
}

void AppStore::resetForWorkspaceChange()
{
    clearCurrentWorkflow();
    resetForWorkflowChange();
    m_state.openWorkflowDocumentsById.clear();
    m_state.workflowHistoriesById.clear();
    m_state.nodeStatusesByWorkflowId.clear();
    m_state.workflowIdByRunId.clear();
    m_state.runningWorkflowIds.clear();
    m_state.activeRunWorkflowId.clear();
    m_state.workflowRunning = false;
}

void AppStore::resetForWorkflowChange()
{
    clearSelection();
    clearNodeOutputs();
}

} // namespace vws::presentation
