#include "domain/Workspace.h"

namespace vws::domain {

QJsonObject Workspace::toJson() const
{
    return {
        {"id", id},
        {"name", name},
        {"root_path", rootPath},
        {"created_at", createdAt},
        {"updated_at", updatedAt},
        {"config", config},
    };
}

Workspace Workspace::fromJson(const QJsonObject& object)
{
    Workspace workspace;
    workspace.id = object.value("id").toString();
    workspace.name = object.value("name").toString();
    workspace.rootPath = object.value("root_path").toString();
    workspace.createdAt = object.value("created_at").toString();
    workspace.updatedAt = object.value("updated_at").toString();
    workspace.config = object.value("config").toObject();
    return workspace;
}

} // namespace vws::domain
