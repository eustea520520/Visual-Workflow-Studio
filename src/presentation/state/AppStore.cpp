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
    m_state.workflowDocument.replace(workflow, state);
}

void AppStore::updateCurrentWorkflowFromView(const domain::Workflow& workflow)
{
    m_state.workflowDocument.replaceFromView(workflow);
}

void AppStore::clearCurrentWorkflow()
{
    m_state.workflowDocument.clear();
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
