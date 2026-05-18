#pragma once

#include "application/generation/WorkflowGenerationTypes.h"

#include <optional>

namespace vws::application {

class WorkflowGenerationTemplateCatalog final {
public:
    QList<NodeTemplateDescriptor> descriptors() const;
    std::optional<NodeTemplateFullSpec> fullSpec(const QString& templateId) const;
    bool contains(const QString& templateId) const;

private:
    QList<NodeTemplateFullSpec> fullSpecs() const;
};

} // namespace vws::application
