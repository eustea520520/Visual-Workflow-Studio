#pragma once

#include "domain/NodeIoSpec.h"
#include "domain/Workflow.h"

#include <QHash>

namespace vws::presentation {

class WorkflowIoController final {
public:
    WorkflowIoController();

    domain::NodeIoSpec visualSpecForNode(const domain::Workflow& workflow, const QString& nodeId) const;
    QHash<QString, domain::NodeIoSpec> visualSpecsForWorkflow(const domain::Workflow& workflow) const;

};

} // namespace vws::presentation
