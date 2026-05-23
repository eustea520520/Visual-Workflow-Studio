#include "presentation/controllers/CanvasNavigationController.h"

namespace vws::presentation {

namespace {

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

QString workflowLabel(const domain::Workflow& workflow)
{
    return workflow.name.trimmed().isEmpty() ? workflow.workflowId : workflow.name.trimmed();
}

} // namespace

CanvasNavigationController::CanvasNavigationController(application::SubsystemService& subsystemService)
    : m_subsystemService(subsystemService)
{
}

void CanvasNavigationController::reset()
{
    m_stack.clear();
    m_workspace = {};
}

void CanvasNavigationController::setRootWorkflow(
    const domain::Workspace& workspace,
    const domain::Workflow& workflow,
    const application::WorkflowHistory& history)
{
    reset();
    m_workspace = workspace;

    CanvasDocumentContext root;
    root.label = workflowLabel(workflow);
    root.workflow = workflow;
    root.history = history;
    m_stack.append(root);
}

bool CanvasNavigationController::hasRootWorkflow() const
{
    return !m_stack.isEmpty();
}

bool CanvasNavigationController::isInsideSubsystem() const
{
    return m_stack.size() > 1;
}

int CanvasNavigationController::currentDepth() const
{
    return qMax(0, m_stack.size() - 1);
}

const domain::Workflow& CanvasNavigationController::currentWorkflow() const
{
    return m_stack.last().workflow;
}

application::WorkflowHistory CanvasNavigationController::currentHistory() const
{
    return m_stack.isEmpty() ? application::WorkflowHistory{} : m_stack.last().history;
}

domain::Workflow CanvasNavigationController::rootWorkflow() const
{
    return m_stack.isEmpty() ? domain::Workflow{} : m_stack.first().workflow;
}

ui::CanvasBreadcrumbViewModel CanvasNavigationController::breadcrumbViewModel() const
{
    ui::CanvasBreadcrumbViewModel viewModel;
    ui::CanvasBreadcrumbItemViewModel workspaceItem;
    workspaceItem.label = m_workspace.name.trimmed().isEmpty()
        ? QStringLiteral("No workspace")
        : m_workspace.name.trimmed();
    workspaceItem.depth = 0;
    workspaceItem.clickable = false;
    viewModel.items.append(workspaceItem);

    for (int index = 0; index < m_stack.size(); ++index) {
        ui::CanvasBreadcrumbItemViewModel item;
        item.label = m_stack.at(index).label;
        item.depth = index;
        item.clickable = index < m_stack.size() - 1;
        viewModel.items.append(item);
    }
    return viewModel;
}

void CanvasNavigationController::updateCurrentWorkflowFromView(const domain::Workflow& workflow)
{
    if (!m_stack.isEmpty()) {
        m_stack.last().workflow = workflow;
    }
}

void CanvasNavigationController::updateCurrentHistory(const application::WorkflowHistory& history)
{
    if (!m_stack.isEmpty()) {
        m_stack.last().history = history;
    }
}

bool CanvasNavigationController::enterSubsystem(
    const domain::Workflow& currentCanvasWorkflow,
    const application::WorkflowHistory& currentHistory,
    const QString& subsystemNodeId,
    QString* errorMessage)
{
    if (m_stack.isEmpty()) {
        setError(errorMessage, QStringLiteral("No workflow is open."));
        return false;
    }

    updateCurrentWorkflowFromView(currentCanvasWorkflow);
    updateCurrentHistory(currentHistory);

    auto* subsystemNode = findNode(m_stack.last().workflow, subsystemNodeId);
    if (subsystemNode == nullptr) {
        setError(errorMessage, QStringLiteral("Subsystem node not found: %1").arg(subsystemNodeId));
        return false;
    }
    if (!m_subsystemService.isSubsystemNode(*subsystemNode)) {
        setError(errorMessage, QStringLiteral("Selected node is not a subsystem node."));
        return false;
    }

    domain::Workflow childWorkflow;
    if (!m_subsystemService.loadSubsystemWorkflow(*subsystemNode, childWorkflow, errorMessage)) {
        return false;
    }

    CanvasDocumentContext child;
    child.label = m_subsystemService.breadcrumbLabel(*subsystemNode);
    child.parentNodeId = subsystemNode->nodeId;
    child.nodePath = m_stack.last().nodePath;
    child.nodePath.append(subsystemNode->nodeId);
    child.workflow = childWorkflow;
    m_stack.append(child);
    return true;
}

bool CanvasNavigationController::navigateToDepth(
    int depth,
    const domain::Workflow& currentCanvasWorkflow,
    const application::WorkflowHistory& currentHistory,
    QString* errorMessage)
{
    if (m_stack.isEmpty()) {
        setError(errorMessage, QStringLiteral("No workflow is open."));
        return false;
    }
    if (depth < 0 || depth >= m_stack.size()) {
        setError(errorMessage, QStringLiteral("Invalid breadcrumb depth."));
        return false;
    }

    updateCurrentWorkflowFromView(currentCanvasWorkflow);
    updateCurrentHistory(currentHistory);
    if (!propagateChildrenToRoot(errorMessage)) {
        return false;
    }
    while (m_stack.size() > depth + 1) {
        m_stack.removeLast();
    }
    return true;
}

bool CanvasNavigationController::flushCurrentWorkflow(
    const domain::Workflow& currentCanvasWorkflow,
    const application::WorkflowHistory& currentHistory,
    domain::Workflow& rootWorkflow,
    QString* errorMessage)
{
    if (m_stack.isEmpty()) {
        setError(errorMessage, QStringLiteral("No workflow is open."));
        return false;
    }

    updateCurrentWorkflowFromView(currentCanvasWorkflow);
    updateCurrentHistory(currentHistory);
    if (!propagateChildrenToRoot(errorMessage)) {
        return false;
    }
    rootWorkflow = m_stack.first().workflow;
    return true;
}

bool CanvasNavigationController::propagateChildrenToRoot(QString* errorMessage)
{
    for (int index = m_stack.size() - 1; index > 0; --index) {
        auto& child = m_stack[index];
        auto& parent = m_stack[index - 1];
        auto* parentNode = findNode(parent.workflow, child.parentNodeId);
        if (parentNode == nullptr) {
            setError(errorMessage, QStringLiteral("Parent subsystem node not found: %1").arg(child.parentNodeId));
            return false;
        }
        if (!m_subsystemService.saveSubsystemWorkflow(*parentNode, child.workflow, errorMessage)) {
            return false;
        }
    }
    return true;
}

domain::Node* CanvasNavigationController::findNode(domain::Workflow& workflow, const QString& nodeId) const
{
    for (auto& node : workflow.nodes) {
        if (node.nodeId == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

} // namespace vws::presentation
