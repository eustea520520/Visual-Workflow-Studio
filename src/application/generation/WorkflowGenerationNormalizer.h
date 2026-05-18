#pragma once

#include "domain/Workspace.h"
#include "domain/Workflow.h"

namespace vws::application {

class WorkflowAutoLayout;

class WorkflowGenerationNormalizer final {
public:
    explicit WorkflowGenerationNormalizer(WorkflowAutoLayout& autoLayout);

    domain::Workflow normalize(const domain::Workflow& workflow, const domain::Workspace& workspace) const;

private:
    WorkflowAutoLayout& m_autoLayout;
};

} // namespace vws::application
