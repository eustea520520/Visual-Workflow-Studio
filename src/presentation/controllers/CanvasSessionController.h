#pragma once

#include "application/WorkflowHistory.h"
#include "domain/Workflow.h"

namespace vws::presentation {

class AppStore;
class CanvasNavigationController;
class WorkflowController;

// Keeps MainWindow out of the details of "root workflow vs subsystem workflow".
// Widgets still render the canvas; this controller only synchronizes document state.
class CanvasSessionController final {
public:
    CanvasSessionController(
        AppStore& store,
        WorkflowController& workflowController,
        CanvasNavigationController& navigationController);

    void startRootSession(const application::WorkflowHistory& history = {});
    void syncCurrentView(const domain::Workflow& canvasWorkflow);
    bool enterSubsystem(
        const domain::Workflow& canvasWorkflow,
        const application::WorkflowHistory& history,
        const QString& subsystemNodeId,
        QString* errorMessage = nullptr);
    bool navigateToDepth(
        int depth,
        const domain::Workflow& canvasWorkflow,
        const application::WorkflowHistory& history,
        QString* errorMessage = nullptr);
    bool cacheCurrentView(
        const domain::Workflow& canvasWorkflow,
        const application::WorkflowHistory& history,
        QString* errorMessage = nullptr);
    bool prepareRootWorkflowFromView(
        const domain::Workflow& canvasWorkflow,
        const application::WorkflowHistory& history,
        domain::Workflow& rootWorkflow,
        QString* errorMessage = nullptr);
    void updateCurrentHistory(const application::WorkflowHistory& history);

private:
    AppStore& m_store;
    WorkflowController& m_workflowController;
    CanvasNavigationController& m_navigationController;
};

} // namespace vws::presentation
