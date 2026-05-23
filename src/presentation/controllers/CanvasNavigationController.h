#pragma once

#include "application/WorkflowHistory.h"
#include "application/subsystem/SubsystemService.h"
#include "domain/Workspace.h"
#include "domain/Workflow.h"
#include "ui/canvas/CanvasBreadcrumbViewModel.h"

#include <QList>
#include <QStringList>

namespace vws::presentation {

struct CanvasDocumentContext {
    QString label;
    QString parentNodeId;
    QStringList nodePath;
    domain::Workflow workflow;
    application::WorkflowHistory history;
};

class CanvasNavigationController final {
public:
    explicit CanvasNavigationController(application::SubsystemService& subsystemService);

    void reset();
    void setRootWorkflow(
        const domain::Workspace& workspace,
        const domain::Workflow& workflow,
        const application::WorkflowHistory& history = {});

    bool hasRootWorkflow() const;
    bool isInsideSubsystem() const;
    int currentDepth() const;

    const domain::Workflow& currentWorkflow() const;
    application::WorkflowHistory currentHistory() const;
    domain::Workflow rootWorkflow() const;
    ui::CanvasBreadcrumbViewModel breadcrumbViewModel() const;

    void updateCurrentWorkflowFromView(const domain::Workflow& workflow);
    void updateCurrentHistory(const application::WorkflowHistory& history);

    bool enterSubsystem(
        const domain::Workflow& currentCanvasWorkflow,
        const application::WorkflowHistory& currentHistory,
        const QString& subsystemNodeId,
        QString* errorMessage = nullptr);

    bool navigateToDepth(
        int depth,
        const domain::Workflow& currentCanvasWorkflow,
        const application::WorkflowHistory& currentHistory,
        QString* errorMessage = nullptr);

    bool flushCurrentWorkflow(
        const domain::Workflow& currentCanvasWorkflow,
        const application::WorkflowHistory& currentHistory,
        domain::Workflow& rootWorkflow,
        QString* errorMessage = nullptr);

private:
    bool propagateChildrenToRoot(QString* errorMessage = nullptr);
    domain::Node* findNode(domain::Workflow& workflow, const QString& nodeId) const;

    application::SubsystemService& m_subsystemService;
    domain::Workspace m_workspace;
    QList<CanvasDocumentContext> m_stack;
};

} // namespace vws::presentation
