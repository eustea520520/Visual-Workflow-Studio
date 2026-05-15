#include "presentation/models/WorkflowDisplayModel.h"

namespace vws::presentation {

WorkflowDisplayModel WorkflowDisplayModelBuilder::build(const domain::Workflow& workflow)
{
    WorkflowDisplayModel model;
    model.workflowName = workflow.name;
    for (const auto& node : workflow.nodes) {
        model.nodeNamesById.insert(node.nodeId, node.name);
    }
    return model;
}

} // namespace vws::presentation
