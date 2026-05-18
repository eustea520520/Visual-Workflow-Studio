#pragma once

#include "application/generation/WorkflowGenerationTypes.h"
#include "domain/Workspace.h"

namespace vws::application {

class WorkflowGenerationTemplateCatalog;

class WorkflowGenerationAssembler final {
public:
    bool assemble(
        const WorkflowSkeleton& skeleton,
        const QHash<QString, NodeImplementation>& implementationsByNodeId,
        const WorkflowGenerationTemplateCatalog& catalog,
        const domain::Workspace& workspace,
        domain::Workflow& workflow,
        QStringList& errors) const;
};

} // namespace vws::application
