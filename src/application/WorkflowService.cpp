#include "application/WorkflowService.h"

#include "application/io/NodeIoSpecUtils.h"
#include "application/io/PythonIoDimensionAnalyzer.h"
#include "domain/NodeConfigKeys.h"
#include "domain/NodeConfigView.h"
#include "domain/WorkflowJsonParser.h"
#include "domain/WorkflowSchema.h"
#include "infrastructure/FileSystemUtils.h"
#include "infrastructure/JsonUtils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QUuid>

namespace vws::application {

namespace ConfigKeys = domain::NodeConfigKeys;

WorkflowService::WorkflowService() = default;

domain::Workflow WorkflowService::createEmptyWorkflow(const QString& workspaceId, const QString& name) const
{
    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    domain::Workflow workflow;
    workflow.schemaVersion = domain::CurrentWorkflowSchemaVersion;
    workflow.workflowId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    workflow.workspaceId = workspaceId;
    workflow.name = name;
    workflow.createdAt = now;
    workflow.updatedAt = now;
    workflow.version = 1;
    return workflow;
}

bool WorkflowService::loadWorkflow(const QString& filePath, domain::Workflow& workflow, QString* errorMessage) const
{
    QJsonObject object;
    if (!infrastructure::JsonUtils::readObjectFromFile(filePath, object, errorMessage)) {
        return false;
    }

    const auto parseResult = domain::WorkflowJsonParser::parseStrict(object);
    if (!parseResult.success) {
        if (errorMessage != nullptr) {
            *errorMessage = parseResult.errors.join(QStringLiteral("\n"));
        }
        return false;
    }

    workflow = parseResult.workflow;
    return true;
}

bool WorkflowService::saveWorkflow(const QString& filePath, const domain::Workflow& workflow, QString* errorMessage) const
{
    return infrastructure::JsonUtils::writeObjectToFile(filePath, workflow.toJson(), errorMessage);
}

bool WorkflowService::updateNodeCode(domain::Workflow& workflow, const QString& nodeId, const QString& code, QString* errorMessage) const
{
    for (const auto& node : workflow.nodes) {
        if (node.nodeId == nodeId) {
            return updateNodeDetails(workflow, nodeId, node.name, node.description, code, errorMessage);
        }
    }

    if (errorMessage != nullptr) {
        *errorMessage = QString("Node not found: %1").arg(nodeId);
    }
    return false;
}

bool WorkflowService::updateNodeDetails(
    domain::Workflow& workflow,
    const QString& nodeId,
    const QString& name,
    const QString& description,
    const QString& code,
    QString* errorMessage) const
{
    return updateNodeDetails(workflow, nodeId, name, description, code, {}, errorMessage);
}

bool WorkflowService::updateNodeDetails(
    domain::Workflow& workflow,
    const QString& nodeId,
    const QString& name,
    const QString& description,
    int timeoutMs,
    const QString& code,
    const QJsonObject& configPatch,
    QString* errorMessage) const
{
    if (timeoutMs <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Node timeout must be greater than 0 ms.");
        }
        return false;
    }

    for (auto& node : workflow.nodes) {
        if (node.nodeId != nodeId) {
            continue;
        }

        node.name = name.trimmed().isEmpty() ? node.name : name.trimmed();
        node.description = description;
        node.runtime.timeoutMs = timeoutMs;
        const domain::NodeConfigView config(node.config);
        const auto language = config.language();
        const auto entry = config.entry();
        node.config.insert(ConfigKeys::Language, language);
        node.config.insert(ConfigKeys::Entry, entry);
        node.config.insert(ConfigKeys::Code, code);
        for (auto it = configPatch.constBegin(); it != configPatch.constEnd(); ++it) {
            node.config.insert(it.key(), it.value());
        }
        const auto analyzedSpec = PythonIoDimensionAnalyzer().analyze(node);
        node.ioSpec = NodeIoSpecUtils::merged(NodeIoSpecUtils::defaultSpecForNode(node), analyzedSpec);
        workflow.updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QString("Node not found: %1").arg(nodeId);
    }
    return false;
}

bool WorkflowService::updateNodeDetails(
    domain::Workflow& workflow,
    const QString& nodeId,
    const QString& name,
    const QString& description,
    const QString& code,
    const QJsonObject& configPatch,
    QString* errorMessage) const
{
    for (auto& node : workflow.nodes) {
        if (node.nodeId != nodeId) {
            continue;
        }

        node.name = name.trimmed().isEmpty() ? node.name : name.trimmed();
        node.description = description;
        const domain::NodeConfigView config(node.config);
        const auto language = config.language();
        const auto entry = config.entry();
        node.config.insert(ConfigKeys::Language, language);
        node.config.insert(ConfigKeys::Entry, entry);
        node.config.insert(ConfigKeys::Code, code);
        for (auto it = configPatch.constBegin(); it != configPatch.constEnd(); ++it) {
            node.config.insert(it.key(), it.value());
        }
        const auto analyzedSpec = PythonIoDimensionAnalyzer().analyze(node);
        node.ioSpec = NodeIoSpecUtils::merged(NodeIoSpecUtils::defaultSpecForNode(node), analyzedSpec);
        workflow.updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QString("Node not found: %1").arg(nodeId);
    }
    return false;
}

QString WorkflowService::workflowPath(const QString& workspaceRootPath, const domain::Workflow& workflow) const
{
    const auto fileStem = workflow.workflowId.isEmpty()
        ? infrastructure::FileSystemUtils::safeFileStem(workflow.name, "workflow")
        : workflow.workflowId;
    return QDir(workspaceRootPath).filePath(QString("workflows/%1.json").arg(fileStem));
}

bool WorkflowService::saveWorkflowToWorkspace(const QString& workspaceRootPath, const domain::Workflow& workflow, QString* errorMessage) const
{
    const auto workflowsPath = QDir(workspaceRootPath).filePath("workflows");
    if (!infrastructure::FileSystemUtils::ensureDirectory(workflowsPath, errorMessage)) {
        return false;
    }

    return saveWorkflow(workflowPath(workspaceRootPath, workflow), workflow, errorMessage);
}

bool WorkflowService::deleteWorkflowFromWorkspace(const QString& workspaceRootPath, const QString& workflowId, QString* errorMessage) const
{
    if (workspaceRootPath.trimmed().isEmpty() || workflowId.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Workspace path or workflow id is empty.");
        }
        return false;
    }

    const auto filePath = QDir(workspaceRootPath).filePath(QString("workflows/%1.json").arg(workflowId));
    if (!QFileInfo::exists(filePath)) {
        return true;
    }

    if (!QFile::remove(filePath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not delete workflow file: %1").arg(filePath);
        }
        return false;
    }

    return true;
}

bool WorkflowService::loadWorkflowFromWorkspace(const QString& workspaceRootPath, const QString& workflowId, domain::Workflow& workflow, QString* errorMessage) const
{
    return loadWorkflow(QDir(workspaceRootPath).filePath(QString("workflows/%1.json").arg(workflowId)), workflow, errorMessage);
}

QList<domain::Workflow> WorkflowService::listWorkflows(const QString& workspaceRootPath, QString* errorMessage) const
{
    QList<domain::Workflow> workflows;
    const auto files = infrastructure::FileSystemUtils::listFiles(
        QDir(workspaceRootPath).filePath("workflows"),
        {"*.json"});

    for (const auto& file : files) {
        domain::Workflow workflow;
        if (!loadWorkflow(file, workflow, errorMessage)) {
            return {};
        }
        workflows.append(workflow);
    }

    return workflows;
}

} // namespace vws::application
