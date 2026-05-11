#include "domain/NodeTemplate.h"

#include <QJsonArray>

namespace vws::domain {

namespace {

QJsonArray stringListToJson(const QStringList& values)
{
    QJsonArray array;
    for (const auto& value : values) {
        array.append(value);
    }
    return array;
}

QStringList stringListFromJson(const QJsonArray& array)
{
    QStringList values;
    for (const auto& value : array) {
        values.append(value.toString());
    }
    return values;
}

} // namespace

QJsonObject NodeTemplate::toJson() const
{
    return {
        {"schema_version", schemaVersion},
        {"template_id", templateId},
        {"workspace_id", workspaceId},
        {"name", name},
        {"description", description},
        {"type", type},
        {"input_ports", stringListToJson(inputPorts)},
        {"output_ports", stringListToJson(outputPorts)},
        {"config", config},
        {"created_at", createdAt},
        {"updated_at", updatedAt},
        {"version", version},
    };
}

NodeTemplate NodeTemplate::fromJson(const QJsonObject& object)
{
    NodeTemplate nodeTemplate;
    nodeTemplate.schemaVersion = object.value("schema_version").toInt(1);
    nodeTemplate.templateId = object.value("template_id").toString();
    nodeTemplate.workspaceId = object.value("workspace_id").toString();
    nodeTemplate.name = object.value("name").toString();
    nodeTemplate.description = object.value("description").toString();
    nodeTemplate.type = object.value("type").toString();
    nodeTemplate.inputPorts = stringListFromJson(object.value("input_ports").toArray());
    nodeTemplate.outputPorts = stringListFromJson(object.value("output_ports").toArray());
    nodeTemplate.config = object.value("config").toObject();
    nodeTemplate.createdAt = object.value("created_at").toString();
    nodeTemplate.updatedAt = object.value("updated_at").toString();
    nodeTemplate.version = object.value("version").toInt(1);
    return nodeTemplate;
}

} // namespace vws::domain
