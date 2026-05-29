#include "domain/Workflow.h"
#include "presentation/state/AppStore.h"
#include "ui/canvas/CanvasHeader.h"
#include "ui/canvas/WorkflowCanvas.h"
#include "ui/inspector/NodeInspector.h"
#include "ui/main/MainWindowUiCoordinator.h"
#include "ui/output/OutputPanel.h"
#include "ui/widgets/EmptyStateOverlay.h"
#include "ui/workspace/WorkspaceExplorer.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTextStream>

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

vws::domain::Node makeNode()
{
    vws::domain::Node node;
    node.nodeId = "node-a";
    node.name = "Node A";
    node.type = "function";
    node.runtime.timeoutMs = 1234;
    node.config.insert("language", "python");
    node.config.insert("code", "def run(inputs, context):\n    return {'outputs': {'output': []}, 'artifacts': []}\n");
    return node;
}

vws::domain::Workflow makeWorkflow()
{
    vws::domain::Workflow workflow;
    workflow.workflowId = "workflow-a";
    workflow.name = "Workflow A";
    workflow.nodes.append(makeNode());
    return workflow;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    vws::presentation::AppStore store;
    store.setCurrentWorkflow(makeWorkflow());

    QWidget owner;
    auto* canvasHeader = new vws::ui::CanvasHeader(&owner);
    auto* workspaceExplorer = new vws::ui::WorkspaceExplorer(&owner);
    auto* workflowCanvas = new vws::ui::WorkflowCanvas(&owner);
    auto* nodeInspector = new vws::ui::NodeInspector(&owner);
    auto* outputPanel = new vws::ui::OutputPanel(&owner);
    auto* canvasOverlay = new vws::ui::EmptyStateOverlay(&owner);
    auto* timeoutLabel = new QLabel(&owner);
    auto* pythonLabel = new QLabel(&owner);

    workflowCanvas->setWorkflow(store.currentWorkflow());

    vws::ui::MainWindowUiCoordinator coordinator(store, {
        canvasHeader,
        workspaceExplorer,
        workflowCanvas,
        nodeInspector,
        outputPanel,
        canvasOverlay,
        timeoutLabel,
        pythonLabel,
    });

    auto* stdoutView = outputPanel->findChild<QPlainTextEdit*>("stdoutView");
    auto* inspectorOutputView = nodeInspector->findChild<QPlainTextEdit*>("inspectorOutputJsonView");
    if (const auto check = expect(stdoutView != nullptr, "Output panel should expose stdout view")) {
        return check;
    }
    if (const auto check = expect(inspectorOutputView != nullptr, "Inspector should expose output JSON view")) {
        return check;
    }

    coordinator.selectNode(store.currentWorkflow().nodes.first());
    if (const auto check = expect(timeoutLabel->text().contains("1234"),
            "Selecting a node should update selected-node status in one place")) {
        return check;
    }

    coordinator.handleNodeOutput(
        "run-a",
        "workflow-a",
        "node-a",
        QJsonObject{{"output", QJsonArray{QJsonObject{{"value", 7}}}}});
    if (const auto check = expect(store.nodeOutputsByNodeId().contains("node-a"),
            "Node output event should update AppStore output cache")) {
        return check;
    }
    if (const auto check = expect(inspectorOutputView->toPlainText().contains("\"value\""),
            "Node output event should refresh selected-node Inspector")) {
        return check;
    }

    coordinator.appendLog("keep this while navigating canvas");
    vws::ui::CanvasBreadcrumbViewModel breadcrumb;
    coordinator.renderCanvasBreadcrumb(breadcrumb);
    if (const auto check = expect(stdoutView->toPlainText().contains("keep this"),
            "Canvas/subsystem navigation UI refresh should not clear output")) {
        return check;
    }

    coordinator.resetForWorkflowChange();
    if (const auto check = expect(stdoutView->toPlainText().isEmpty(),
            "Workflow changes should clear output through the coordinator")) {
        return check;
    }
    if (const auto check = expect(store.nodeOutputsByNodeId().isEmpty() && store.selectedNodeId().isEmpty(),
            "Workflow changes should clear selected node and output cache together")) {
        return check;
    }

    coordinator.setPythonExecutableStatus("C:/Python/python.exe");
    if (const auto check = expect(pythonLabel->text().contains("python.exe"),
            "Python status label should be updated through the coordinator")) {
        return check;
    }

    QTextStream(stdout) << "main window UI coordinator tests passed" << Qt::endl;
    return 0;
}
