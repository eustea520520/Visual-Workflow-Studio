#pragma once

#include <QString>

namespace vws::application {
class WorkspaceService;
}

namespace vws::presentation {

class AppStore;

// Owns workspace-level use cases for the presentation layer. Dialogs still live
// in MainWindow, but state mutation and service calls are kept out of widgets.
class WorkspaceController {
public:
    WorkspaceController(application::WorkspaceService& workspaceService, AppStore& store);

    bool createWorkspace(const QString& rootPath, const QString& name, QString* errorMessage = nullptr);
    bool openWorkspace(const QString& rootPath, QString* errorMessage = nullptr);

private:
    application::WorkspaceService& m_workspaceService;
    AppStore& m_store;
};

} // namespace vws::presentation
