#include "presentation/controllers/NodeTemplateController.h"

#include "application/NodeTemplateService.h"
#include "presentation/state/AppStore.h"

namespace vws::presentation {

NodeTemplateController::NodeTemplateController(
    application::NodeTemplateService& nodeTemplateService,
    AppStore& store)
    : m_nodeTemplateService(nodeTemplateService)
    , m_store(store)
{
}

bool NodeTemplateController::saveTemplateFromNode(
    const domain::Node& node,
    const QString& templateName,
    domain::NodeTemplate* savedTemplate,
    QString* errorMessage)
{
    const auto nodeTemplate = m_nodeTemplateService.createTemplateFromNode(
        m_store.currentWorkspace().id,
        node,
        templateName);

    if (!m_nodeTemplateService.saveTemplate(m_store.currentWorkspace().rootPath, nodeTemplate, errorMessage)) {
        return false;
    }

    if (savedTemplate != nullptr) {
        *savedTemplate = nodeTemplate;
    }
    return true;
}

bool NodeTemplateController::createNodeFromTemplateFile(
    const QString& filePath,
    domain::Node& node,
    domain::NodeTemplate* sourceTemplate,
    QString* errorMessage)
{
    domain::NodeTemplate nodeTemplate;
    if (!m_nodeTemplateService.loadTemplate(filePath, nodeTemplate, errorMessage)) {
        return false;
    }

    node = m_nodeTemplateService.createNodeFromTemplate(nodeTemplate);
    if (sourceTemplate != nullptr) {
        *sourceTemplate = nodeTemplate;
    }
    return true;
}

bool NodeTemplateController::importTemplateFile(
    const QString& filePath,
    domain::NodeTemplate* importedTemplate,
    QString* errorMessage)
{
    domain::NodeTemplate nodeTemplate;
    if (!m_nodeTemplateService.importTemplateFile(
            filePath,
            m_store.currentWorkspace().rootPath,
            nodeTemplate,
            errorMessage)) {
        return false;
    }

    if (importedTemplate != nullptr) {
        *importedTemplate = nodeTemplate;
    }
    return true;
}

QList<domain::NodeTemplate> NodeTemplateController::listTemplates(QString* errorMessage) const
{
    return m_nodeTemplateService.listTemplates(m_store.currentWorkspace().rootPath, errorMessage);
}

bool NodeTemplateController::createNodeFromWorkspaceTemplate(
    const QString& templateId,
    domain::Node& node,
    domain::NodeTemplate* sourceTemplate,
    QString* errorMessage)
{
    domain::NodeTemplate nodeTemplate;
    if (!m_nodeTemplateService.loadTemplateFromWorkspace(
            m_store.currentWorkspace().rootPath,
            templateId,
            nodeTemplate,
            errorMessage)) {
        return false;
    }

    node = m_nodeTemplateService.createNodeFromTemplate(nodeTemplate);
    if (sourceTemplate != nullptr) {
        *sourceTemplate = nodeTemplate;
    }
    return true;
}

} // namespace vws::presentation
