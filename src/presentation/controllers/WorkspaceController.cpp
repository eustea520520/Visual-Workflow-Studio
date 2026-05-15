#include "presentation/controllers/WorkspaceController.h"

#include "application/WorkspaceService.h"
#include "presentation/state/AppStore.h"

namespace vws::presentation {

WorkspaceController::WorkspaceController(application::WorkspaceService& workspaceService, AppStore& store)
    : m_workspaceService(workspaceService)
    , m_store(store)
{
}

bool WorkspaceController::createWorkspace(const QString& rootPath, const QString& name, QString* errorMessage)
{
    domain::Workspace workspace;
    if (!m_workspaceService.createWorkspace(rootPath, name, workspace, errorMessage)) {
        return false;
    }

    m_store.setCurrentWorkspace(workspace);
    m_store.resetForWorkspaceChange();
    return true;
}

bool WorkspaceController::openWorkspace(const QString& rootPath, QString* errorMessage)
{
    domain::Workspace workspace;
    if (!m_workspaceService.openWorkspace(rootPath, workspace, errorMessage)) {
        return false;
    }

    m_store.setCurrentWorkspace(workspace);
    m_store.resetForWorkspaceChange();
    return true;
}

} // namespace vws::presentation
