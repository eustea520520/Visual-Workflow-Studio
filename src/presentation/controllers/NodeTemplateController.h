#pragma once

#include "domain/Node.h"
#include "domain/NodeTemplate.h"

#include <QList>
#include <QString>

namespace vws::application {
class NodeTemplateService;
}

namespace vws::presentation {

class AppStore;

// NodeTemplateController coordinates template operations with the current workspace.
// UI code still chooses files and positions nodes; this controller owns the application-service calls.
class NodeTemplateController {
public:
    NodeTemplateController(application::NodeTemplateService& nodeTemplateService, AppStore& store);

    bool saveTemplateFromNode(
        const domain::Node& node,
        const QString& templateName,
        domain::NodeTemplate* savedTemplate = nullptr,
        QString* errorMessage = nullptr);
    bool createNodeFromTemplateFile(
        const QString& filePath,
        domain::Node& node,
        domain::NodeTemplate* sourceTemplate = nullptr,
        QString* errorMessage = nullptr);
    bool importTemplateFile(
        const QString& filePath,
        domain::NodeTemplate* importedTemplate = nullptr,
        QString* errorMessage = nullptr);
    QList<domain::NodeTemplate> listTemplates(QString* errorMessage = nullptr) const;
    bool createNodeFromWorkspaceTemplate(
        const QString& templateId,
        domain::Node& node,
        domain::NodeTemplate* sourceTemplate = nullptr,
        QString* errorMessage = nullptr);

private:
    application::NodeTemplateService& m_nodeTemplateService;
    AppStore& m_store;
};

} // namespace vws::presentation
