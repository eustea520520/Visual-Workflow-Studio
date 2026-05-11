#pragma once

#include "domain/Workspace.h"

#include <QString>
#include <QStringList>

namespace vws::application {

// WorkspaceService creates and opens workspace folders while preserving the
// existing workspace.json and directory layout.
class WorkspaceService {
public:
    WorkspaceService();

    domain::Workspace activeWorkspace() const;

    bool createWorkspace(const QString& rootPath, const QString& name, domain::Workspace& workspace, QString* errorMessage = nullptr);
    bool openWorkspace(const QString& rootPath, domain::Workspace& workspace, QString* errorMessage = nullptr);
    bool saveWorkspace(const domain::Workspace& workspace, QString* errorMessage = nullptr);
    bool updatePythonExecutable(domain::Workspace& workspace, const QString& pythonExecutable, QString* errorMessage = nullptr);
    QString pythonExecutable(const domain::Workspace& workspace) const;
    QStringList workspaceDirectories(const QString& rootPath) const;

private:
    bool ensureWorkspaceDirectories(const QString& rootPath, QString* errorMessage) const;

    domain::Workspace m_activeWorkspace;
};

} // namespace vws::application
