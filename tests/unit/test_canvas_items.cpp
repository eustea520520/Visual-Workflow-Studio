#include "domain/Workflow.h"
#include "ui/canvas/EdgeGraphicsItem.h"
#include "ui/canvas/NodeGraphicsItem.h"
#include "ui/canvas/WorkflowCanvas.h"

#include <QApplication>
#include <QGraphicsScene>
#include <QMouseEvent>
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

vws::domain::Node makeNode(const QString& id, const QString& name, double x, double y)
{
    vws::domain::Node node;
    node.nodeId = id;
    node.type = "function";
    node.name = name;
    node.inputPorts = {"input"};
    node.outputPorts = {"output"};
    node.position.x = x;
    node.position.y = y;
    node.config = {
        {"code", "def run(inputs, context):\n    return {'outputs': inputs, 'artifacts': []}"},
    };
    return node;
}

vws::ui::NodeGraphicsItem* findNodeItem(QGraphicsScene* scene, const QString& nodeId)
{
    for (auto* item : scene->items()) {
        if (auto* nodeItem = dynamic_cast<vws::ui::NodeGraphicsItem*>(item)) {
            if (nodeItem->nodeId() == nodeId) {
                return nodeItem;
            }
        }
    }
    return nullptr;
}

vws::ui::EdgeGraphicsItem* findFirstEdgeItem(QGraphicsScene* scene)
{
    for (auto* item : scene->items()) {
        if (auto* edgeItem = dynamic_cast<vws::ui::EdgeGraphicsItem*>(item)) {
            return edgeItem;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // Verify canvas data flow: workflow rendering, node movement, selection, and edge creation.
    vws::domain::Workflow workflow;
    workflow.workflowId = "canvas-test";
    workflow.workspaceId = "workspace-test";
    workflow.name = "Canvas Test";
    workflow.nodes = {
        makeNode("node-a", "Node A", -120, 0),
        makeNode("node-b", "Node B", 160, 0),
    };

    vws::ui::WorkflowCanvas canvas;
    canvas.setWorkflow(workflow);

    if (const auto check = expect(canvas.workflow().nodes.size() == 2, "Canvas should preserve two nodes")) {
        return check;
    }

    canvas.addStarterNodeAt(QPointF(-360, 36));
    auto workflowWithStarterNode = canvas.workflow();
    if (const auto check = expect(workflowWithStarterNode.nodes.size() == 3,
            "Canvas context helper should add a starter node")) {
        return check;
    }
    if (const auto check = expect(workflowWithStarterNode.nodes.last().type == "starter",
            "Context-created starter node should use starter type")) {
        return check;
    }
    if (const auto check = expect(workflowWithStarterNode.nodes.last().inputPorts.isEmpty(),
            "Starter node should not define input ports")) {
        return check;
    }
    if (const auto check = expect(workflowWithStarterNode.nodes.last().outputPorts == QStringList{"output"},
            "Starter node should define one output port")) {
        return check;
    }

    canvas.addFunctionNodeAt(QPointF(24, 36));
    auto workflowWithContextNode = canvas.workflow();
    if (const auto check = expect(workflowWithContextNode.nodes.size() == 4,
            "Canvas context helper should add a function node")) {
        return check;
    }
    if (const auto check = expect(workflowWithContextNode.nodes.last().type == "function",
            "Context-created node should be a function node")) {
        return check;
    }
    if (const auto check = expect(workflowWithContextNode.nodes.last().position.x == 24.0,
            "Context-created node should use the requested x position")) {
        return check;
    }
    if (const auto check = expect(workflowWithContextNode.nodes.last().position.y == 36.0,
            "Context-created node should use the requested y position")) {
        return check;
    }
    const auto selectedContextNode = canvas.selectedNode();
    if (const auto check = expect(selectedContextNode.has_value(),
            "Adding a node should select it for template-saving workflows")) {
        return check;
    }
    if (const auto check = expect(selectedContextNode->nodeId == workflowWithContextNode.nodes.last().nodeId,
            "selectedNode should return the currently selected graphics node")) {
        return check;
    }

    auto* nodeA = findNodeItem(canvas.scene(), "node-a");
    auto* nodeB = findNodeItem(canvas.scene(), "node-b");
    if (const auto check = expect(nodeA != nullptr, "Node A graphics item should exist")) {
        return check;
    }
    if (const auto check = expect(nodeB != nullptr, "Node B graphics item should exist")) {
        return check;
    }

    const auto pressPos = canvas.mapFromScene(nodeA->outputAnchorScenePos());
    const auto releasePos = canvas.mapFromScene(nodeB->inputAnchorScenePos());
    QMouseEvent pressEvent(
        QEvent::MouseButtonPress,
        QPointF(pressPos),
        QPointF(canvas.viewport()->mapToGlobal(pressPos)),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &pressEvent);

    QMouseEvent moveEvent(
        QEvent::MouseMove,
        QPointF(releasePos),
        QPointF(canvas.viewport()->mapToGlobal(releasePos)),
        Qt::NoButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &moveEvent);

    QMouseEvent releaseEvent(
        QEvent::MouseButtonRelease,
        QPointF(releasePos),
        QPointF(canvas.viewport()->mapToGlobal(releasePos)),
        Qt::LeftButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &releaseEvent);

    if (const auto check = expect(canvas.workflow().edges.size() == 1,
            "Dragging from an output port to an input port should create one edge")) {
        return check;
    }
    auto* edgeItem = findFirstEdgeItem(canvas.scene());
    if (const auto check = expect(edgeItem != nullptr,
            "Created edge should have a graphics item")) {
        return check;
    }
    if (const auto check = expect(edgeItem->path().elementCount() >= 2,
            "Created edge should build a drawable routed path")) {
        return check;
    }

    auto updatedNodeA = nodeA->node();
    updatedNodeA.config.insert("code", "def run(inputs, context):\n    return {'outputs': {'output': 3}, 'artifacts': []}");
    if (const auto check = expect(canvas.updateNode(updatedNodeA),
            "Canvas should update one node without rebuilding the whole scene")) {
        return check;
    }
    if (const auto check = expect(findNodeItem(canvas.scene(), "node-b") == nodeB,
            "Updating one node should keep unrelated node graphics items alive")) {
        return check;
    }
    if (const auto check = expect(canvas.workflow().nodes.first().config.value("code").toString().contains("'output': 3"),
            "Updated node config should be reflected in canvas workflow")) {
        return check;
    }

    auto* starterItem = findNodeItem(canvas.scene(), workflowWithStarterNode.nodes.last().nodeId);
    if (const auto check = expect(starterItem != nullptr, "Starter node graphics item should exist")) {
        return check;
    }
    auto updatedStarterNode = starterItem->node();
    updatedStarterNode.config.insert("code", "def run(inputs, context):\n    return {'outputs': {'output': {'ready': True}}, 'artifacts': []}");
    if (const auto check = expect(canvas.updateNode(updatedStarterNode),
            "Canvas should update Starter node code without rebuilding the scene")) {
        return check;
    }
    if (const auto check = expect(findNodeItem(canvas.scene(), updatedStarterNode.nodeId) == starterItem,
            "Updating Starter node code should keep the same graphics item alive")) {
        return check;
    }
    const auto workflowAfterStarterUpdate = canvas.workflow();
    bool starterStillHasNoInputs = false;
    for (const auto& node : workflowAfterStarterUpdate.nodes) {
        if (node.nodeId == updatedStarterNode.nodeId) {
            starterStillHasNoInputs = node.inputPorts.isEmpty();
            break;
        }
    }
    if (const auto check = expect(starterStillHasNoInputs,
            "Updating Starter node code should preserve empty input ports")) {
        return check;
    }

    canvas.addStarterNodeAt(QPointF(-520, 156));
    const auto workflowWithSecondStarter = canvas.workflow();
    const auto secondStarterId = workflowWithSecondStarter.nodes.last().nodeId;
    auto* secondStarterItem = findNodeItem(canvas.scene(), secondStarterId);
    if (const auto check = expect(secondStarterItem != nullptr,
            "Second Starter node graphics item should exist")) {
        return check;
    }
    auto updatedSecondStarterNode = secondStarterItem->node();
    updatedSecondStarterNode.config.insert("code", "def run(inputs, context):\n    return {'outputs': {'output': {'second': True}}, 'artifacts': []}");
    if (const auto check = expect(canvas.updateNode(updatedSecondStarterNode),
            "Canvas should update a second Starter node after the first Starter was saved")) {
        return check;
    }
    if (const auto check = expect(findNodeItem(canvas.scene(), secondStarterId) == secondStarterItem,
            "Updating the second Starter should keep its graphics item alive")) {
        return check;
    }

    for (int i = 0; i < 20; ++i) {
        canvas.addStarterNodeAt(QPointF(-640 + i * 12, 240 + i * 4));
        const auto stressWorkflow = canvas.workflow();
        const auto stressStarterId = stressWorkflow.nodes.last().nodeId;
        auto* stressStarterItem = findNodeItem(canvas.scene(), stressStarterId);
        if (const auto check = expect(stressStarterItem != nullptr,
                "Stress-created Starter node graphics item should exist")) {
            return check;
        }

        auto stressStarterNode = stressStarterItem->node();
        stressStarterNode.config.insert("code",
            QString("def run(inputs, context):\n    return {'outputs': {'output': {'index': %1}}, 'artifacts': []}").arg(i));
        if (const auto check = expect(canvas.updateNode(stressStarterNode),
                "Canvas should repeatedly update newly created Starter nodes")) {
            return check;
        }
        app.processEvents();
    }

    if (const auto check = expect(canvas.workflow().nodes.size() >= 23,
            "Repeated Starter creation/update should keep workflow nodes alive")) {
        return check;
    }

    nodeA->setPos(42, 84);
    app.processEvents();
    const auto movedWorkflow = canvas.workflow();
    if (const auto check = expect(movedWorkflow.nodes.first().position.x == 42.0,
            "Dragging a node should update workflow node position x")) {
        return check;
    }
    if (const auto check = expect(movedWorkflow.nodes.first().position.y == 84.0,
            "Dragging a node should update workflow node position y")) {
        return check;
    }

    canvas.setNodeStatus("node-a", "running");
    if (const auto check = expect(nodeA->visualState() == vws::ui::NodeVisualState::Running,
            "Running status should update node visual state")) {
        return check;
    }

    const auto countBeforeUndoShortcut = canvas.workflow().nodes.size();
    canvas.addFunctionNodeAt(QPointF(320, 220));
    QKeyEvent undoEvent(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
    QApplication::sendEvent(&canvas, &undoEvent);
    if (const auto check = expect(canvas.workflow().nodes.size() == countBeforeUndoShortcut,
            "Ctrl+Z on the canvas should undo the previous canvas edit")) {
        return check;
    }

    vws::domain::Workflow dragResetWorkflow;
    dragResetWorkflow.workflowId = "drag-reset";
    dragResetWorkflow.workspaceId = "workspace-test";
    dragResetWorkflow.name = "Drag Reset";
    dragResetWorkflow.nodes = {
        makeNode("drag-a", "Drag A", -120, 0),
        makeNode("drag-b", "Drag B", 160, 0),
    };
    canvas.setWorkflow(dragResetWorkflow);
    auto* dragSource = findNodeItem(canvas.scene(), "drag-a");
    if (const auto check = expect(dragSource != nullptr, "Drag source node should exist")) {
        return check;
    }
    const auto dragPressPos = canvas.mapFromScene(dragSource->outputAnchorScenePos());
    QMouseEvent dragPressEvent(
        QEvent::MouseButtonPress,
        QPointF(dragPressPos),
        QPointF(canvas.viewport()->mapToGlobal(dragPressPos)),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &dragPressEvent);
    canvas.clearWorkflow();
    QMouseEvent dragReleaseEvent(
        QEvent::MouseButtonRelease,
        QPointF(dragPressPos),
        QPointF(canvas.viewport()->mapToGlobal(dragPressPos)),
        Qt::LeftButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &dragReleaseEvent);
    app.processEvents();

    QTextStream(stdout) << "canvas item tests passed" << Qt::endl;
    return 0;
}
