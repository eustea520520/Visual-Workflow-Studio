#pragma once

#include "domain/Node.h"
#include "domain/NodeTemplate.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace vws::application {

// NodeTemplateService stores reusable node definitions and creates fresh node
// instances from them.
class NodeTemplateService {
public:
    NodeTemplateService();

    QStringList templateNames() const;
    domain::NodeTemplate createTemplateFromNode(const QString& workspaceId, const domain::Node& node, const QString& name) const;
    domain::Node createNodeFromTemplate(const domain::NodeTemplate& nodeTemplate, const QString& nodeName = {}) const;
    QString templatePath(const QString& workspaceRootPath, const domain::NodeTemplate& nodeTemplate) const;
    bool saveTemplate(const QString& workspaceRootPath, const domain::NodeTemplate& nodeTemplate, QString* errorMessage = nullptr) const;
    bool loadTemplate(const QString& filePath, domain::NodeTemplate& nodeTemplate, QString* errorMessage = nullptr) const;
    QList<domain::NodeTemplate> listTemplates(const QString& workspaceRootPath, QString* errorMessage = nullptr) const;
    bool importTemplateFile(const QString& sourceFilePath, const QString& targetWorkspaceRootPath, domain::NodeTemplate& importedTemplate, QString* errorMessage = nullptr) const;
};

} // namespace vws::application
