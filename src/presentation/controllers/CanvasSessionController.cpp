#include "presentation/controllers/CanvasSessionController.h"

#include "presentation/controllers/CanvasNavigationController.h"
#include "presentation/controllers/WorkflowController.h"
#include "presentation/state/AppStore.h"

namespace vws::presentation {

CanvasSessionController::CanvasSessionController(
    AppStore& store,
    WorkflowController& workflowController,
    CanvasNavigationController& navigationController)
    : m_store(store)
    , m_workflowController(workflowController)
    , m_navigationController(navigationController)
{
}

void CanvasSessionController::startRootSession(const application::WorkflowHistory& history)
{
    m_navigationController.setRootWorkflow(
        m_store.currentWorkspace(),
        m_store.currentWorkflow(),
        history);
}

void CanvasSessionController::syncCurrentView(const domain::Workflow& canvasWorkflow)
{
    if (m_navigationController.isInsideSubsystem()) {
        m_navigationController.updateCurrentWorkflowFromView(canvasWorkflow);
        return;
    }

    m_workflowController.syncCurrentWorkflowFromView(canvasWorkflow);
    m_navigationController.updateCurrentWorkflowFromView(canvasWorkflow);
}

bool CanvasSessionController::enterSubsystem(
    const domain::Workflow& canvasWorkflow,
    const application::WorkflowHistory& history,
    const QString& subsystemNodeId,
    QString* errorMessage)
{
    return m_navigationController.enterSubsystem(
        canvasWorkflow,
        history,
        subsystemNodeId,
        errorMessage);
}

bool CanvasSessionController::navigateToDepth(
    int depth,
    const domain::Workflow& canvasWorkflow,
    const application::WorkflowHistory& history,
    QString* errorMessage)
{
    if (!m_navigationController.navigateToDepth(depth, canvasWorkflow, history, errorMessage)) {
        return false;
    }

    if (depth == 0) {
        m_workflowController.syncCurrentWorkflowFromView(m_navigationController.rootWorkflow());
    }
    return true;
}

bool CanvasSessionController::cacheCurrentView(
    const domain::Workflow& canvasWorkflow,
    const application::WorkflowHistory& history,
    QString* errorMessage)
{
    if (canvasWorkflow.workflowId.trimmed().isEmpty()
        || m_store.currentWorkflow().workflowId.trimmed().isEmpty()) {
        return true;
    }

    if (m_navigationController.isInsideSubsystem()) {
        domain::Workflow rootWorkflow;
        if (!m_navigationController.flushCurrentWorkflow(
                canvasWorkflow,
                history,
                rootWorkflow,
                errorMessage)) {
            return false;
        }
        m_workflowController.syncCurrentWorkflowFromView(rootWorkflow);
        return true;
    }

    m_workflowController.syncCurrentWorkflowFromView(canvasWorkflow);
    m_navigationController.updateCurrentWorkflowFromView(canvasWorkflow);
    m_navigationController.updateCurrentHistory(history);
    m_store.cacheWorkflowHistory(m_store.currentWorkflow().workflowId, history);
    return true;
}

bool CanvasSessionController::prepareRootWorkflowFromView(
    const domain::Workflow& canvasWorkflow,
    const application::WorkflowHistory& history,
    domain::Workflow& rootWorkflow,
    QString* errorMessage)
{
    if (!m_navigationController.hasRootWorkflow()) {
        rootWorkflow = canvasWorkflow;
    } else if (!m_navigationController.flushCurrentWorkflow(
            canvasWorkflow,
            history,
            rootWorkflow,
            errorMessage)) {
        return false;
    }

    m_workflowController.syncCurrentWorkflowFromView(rootWorkflow);
    return true;
}

void CanvasSessionController::updateCurrentHistory(const application::WorkflowHistory& history)
{
    m_navigationController.updateCurrentHistory(history);
    if (!m_navigationController.isInsideSubsystem()) {
        m_store.cacheWorkflowHistory(m_store.currentWorkflow().workflowId, history);
    }
}

} // namespace vws::presentation
