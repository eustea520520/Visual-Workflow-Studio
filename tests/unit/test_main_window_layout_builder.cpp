#include "ui/main/MainWindowLayoutBuilder.h"

#include "ui/canvas/CanvasHeader.h"
#include "ui/canvas/WorkflowCanvas.h"
#include "ui/inspector/NodeInspector.h"
#include "ui/output/OutputPanel.h"
#include "ui/widgets/CommandBar.h"
#include "ui/widgets/EmptyStateOverlay.h"
#include "ui/workspace/WorkspaceExplorer.h"
#include "ui/workspace/WorkspaceExplorerViewModel.h"

#include <QAction>
#include <QApplication>
#include <QTextStream>
#include <QWidget>
#include <QTreeWidget>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << message << Qt::endl;
    return 1;
}

int expect(bool condition, const QString& message)
{
    return condition ? 0 : fail(message);
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QWidget owner;

    QAction newWorkspace(&owner);
    QAction openWorkspace(&owner);
    QAction selectPython(&owner);
    QAction newWorkflow(&owner);
    QAction saveWorkflow(&owner);
    QAction saveTemplate(&owner);
    QAction connectNodes(&owner);
    QAction importTemplate(&owner);
    QAction generateWorkflowByLlm(&owner);
    QAction runWorkflow(&owner);
    QAction cancelRun(&owner);
    QAction toggleTheme(&owner);

    const vws::ui::MainWindowLayoutActions actions {
        &newWorkspace,
        &openWorkspace,
        &selectPython,
        &newWorkflow,
        &saveWorkflow,
        &saveTemplate,
        &connectNodes,
        &importTemplate,
        &generateWorkflowByLlm,
        &runWorkflow,
        &cancelRun,
        &toggleTheme,
    };

    const vws::ui::MainWindowLayoutBuilder builder(&owner);
    const auto layout = builder.build(actions);

    if (const auto check = expect(layout.centralWidget != nullptr, "Layout builder should create central widget")) {
        return check;
    }
    if (const auto check = expect(layout.commandBar != nullptr, "Layout builder should create command bar")) {
        return check;
    }
    if (const auto check = expect(layout.workspaceExplorer != nullptr, "Layout builder should create workspace explorer")) {
        return check;
    }
    if (const auto check = expect(layout.workflowCanvas != nullptr, "Layout builder should create workflow canvas")) {
        return check;
    }
    if (const auto check = expect(layout.canvasHeader != nullptr, "Layout builder should create canvas header")) {
        return check;
    }
    if (const auto check = expect(layout.nodeInspector != nullptr, "Layout builder should create node inspector")) {
        return check;
    }
    if (const auto check = expect(layout.outputPanel != nullptr, "Layout builder should create output panel")) {
        return check;
    }
    if (const auto check = expect(layout.canvasOverlay != nullptr, "Layout builder should create canvas overlay")) {
        return check;
    }

    if (const auto check = expect(layout.centralWidget->objectName() == "centralWrapper",
            "Central widget should have stable objectName")) {
        return check;
    }
    if (const auto check = expect(owner.findChild<QWidget*>("canvasHost") != nullptr,
            "Canvas host should exist outside the scene")) {
        return check;
    }
    if (const auto check = expect(owner.findChild<vws::ui::CanvasHeader*>("canvasHeader") == layout.canvasHeader,
            "CanvasHeader should be discoverable above the canvas")) {
        return check;
    }
    if (const auto check = expect(owner.findChild<vws::ui::CommandBar*>("CommandBar") == layout.commandBar,
            "CommandBar should be discoverable by objectName")) {
        return check;
    }
    if (const auto check = expect(owner.findChild<vws::ui::WorkspaceExplorer*>("workspaceExplorer") == layout.workspaceExplorer,
            "WorkspaceExplorer should be discoverable by objectName")) {
        return check;
    }
    if (const auto check = expect(owner.findChild<vws::ui::WorkflowCanvas*>("workflowCanvas") == layout.workflowCanvas,
            "WorkflowCanvas should be discoverable by objectName")) {
        return check;
    }
    if (const auto check = expect(owner.findChild<vws::ui::NodeInspector*>("nodeInspector") == layout.nodeInspector,
            "NodeInspector should be discoverable by objectName")) {
        return check;
    }
    if (const auto check = expect(owner.findChild<vws::ui::OutputPanel*>("outputPanel") == layout.outputPanel,
            "OutputPanel should be discoverable by objectName")) {
        return check;
    }
    if (const auto check = expect(owner.findChild<vws::ui::EmptyStateOverlay*>("canvasOverlay") == layout.canvasOverlay,
            "EmptyStateOverlay should be discoverable by objectName")) {
        return check;
    }

    vws::ui::WorkspaceExplorerViewModel workspaceViewModel;
    workspaceViewModel.workspaceName = "Demo Workspace";
    workspaceViewModel.workflows = {
        vws::ui::WorkspaceExplorerItemViewModel{"wf-1", "Demo Workflow", true},
    };
    workspaceViewModel.templates = {
        vws::ui::WorkspaceExplorerItemViewModel{"template-1", "Reusable Node", false},
    };
    workspaceViewModel.runs = {
        vws::ui::WorkspaceExplorerItemViewModel{"run-1", "Latest Run", false},
    };
    layout.workspaceExplorer->render(workspaceViewModel);
    auto* workspaceTree = layout.workspaceExplorer->findChild<QTreeWidget*>("workspaceTree");
    if (const auto check = expect(workspaceTree != nullptr,
            "WorkspaceExplorer should keep a stable tree objectName")) {
        return check;
    }
    if (const auto check = expect(workspaceTree->topLevelItem(0)->child(0)->data(0, Qt::UserRole).toString() == "wf-1",
            "WorkspaceExplorer render should bind workflow ids to rendered rows")) {
        return check;
    }

    QTextStream(stdout) << "main window layout builder tests passed" << Qt::endl;
    return 0;
}
