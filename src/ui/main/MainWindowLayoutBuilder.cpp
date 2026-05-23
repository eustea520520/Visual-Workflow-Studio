#include "ui/main/MainWindowLayoutBuilder.h"

#include "ui/canvas/CanvasHeader.h"
#include "ui/canvas/WorkflowCanvas.h"
#include "ui/inspector/NodeInspector.h"
#include "ui/output/OutputPanel.h"
#include "ui/theme/UiMetrics.h"
#include "ui/widgets/CommandBar.h"
#include "ui/widgets/EmptyStateOverlay.h"
#include "ui/widgets/IconSquareButton.h"
#include "ui/workspace/WorkspaceExplorer.h"

#include <QIcon>
#include <QSplitter>
#include <QStackedLayout>
#include <QVBoxLayout>
#include <QWidget>

namespace vws::ui {

MainWindowLayoutBuilder::MainWindowLayoutBuilder(QWidget* owner)
    : m_owner(owner)
{
}

MainWindowLayout MainWindowLayoutBuilder::build(const MainWindowLayoutActions& actions) const
{
    MainWindowLayout layout;
    layout.commandBar = buildCommandBar(actions);
    layout.canvasHeader = new CanvasHeader(m_owner);
    layout.workspaceExplorer = new WorkspaceExplorer(m_owner);
    layout.workflowCanvas = new WorkflowCanvas(m_owner);
    layout.nodeInspector = new NodeInspector(m_owner);
    layout.outputPanel = new OutputPanel(m_owner);
    layout.canvasOverlay = new EmptyStateOverlay(m_owner);

    auto* canvasHost = buildCanvasHost(layout.canvasHeader, layout.workflowCanvas, layout.canvasOverlay);

    auto* horizontalSplitter = new QSplitter(Qt::Horizontal, m_owner);
    horizontalSplitter->setObjectName(QStringLiteral("mainHorizontalSplitter"));
    horizontalSplitter->addWidget(layout.workspaceExplorer);
    horizontalSplitter->addWidget(canvasHost);
    horizontalSplitter->addWidget(layout.nodeInspector);
    horizontalSplitter->setStretchFactor(0, 0);
    horizontalSplitter->setStretchFactor(1, 1);
    horizontalSplitter->setStretchFactor(2, 0);
    horizontalSplitter->setSizes({UiMetrics::ExplorerDefaultWidth, 880, UiMetrics::InspectorDefaultWidth});

    auto* verticalSplitter = new QSplitter(Qt::Vertical, m_owner);
    verticalSplitter->setObjectName(QStringLiteral("mainVerticalSplitter"));
    verticalSplitter->addWidget(horizontalSplitter);
    verticalSplitter->addWidget(layout.outputPanel);
    verticalSplitter->setStretchFactor(0, 1);
    verticalSplitter->setStretchFactor(1, 0);
    verticalSplitter->setSizes({650, UiMetrics::OutputDefaultHeight});

    auto* centralWrapper = new QWidget(m_owner);
    centralWrapper->setObjectName(QStringLiteral("centralWrapper"));
    auto* centralLayout = new QVBoxLayout(centralWrapper);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(layout.commandBar);
    centralLayout->addWidget(verticalSplitter, 1);

    layout.centralWidget = centralWrapper;
    return layout;
}

CommandBar* MainWindowLayoutBuilder::buildCommandBar(const MainWindowLayoutActions& actions) const
{
    auto* commandBar = new CommandBar(m_owner);

    commandBar->addActionButton(QIcon(":/icons/workspace-new.svg"),
        actions.newWorkspace, IconSquareButton::Role::Secondary);
    commandBar->addActionButton(QIcon(":/icons/workspace-open.svg"),
        actions.openWorkspace, IconSquareButton::Role::Secondary);
    commandBar->addActionButton(QIcon(":/icons/python.svg"),
        actions.selectPython, IconSquareButton::Role::Secondary);
    commandBar->addSeparator();
    commandBar->addActionButton(QIcon(":/icons/workflow-new.svg"),
        actions.newWorkflow, IconSquareButton::Role::Secondary);
    commandBar->addActionButton(QIcon(":/icons/save.svg"),
        actions.saveWorkflow, IconSquareButton::Role::Secondary);
    commandBar->addActionButton(QIcon(":/icons/LLM.svg"),
        actions.generateWorkflowByLlm, IconSquareButton::Role::Secondary);
    commandBar->addSeparator();
    commandBar->addActionButton(QIcon(":/icons/template-save.svg"),
        actions.saveTemplate, IconSquareButton::Role::Secondary);
    commandBar->addActionButton(QIcon(":/icons/link.svg"),
        actions.connectNodes, IconSquareButton::Role::Secondary);
    commandBar->addActionButton(QIcon(":/icons/import.svg"),
        actions.importTemplate, IconSquareButton::Role::Secondary);
    commandBar->addSeparator();
    commandBar->addActionButton(QIcon(":/icons/run.svg"),
        actions.runWorkflow, IconSquareButton::Role::Primary);
    commandBar->addActionButton(QIcon(":/icons/stop.svg"),
        actions.cancelRun, IconSquareButton::Role::Danger);
    commandBar->addActionButton(QIcon(":/icons/theme.svg"),
        actions.toggleTheme, IconSquareButton::Role::Ghost);

    return commandBar;
}

QWidget* MainWindowLayoutBuilder::buildCanvasHost(CanvasHeader* canvasHeader, WorkflowCanvas* workflowCanvas, EmptyStateOverlay* canvasOverlay) const
{
    auto* canvasHost = new QWidget(m_owner);
    canvasHost->setObjectName(QStringLiteral("canvasHost"));

    auto* hostLayout = new QVBoxLayout(canvasHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);
    hostLayout->addWidget(canvasHeader);

    auto* canvasStackHost = new QWidget(canvasHost);
    canvasStackHost->setObjectName(QStringLiteral("canvasStackHost"));
    auto* canvasStack = new QStackedLayout(canvasStackHost);
    canvasStack->setStackingMode(QStackedLayout::StackAll);
    canvasStack->setContentsMargins(0, 0, 0, 0);
    canvasStack->addWidget(workflowCanvas);
    canvasStack->addWidget(canvasOverlay);
    hostLayout->addWidget(canvasStackHost, 1);

    return canvasHost;
}

} // namespace vws::ui
