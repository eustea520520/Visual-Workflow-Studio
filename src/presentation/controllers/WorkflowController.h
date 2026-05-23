#pragma once

#include "domain/RunRecord.h"
#include "domain/Workflow.h"

#include <QList>
#include <QJsonObject>
#include <QString>

namespace vws::application {
class WorkflowService;
}

namespace vws::presentation {

class AppStore;

// Coordinates workflow persistence and the current workflow held by AppStore.
class WorkflowController {
public:
    WorkflowController(application::WorkflowService& workflowService, AppStore& store);

    void createWorkflow(const QString& name);
    QList<domain::Workflow> listWorkflows(QString* errorMessage = nullptr) const;
    bool loadWorkflowFile(const QString& filePath, QString* errorMessage = nullptr);
    bool loadWorkflowFromWorkspace(const QString& workflowId, QString* errorMessage = nullptr);
    bool workflowSnapshot(const QString& workflowId, domain::Workflow& workflow, QString* errorMessage = nullptr) const;
    bool renameWorkflow(const QString& workflowId, const QString& newName, QString* errorMessage = nullptr);
    bool loadWorkflowSnapshotForRun(
        const domain::RunRecord& record,
        domain::Workflow& workflowSnapshot,
        bool* usedCurrentWorkflowFile = nullptr,
        QString* errorMessage = nullptr) const;
    bool saveCurrentWorkflow(QString* errorMessage = nullptr);
    bool replaceCurrentWorkflowAndSave(const domain::Workflow& workflow, QString* errorMessage = nullptr);
    void syncCurrentWorkflowFromView(const domain::Workflow& workflow);
    bool updateNodeDetails(
        const QString& nodeId,
        const QString& name,
        const QString& description,
        int timeoutMs,
        const QString& code,
        const QJsonObject& configPatch,
        QString* errorMessage = nullptr);

private:
    application::WorkflowService& m_workflowService;
    AppStore& m_store;
};

} // namespace vws::presentation
