#include "presentation/controllers/WorkflowIoController.h"

#include "application/io/NodeIoSpecUtils.h"

namespace vws::presentation {

namespace {

const domain::Node* findNode(const domain::Workflow& workflow, const QString& nodeId)
{
    for (const auto& node : workflow.nodes) {
        if (node.nodeId == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

} // namespace

WorkflowIoController::WorkflowIoController()
{
}

domain::NodeIoSpec WorkflowIoController::visualSpecForNode(const domain::Workflow& workflow, const QString& nodeId) const
{
    const auto* targetNode = findNode(workflow, nodeId);
    if (targetNode == nullptr) {
        return {};
    }

    return targetNode->ioSpec.isEmpty()
        ? application::NodeIoSpecUtils::defaultSpecForNode(*targetNode)
        : targetNode->ioSpec;
}

QHash<QString, domain::NodeIoSpec> WorkflowIoController::visualSpecsForWorkflow(const domain::Workflow& workflow) const
{
    QHash<QString, domain::NodeIoSpec> specs;
    for (const auto& node : workflow.nodes) {
        specs.insert(node.nodeId, visualSpecForNode(workflow, node.nodeId));
    }
    return specs;
}

} // namespace vws::presentation
