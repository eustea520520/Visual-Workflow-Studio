#pragma once

#include <QString>

#include <functional>

namespace vws::application {
class WorkspaceService;
}

namespace vws::presentation {

class AppStore;

class PythonEnvironmentController {
public:
    using ApplyPythonExecutable = std::function<void(const QString&)>;

    PythonEnvironmentController(
        application::WorkspaceService& workspaceService,
        AppStore& store,
        ApplyPythonExecutable applyPythonExecutable);

    bool updatePythonExecutable(const QString& pythonExecutable, QString* errorMessage = nullptr);
    QString pythonExecutable() const;
    void applyCurrentWorkspacePythonExecutable() const;

private:
    application::WorkspaceService& m_workspaceService;
    AppStore& m_store;
    ApplyPythonExecutable m_applyPythonExecutable;
};

} // namespace vws::presentation
