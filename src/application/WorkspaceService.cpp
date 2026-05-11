#include "application/WorkspaceService.h"

#include "infrastructure/FileSystemUtils.h"
#include "infrastructure/JsonUtils.h"

#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QUuid>

namespace vws::application {

namespace {

constexpr auto PythonExecutableKey = "python_executable";

} // namespace

WorkspaceService::WorkspaceService() = default;

domain::Workspace WorkspaceService::activeWorkspace() const
{
    return m_activeWorkspace;
}

bool WorkspaceService::createWorkspace(const QString& rootPath, const QString& name, domain::Workspace& workspace, QString* errorMessage)
{
    if (!ensureWorkspaceDirectories(rootPath, errorMessage)) {
        return false;
    }

    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    workspace.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    workspace.name = name.trimmed().isEmpty() ? "Untitled Workspace" : name.trimmed();
    workspace.rootPath = QDir(rootPath).absolutePath();
    workspace.createdAt = now;
    workspace.updatedAt = now;
    workspace.config = {};

    if (!saveWorkspace(workspace, errorMessage)) {
        return false;
    }

    m_activeWorkspace = workspace;
    return true;
}

bool WorkspaceService::openWorkspace(const QString& rootPath, domain::Workspace& workspace, QString* errorMessage)
{
    const auto workspaceFilePath = QDir(rootPath).filePath("workspace.json");
    QJsonObject object;
    if (!infrastructure::JsonUtils::readObjectFromFile(workspaceFilePath, object, errorMessage)) {
        return false;
    }

    workspace = domain::Workspace::fromJson(object);
    if (workspace.rootPath.isEmpty()) {
        workspace.rootPath = QDir(rootPath).absolutePath();
    }

    if (!ensureWorkspaceDirectories(workspace.rootPath, errorMessage)) {
        return false;
    }

    m_activeWorkspace = workspace;
    return true;
}

bool WorkspaceService::saveWorkspace(const domain::Workspace& workspace, QString* errorMessage)
{
    if (workspace.rootPath.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Workspace root path is empty.";
        }
        return false;
    }

    return infrastructure::JsonUtils::writeObjectToFile(
        QDir(workspace.rootPath).filePath("workspace.json"),
        workspace.toJson(),
        errorMessage);
}

bool WorkspaceService::updatePythonExecutable(domain::Workspace& workspace, const QString& pythonExecutable, QString* errorMessage)
{
    workspace.config.insert(PythonExecutableKey, pythonExecutable.trimmed());
    workspace.updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!saveWorkspace(workspace, errorMessage)) {
        return false;
    }

    m_activeWorkspace = workspace;
    return true;
}

QString WorkspaceService::pythonExecutable(const domain::Workspace& workspace) const
{
    return workspace.config.value(PythonExecutableKey).toString();
}

QStringList WorkspaceService::workspaceDirectories(const QString& rootPath) const
{
    const QDir root(rootPath);
    return {
        root.filePath("workflows"),
        root.filePath("node_templates"),
        root.filePath("runs"),
        root.filePath("artifacts"),
        root.filePath("secrets"),
        root.filePath("cache"),
        root.filePath("logs"),
    };
}

bool WorkspaceService::ensureWorkspaceDirectories(const QString& rootPath, QString* errorMessage) const
{
    if (!infrastructure::FileSystemUtils::ensureDirectory(rootPath, errorMessage)) {
        return false;
    }

    for (const auto& directory : workspaceDirectories(rootPath)) {
        if (!infrastructure::FileSystemUtils::ensureDirectory(directory, errorMessage)) {
            return false;
        }
    }
    return true;
}

} // namespace vws::application
