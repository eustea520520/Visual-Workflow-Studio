#pragma once

#include "domain/Workflow.h"

#include <QJsonObject>
#include <QList>
#include <QString>

namespace vws::application {

// WorkflowService owns workflow use cases: create, load, save, list, and edit
// node code while preserving the existing workflow JSON shape.
class WorkflowService {
public:
    WorkflowService();

    domain::Workflow createEmptyWorkflow(const QString& workspaceId, const QString& name) const;

    bool loadWorkflow(const QString& filePath, domain::Workflow& workflow, QString* errorMessage = nullptr) const;
    bool saveWorkflow(const QString& filePath, const domain::Workflow& workflow, QString* errorMessage = nullptr) const;
    bool updateNodeCode(domain::Workflow& workflow, const QString& nodeId, const QString& code, QString* errorMessage = nullptr) const;
    bool updateNodeDetails(
        domain::Workflow& workflow,
        const QString& nodeId,
        const QString& name,
        const QString& description,
        const QString& code,
        QString* errorMessage = nullptr) const;
    bool updateNodeDetails(
        domain::Workflow& workflow,
        const QString& nodeId,
        const QString& name,
        const QString& description,
        int timeoutMs,
        const QString& code,
        const QJsonObject& configPatch,
        QString* errorMessage = nullptr) const;

    bool updateNodeDetails(
        domain::Workflow& workflow,
        const QString& nodeId,
        const QString& name,
        const QString& description,
        const QString& code,
        const QJsonObject& configPatch,
        QString* errorMessage = nullptr) const;

    QString workflowPath(const QString& workspaceRootPath, const domain::Workflow& workflow) const;
    bool saveWorkflowToWorkspace(const QString& workspaceRootPath, const domain::Workflow& workflow, QString* errorMessage = nullptr) const;
    bool loadWorkflowFromWorkspace(const QString& workspaceRootPath, const QString& workflowId, domain::Workflow& workflow, QString* errorMessage = nullptr) const;
    QList<domain::Workflow> listWorkflows(const QString& workspaceRootPath, QString* errorMessage = nullptr) const;
};

} // namespace vws::application
