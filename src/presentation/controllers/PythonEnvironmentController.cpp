#include "presentation/controllers/PythonEnvironmentController.h"

#include "application/WorkspaceService.h"
#include "presentation/state/AppStore.h"

#include <utility>

namespace vws::presentation {

PythonEnvironmentController::PythonEnvironmentController(
    application::WorkspaceService& workspaceService,
    AppStore& store,
    ApplyPythonExecutable applyPythonExecutable)
    : m_workspaceService(workspaceService)
    , m_store(store)
    , m_applyPythonExecutable(std::move(applyPythonExecutable))
{
}

bool PythonEnvironmentController::updatePythonExecutable(const QString& pythonExecutable, QString* errorMessage)
{
    if (!m_workspaceService.updatePythonExecutable(
            m_store.currentWorkspace(),
            pythonExecutable,
            errorMessage)) {
        return false;
    }

    applyCurrentWorkspacePythonExecutable();
    return true;
}

QString PythonEnvironmentController::pythonExecutable() const
{
    return m_workspaceService.pythonExecutable(m_store.currentWorkspace());
}

void PythonEnvironmentController::applyCurrentWorkspacePythonExecutable() const
{
    if (m_applyPythonExecutable) {
        m_applyPythonExecutable(pythonExecutable().trimmed());
    }
}

} // namespace vws::presentation
