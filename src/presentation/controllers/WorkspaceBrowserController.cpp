#include "presentation/controllers/WorkspaceBrowserController.h"

#include "application/NodeTemplateService.h"
#include "application/RunService.h"
#include "application/WorkflowService.h"
#include "presentation/state/AppStore.h"

namespace vws::presentation {

WorkspaceBrowserController::WorkspaceBrowserController(
    application::WorkflowService& workflowService,
    application::NodeTemplateService& nodeTemplateService,
    application::RunService& runService,
    AppStore& store)
    : m_workflowService(workflowService)
    , m_nodeTemplateService(nodeTemplateService)
    , m_runService(runService)
    , m_store(store)
{
}

WorkspaceBrowserSnapshot WorkspaceBrowserController::snapshot() const
{
    WorkspaceBrowserSnapshot snapshot;
    const auto& workspace = m_store.currentWorkspace();
    if (workspace.rootPath.trimmed().isEmpty()) {
        return snapshot;
    }

    snapshot.workspaceName = workspace.name;
    snapshot.runningWorkflowIds = m_store.runningWorkflowIds();

    QString errorMessage;
    const auto workflows = m_workflowService.listWorkflows(workspace.rootPath, &errorMessage);
    if (!errorMessage.isEmpty()) {
        snapshot.errors.append(errorMessage);
    }
    for (const auto& workflow : workflows) {
        snapshot.workflowNames.append(workflow.name.isEmpty() ? workflow.workflowId : workflow.name);
        snapshot.workflowIds.append(workflow.workflowId);
    }

    errorMessage.clear();
    const auto templates = m_nodeTemplateService.listTemplates(workspace.rootPath, &errorMessage);
    if (!errorMessage.isEmpty()) {
        snapshot.errors.append(errorMessage);
    }
    for (const auto& nodeTemplate : templates) {
        snapshot.templateNames.append(nodeTemplate.name.isEmpty() ? nodeTemplate.templateId : nodeTemplate.name);
        snapshot.templateIds.append(nodeTemplate.templateId);
    }

    const auto runEntries = m_runService.recentRunEntries(workspace.rootPath);
    for (const auto& run : runEntries) {
        snapshot.runNames.append(run.displayName);
        snapshot.runIds.append(run.runId);
    }

    return snapshot;
}

} // namespace vws::presentation
