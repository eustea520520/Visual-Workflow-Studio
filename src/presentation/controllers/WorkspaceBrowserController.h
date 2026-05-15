#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

namespace vws::application {
class NodeTemplateService;
class RunService;
class WorkflowService;
}

namespace vws::presentation {

class AppStore;

struct WorkspaceBrowserSnapshot {
    QString workspaceName;
    QStringList workflowNames;
    QStringList workflowIds;
    QStringList templateNames;
    QStringList templateIds;
    QStringList runNames;
    QStringList runIds;
    QSet<QString> runningWorkflowIds;
    QStringList errors;
};

class WorkspaceBrowserController {
public:
    WorkspaceBrowserController(
        application::WorkflowService& workflowService,
        application::NodeTemplateService& nodeTemplateService,
        application::RunService& runService,
        AppStore& store);

    WorkspaceBrowserSnapshot snapshot() const;

private:
    application::WorkflowService& m_workflowService;
    application::NodeTemplateService& m_nodeTemplateService;
    application::RunService& m_runService;
    AppStore& m_store;
};

} // namespace vws::presentation
