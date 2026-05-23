#include "application/NodeFactory.h"
#include "domain/Workflow.h"
#include "ui/canvas/EdgeGraphicsItem.h"
#include "ui/canvas/NodeGraphicsItem.h"
#include "ui/canvas/WorkflowCanvas.h"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
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

void setIoDimension(vws::domain::Node& node, int inputDimension, int outputDimension)
{
    node.ioSpec.inputs.clear();
    node.ioSpec.outputs.clear();
    if (!node.inputPorts.isEmpty()) {
        vws::domain::PortDimensionSpec input;
        input.portName = "input";
        input.dimension = inputDimension;
        input.itemLabels = {"1", "2", "3"};
        node.ioSpec.inputs.append(input);
    }
    if (!node.outputPorts.isEmpty()) {
        vws::domain::PortDimensionSpec output;
        output.portName = "output";
        output.dimension = outputDimension;
        output.itemLabels = {"1", "2", "3"};
        node.ioSpec.outputs.append(output);
    }
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

    canvas.addNode(vws::application::NodeFactory::createStarterNode(
        QPointF(-360, 36),
        canvas.workflow().nodes.size(),
        vws::application::NodeFactory::StarterTemplateKind::DataOutput));
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

    canvas.addNode(vws::application::NodeFactory::createFunctionNode(
        QPointF(24, 36),
        canvas.workflow().nodes.size(),
        vws::application::DataTransferTemplate::DataToData));
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

    auto rotatedNodeA = nodeA->node();
    rotatedNodeA.rotationDegrees = 90;
    nodeA->setNode(rotatedNodeA);
    if (const auto check = expect(nodeA->inputAnchorScenePos().y() < nodeA->bodySceneRect().center().y(),
            "90-degree node input anchor should move to the top")) {
        return check;
    }
    if (const auto check = expect(nodeA->outputAnchorScenePos().y() > nodeA->bodySceneRect().center().y(),
            "90-degree node output anchor should move to the bottom")) {
        return check;
    }
    rotatedNodeA.rotationDegrees = 180;
    nodeA->setNode(rotatedNodeA);
    if (const auto check = expect(nodeA->inputAnchorScenePos().x() > nodeA->outputAnchorScenePos().x(),
            "180-degree node should swap input/output horizontal anchors")) {
        return check;
    }
    rotatedNodeA.rotationDegrees = 0;
    nodeA->setNode(rotatedNodeA);

    nodeA->setSelected(true);
    const auto widthBeforeResize = nodeA->bodySceneRect().width();
    const QPointF resizeHandleScenePos(
        nodeA->bodySceneRect().right() + 6.0,
        nodeA->bodySceneRect().center().y());
    const QPointF resizeTargetScenePos = resizeHandleScenePos + QPointF(52.0, 0.0);
    QGraphicsSceneMouseEvent resizePressEvent(QEvent::GraphicsSceneMousePress);
    resizePressEvent.setScenePos(resizeHandleScenePos);
    resizePressEvent.setPos(nodeA->mapFromScene(resizeHandleScenePos));
    resizePressEvent.setButton(Qt::LeftButton);
    resizePressEvent.setButtons(Qt::LeftButton);
    canvas.scene()->sendEvent(nodeA, &resizePressEvent);

    QGraphicsSceneMouseEvent resizeMoveEvent(QEvent::GraphicsSceneMouseMove);
    resizeMoveEvent.setScenePos(resizeTargetScenePos);
    resizeMoveEvent.setLastScenePos(resizeHandleScenePos);
    resizeMoveEvent.setPos(nodeA->mapFromScene(resizeTargetScenePos));
    resizeMoveEvent.setLastPos(nodeA->mapFromScene(resizeHandleScenePos));
    resizeMoveEvent.setButtons(Qt::LeftButton);
    canvas.scene()->sendEvent(nodeA, &resizeMoveEvent);

    QGraphicsSceneMouseEvent resizeReleaseEvent(QEvent::GraphicsSceneMouseRelease);
    resizeReleaseEvent.setScenePos(resizeTargetScenePos);
    resizeReleaseEvent.setPos(nodeA->mapFromScene(resizeTargetScenePos));
    resizeReleaseEvent.setButton(Qt::LeftButton);
    resizeReleaseEvent.setButtons(Qt::NoButton);
    canvas.scene()->sendEvent(nodeA, &resizeReleaseEvent);
    if (const auto check = expect(nodeA->bodySceneRect().width() > widthBeforeResize + 30.0,
            "Dragging a selected node side handle should resize the node body")) {
        return check;
    }
    bool nodeSizeStored = false;
    for (const auto& node : canvas.workflow().nodes) {
        if (node.nodeId == "node-a") {
            nodeSizeStored = node.size.isValid() && node.size.width > widthBeforeResize + 30.0;
            break;
        }
    }
    if (const auto check = expect(nodeSizeStored,
            "Resized node size should be stored in canvas workflow data")) {
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
    if (const auto check = expect(canvas.workflow().edges.first().fromSlot == 0
            && canvas.workflow().edges.first().toSlot == 0,
            "Dragging default single-circle ports should create a slot-0 edge")) {
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

    canvas.addNode(vws::application::NodeFactory::createStarterNode(
        QPointF(-520, 156),
        canvas.workflow().nodes.size(),
        vws::application::NodeFactory::StarterTemplateKind::DataOutput));
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
        canvas.addNode(vws::application::NodeFactory::createStarterNode(
            QPointF(-640 + i * 12, 240 + i * 4),
            canvas.workflow().nodes.size(),
            vws::application::NodeFactory::StarterTemplateKind::DataOutput));
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
    canvas.addNode(vws::application::NodeFactory::createFunctionNode(
        QPointF(320, 220),
        canvas.workflow().nodes.size(),
        vws::application::DataTransferTemplate::DataToData));
    QKeyEvent undoEvent(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
    QApplication::sendEvent(&canvas, &undoEvent);
    if (const auto check = expect(canvas.workflow().nodes.size() == countBeforeUndoShortcut,
            "Ctrl+Z on the canvas should undo the previous canvas edit")) {
        return check;
    }

    vws::domain::Workflow historyWorkflow;
    historyWorkflow.workflowId = "history-workflow";
    historyWorkflow.workspaceId = "workspace-test";
    historyWorkflow.name = "History Workflow";
    historyWorkflow.nodes = {makeNode("history-a", "History A", -80, 0)};
    canvas.setWorkflow(historyWorkflow);
    canvas.addNode(vws::application::NodeFactory::createFunctionNode(
        QPointF(180, 0),
        canvas.workflow().nodes.size(),
        vws::application::DataTransferTemplate::DataToData));
    const auto historyWithAddedNode = canvas.history();
    const auto workflowWithAddedNode = canvas.workflow();
    canvas.setWorkflow(workflow);
    canvas.setWorkflow(workflowWithAddedNode);
    canvas.setHistory(historyWithAddedNode);
    QApplication::sendEvent(&canvas, &undoEvent);
    if (const auto check = expect(canvas.workflow().nodes.size() == 1,
            "Restored canvas history should allow Ctrl+Z after switching back to a workflow")) {
        return check;
    }

    vws::domain::Workflow slotCanvasWorkflow;
    slotCanvasWorkflow.workflowId = "slot-canvas";
    slotCanvasWorkflow.workspaceId = "workspace-test";
    slotCanvasWorkflow.name = "Slot Canvas";
    auto slotSourceNode = makeNode("slot-a", "Slot A", -160, 0);
    auto slotTargetNode = makeNode("slot-b", "Slot B", 180, 0);
    setIoDimension(slotSourceNode, 1, 3);
    setIoDimension(slotTargetNode, 3, 1);
    slotCanvasWorkflow.nodes = {slotSourceNode, slotTargetNode};
    canvas.setWorkflow(slotCanvasWorkflow);
    auto* slotSource = findNodeItem(canvas.scene(), "slot-a");
    auto* slotTarget = findNodeItem(canvas.scene(), "slot-b");
    if (const auto check = expect(slotSource != nullptr && slotTarget != nullptr,
            "Slot test nodes should exist")) {
        return check;
    }
    if (const auto check = expect(slotSource->outputSlotCount() == 3
            && slotTarget->inputSlotCount() == 3,
            "NodeGraphicsItem should expose three slot circles for configured dimensions")) {
        return check;
    }
    if (const auto check = expect(slotSource->outputAnchorScenePos(1) != slotSource->outputAnchorScenePos(0)
            && slotTarget->inputAnchorScenePos(2) != slotTarget->inputAnchorScenePos(0),
            "Slot anchors should differ by slot index")) {
        return check;
    }
    const auto outputHit = slotSource->outputSlotAt(slotSource->outputAnchorScenePos(1), 14.0);
    const auto inputHit = slotTarget->inputSlotAt(slotTarget->inputAnchorScenePos(2), 14.0);
    if (const auto check = expect(outputHit.has_value() && outputHit->slotIndex == 1
            && inputHit.has_value() && inputHit->slotIndex == 2,
            "Slot hit-test should report exact output/input slot index")) {
        return check;
    }

    const auto slotPressPos = canvas.mapFromScene(slotSource->outputAnchorScenePos(1));
    const auto slotReleasePos = canvas.mapFromScene(slotTarget->inputAnchorScenePos(2));
    QMouseEvent slotPressEvent(
        QEvent::MouseButtonPress,
        QPointF(slotPressPos),
        QPointF(canvas.viewport()->mapToGlobal(slotPressPos)),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &slotPressEvent);
    QMouseEvent slotMoveEvent(
        QEvent::MouseMove,
        QPointF(slotReleasePos),
        QPointF(canvas.viewport()->mapToGlobal(slotReleasePos)),
        Qt::NoButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &slotMoveEvent);
    QMouseEvent slotReleaseEvent(
        QEvent::MouseButtonRelease,
        QPointF(slotReleasePos),
        QPointF(canvas.viewport()->mapToGlobal(slotReleasePos)),
        Qt::LeftButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &slotReleaseEvent);
    if (const auto check = expect(canvas.workflow().edges.size() == 1
            && canvas.workflow().edges.first().fromSlot == 1
            && canvas.workflow().edges.first().toSlot == 2,
            "Dragging output[1] to input[2] should create a slot-level edge")) {
        return check;
    }

    vws::ui::WorkflowCanvas connectCanvas;
    vws::domain::Workflow connectWorkflow;
    connectWorkflow.workflowId = "connect-selected-canvas";
    connectWorkflow.workspaceId = "workspace-test";
    auto connectSourceNode = makeNode("connect-a", "Connect A", -160, 0);
    auto connectTargetNode = makeNode("connect-b", "Connect B", 180, 0);
    setIoDimension(connectSourceNode, 1, 3);
    setIoDimension(connectTargetNode, 2, 1);
    connectWorkflow.nodes = {connectSourceNode, connectTargetNode};
    connectCanvas.setWorkflow(connectWorkflow);
    auto* connectSource = findNodeItem(connectCanvas.scene(), "connect-a");
    auto* connectTarget = findNodeItem(connectCanvas.scene(), "connect-b");
    if (const auto check = expect(connectSource != nullptr && connectTarget != nullptr,
            "Connect-selected canvas should render both nodes")) {
        return check;
    }
    connectSource->setSelected(true);
    connectTarget->setSelected(true);
    if (const auto check = expect(connectCanvas.connectSelectedNodes(),
            "Connect Selected Nodes should create slot-paired edges")) {
        return check;
    }
    const auto connectedWorkflow = connectCanvas.workflow();
    if (const auto check = expect(connectedWorkflow.edges.size() == 2,
            "Connect Selected Nodes should connect as many matching slots as possible")) {
        return check;
    }
    if (const auto check = expect(connectedWorkflow.edges.at(0).fromSlot == 0
            && connectedWorkflow.edges.at(0).toSlot == 0
            && connectedWorkflow.edges.at(1).fromSlot == 1
            && connectedWorkflow.edges.at(1).toSlot == 1,
            "Connect Selected Nodes should map output slots to input slots one by one")) {
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
