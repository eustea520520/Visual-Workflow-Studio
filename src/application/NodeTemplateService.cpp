#include "application/NodeTemplateService.h"

#include "domain/Workspace.h"
#include "infrastructure/FileSystemUtils.h"
#include "infrastructure/JsonUtils.h"

#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QJsonValue>
#include <QUuid>

namespace vws::application {

namespace {

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool isNonEmptyString(const QJsonObject& object, const QString& key)
{
    return object.value(key).isString() && !object.value(key).toString().trimmed().isEmpty();
}

bool validateNodeTemplateObject(const QJsonObject& object, QString* errorMessage)
{
    if (object.contains("workflow_id") || object.contains("nodes") || object.contains("edges")) {
        setError(errorMessage, "Selected JSON is a Workflow file, not a Node Template file.");
        return false;
    }
    if (object.contains("node_id")) {
        setError(errorMessage, "Selected JSON is a Node instance file, not a Node Template file.");
        return false;
    }
    if (!isNonEmptyString(object, "template_id")) {
        setError(errorMessage, "Node Template JSON must contain a non-empty string field: template_id.");
        return false;
    }
    if (!isNonEmptyString(object, "name")) {
        setError(errorMessage, "Node Template JSON must contain a non-empty string field: name.");
        return false;
    }
    if (!isNonEmptyString(object, "type")) {
        setError(errorMessage, "Node Template JSON must contain a non-empty string field: type.");
        return false;
    }
    if (!object.value("input_ports").isArray()) {
        setError(errorMessage, "Node Template JSON must contain an array field: input_ports.");
        return false;
    }
    if (!object.value("output_ports").isArray()) {
        setError(errorMessage, "Node Template JSON must contain an array field: output_ports.");
        return false;
    }
    if (!object.value("config").isObject()) {
        setError(errorMessage, "Node Template JSON must contain an object field: config.");
        return false;
    }
    return true;
}

} // namespace

NodeTemplateService::NodeTemplateService() = default;

QStringList NodeTemplateService::templateNames() const
{
    return {};
}

domain::NodeTemplate NodeTemplateService::createTemplateFromNode(const QString& workspaceId, const domain::Node& node, const QString& name) const
{
    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    domain::NodeTemplate nodeTemplate;
    nodeTemplate.schemaVersion = 1;
    nodeTemplate.templateId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    nodeTemplate.workspaceId = workspaceId;
    nodeTemplate.name = name.trimmed().isEmpty() ? node.name : name.trimmed();
    nodeTemplate.description = node.description;
    nodeTemplate.type = node.type;
    nodeTemplate.inputPorts = node.inputPorts;
    nodeTemplate.outputPorts = node.outputPorts;
    nodeTemplate.config = node.config;
    nodeTemplate.createdAt = now;
    nodeTemplate.updatedAt = now;
    nodeTemplate.version = 1;
    return nodeTemplate;
}

domain::Node NodeTemplateService::createNodeFromTemplate(const domain::NodeTemplate& nodeTemplate, const QString& nodeName) const
{
    domain::Node node;
    node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    node.templateId = nodeTemplate.templateId;
    node.type = nodeTemplate.type;
    node.name = nodeName.trimmed().isEmpty() ? nodeTemplate.name : nodeName.trimmed();
    node.description = nodeTemplate.description;
    node.inputPorts = nodeTemplate.inputPorts;
    node.outputPorts = nodeTemplate.outputPorts;
    node.config = nodeTemplate.config;
    return node;
}

QString NodeTemplateService::templatePath(const QString& workspaceRootPath, const domain::NodeTemplate& nodeTemplate) const
{
    const auto fileStem = nodeTemplate.templateId.isEmpty()
        ? infrastructure::FileSystemUtils::safeFileStem(nodeTemplate.name, "node_template")
        : nodeTemplate.templateId;
    return QDir(workspaceRootPath).filePath(QString("node_templates/%1.json").arg(fileStem));
}

bool NodeTemplateService::saveTemplate(const QString& workspaceRootPath, const domain::NodeTemplate& nodeTemplate, QString* errorMessage) const
{
    const auto templateDirectory = QDir(workspaceRootPath).filePath("node_templates");
    if (!infrastructure::FileSystemUtils::ensureDirectory(templateDirectory, errorMessage)) {
        return false;
    }

    return infrastructure::JsonUtils::writeObjectToFile(templatePath(workspaceRootPath, nodeTemplate), nodeTemplate.toJson(), errorMessage);
}

bool NodeTemplateService::loadTemplate(const QString& filePath, domain::NodeTemplate& nodeTemplate, QString* errorMessage) const
{
    QJsonObject object;
    if (!infrastructure::JsonUtils::readObjectFromFile(filePath, object, errorMessage)) {
        return false;
    }

    if (!validateNodeTemplateObject(object, errorMessage)) {
        return false;
    }

    nodeTemplate = domain::NodeTemplate::fromJson(object);
    return true;
}

QList<domain::NodeTemplate> NodeTemplateService::listTemplates(const QString& workspaceRootPath, QString* errorMessage) const
{
    QList<domain::NodeTemplate> templates;
    QStringList warnings;
    const auto files = infrastructure::FileSystemUtils::listFiles(
        QDir(workspaceRootPath).filePath("node_templates"),
        {"*.json"});

    for (const auto& file : files) {
        domain::NodeTemplate nodeTemplate;
        QString loadError;
        if (!loadTemplate(file, nodeTemplate, &loadError)) {
            warnings.append(QString("%1: %2").arg(QDir::toNativeSeparators(file), loadError));
            continue;
        }
        templates.append(nodeTemplate);
    }

    if (errorMessage != nullptr) {
        *errorMessage = warnings.join('\n');
    }
    return templates;
}

bool NodeTemplateService::importTemplateFile(const QString& sourceFilePath, const QString& targetWorkspaceRootPath, domain::NodeTemplate& importedTemplate, QString* errorMessage) const
{
    if (!loadTemplate(sourceFilePath, importedTemplate, errorMessage)) {
        return false;
    }

    QJsonObject workspaceObject;
    if (infrastructure::JsonUtils::readObjectFromFile(QDir(targetWorkspaceRootPath).filePath("workspace.json"), workspaceObject, nullptr)) {
        importedTemplate.workspaceId = domain::Workspace::fromJson(workspaceObject).id;
    }
    importedTemplate.templateId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    importedTemplate.updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return saveTemplate(targetWorkspaceRootPath, importedTemplate, errorMessage);
}

} // namespace vws::application
