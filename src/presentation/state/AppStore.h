#pragma once

#include "presentation/state/AppState.h"

namespace vws::presentation {

// AppStore centralizes mutable UI/application state. It intentionally does not
// know about QWidget, QGraphicsScene, or persistence services.
class AppStore {
public:
    AppState& state();
    const AppState& state() const;

    domain::Workspace& currentWorkspace();
    const domain::Workspace& currentWorkspace() const;
    void setCurrentWorkspace(const domain::Workspace& workspace);

    domain::Workflow& currentWorkflow();
    const domain::Workflow& currentWorkflow() const;
    domain::Workflow currentWorkflowSnapshot() const;
    application::WorkflowDocument& workflowDocument();
    const application::WorkflowDocument& workflowDocument() const;
    void setCurrentWorkflow(
        const domain::Workflow& workflow,
        application::WorkflowDocument::ChangeState state = application::WorkflowDocument::ChangeState::Clean);
    void updateCurrentWorkflowFromView(const domain::Workflow& workflow);
    void clearCurrentWorkflow();

    bool& workflowRunning();
    bool workflowRunning() const;

    QHash<QString, QJsonObject>& nodeOutputsByNodeId();
    const QHash<QString, QJsonObject>& nodeOutputsByNodeId() const;
    void clearNodeOutputs();

    QString& selectedNodeId();
    const QString& selectedNodeId() const;
    void clearSelection();

    QHash<QString, QHash<QString, QString>>& nodeStatusesByWorkflowId();
    const QHash<QString, QHash<QString, QString>>& nodeStatusesByWorkflowId() const;
    void clearNodeStatusesForWorkflow(const QString& workflowId);
    void cacheNodeStatus(const QString& workflowId, const QString& nodeId, const QString& status);

    QHash<QString, QString>& workflowIdByRunId();
    const QHash<QString, QString>& workflowIdByRunId() const;
    void rememberRunWorkflow(const QString& runId, const QString& workflowId);
    QString workflowIdForRun(const QString& runId) const;

    QSet<QString>& runningWorkflowIds();
    const QSet<QString>& runningWorkflowIds() const;
    void setWorkflowRunning(const QString& workflowId, bool running);

    QString& activeRunWorkflowId();
    const QString& activeRunWorkflowId() const;
    void setActiveRunWorkflowId(const QString& workflowId);
    void clearActiveRunWorkflowId();

    void resetForWorkspaceChange();
    void resetForWorkflowChange();

private:
    AppState m_state;
};

} // namespace vws::presentation
