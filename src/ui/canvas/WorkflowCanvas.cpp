#include "ui/canvas/WorkflowCanvas.h"

#include "ui/canvas/EdgeGraphicsItem.h"
#include "ui/editor/PythonCodeTemplates.h"
#include "ui/theme/ThemeManager.h"

#include <QContextMenuEvent>
#include <QFrame>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineF>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QUuid>
#include <QWheelEvent>

namespace vws::ui {

namespace {

constexpr qreal PortHitRadius = 14.0;
constexpr qreal ZoomStep = 1.15;
constexpr qreal MinZoom = 0.25;
constexpr qreal MaxZoom = 3.0;

} // namespace

WorkflowCanvas::WorkflowCanvas(QWidget* parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setFrameShape(QFrame::NoFrame);
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    buildScene();
}

void WorkflowCanvas::setWorkflow(const domain::Workflow& workflow)
{
    m_workflow = workflow;
    m_undoStack.clear();
    rebuildSceneFromWorkflow();
    if (!m_workflow.nodes.isEmpty()) {
        const auto firstNode = m_workflow.nodes.first();
        centerOn(firstNode.position.x, firstNode.position.y);
    }
    emit workflowChanged(m_workflow);
}

domain::Workflow WorkflowCanvas::workflow() const
{
    auto workflow = m_workflow;
    for (const auto& nodeId : m_nodeItems.keys()) {
        const auto* item = m_nodeItems.value(nodeId);
        for (auto& node : workflow.nodes) {
            if (node.nodeId == nodeId) {
                node = item->node();
                break;
            }
        }
    }
    return workflow;
}

std::optional<domain::Node> WorkflowCanvas::selectedNode() const
{
    for (auto* item : m_scene->selectedItems()) {
        if (auto* nodeItem = dynamic_cast<NodeGraphicsItem*>(item)) {
            return nodeItem->node();
        }
    }
    return std::nullopt;
}

void WorkflowCanvas::addNode(const domain::Node& node)
{
    pushUndoState();
    auto nodeToAdd = node;
    if (nodeToAdd.nodeId.isEmpty()) {
        nodeToAdd.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    m_workflow.nodes.append(nodeToAdd);
    addNodeItem(nodeToAdd);
    updateAllEdgeRoutes();
    m_scene->clearSelection();
    if (auto* item = m_nodeItems.value(nodeToAdd.nodeId, nullptr)) {
        item->setSelected(true);
    }
    emit workflowChanged(workflow());
}

void WorkflowCanvas::addStarterNodeAt(const QPointF& scenePos)
{
    addNode(createStarterNode(scenePos, StarterTemplateKind::DataOutput));
}

void WorkflowCanvas::addFunctionNodeAt(const QPointF& scenePos)
{
    addNode(createFunctionNode(scenePos, DataTransferTemplate::DataToData));
}

void WorkflowCanvas::addAgentNodeAt(const QPointF& scenePos)
{
    addNode(createAgentNode(scenePos, DataTransferTemplate::DataToData));
}

bool WorkflowCanvas::connectSelectedNodes()
{
    QList<NodeGraphicsItem*> selectedNodes;
    for (auto* item : m_scene->selectedItems()) {
        if (auto* nodeItem = dynamic_cast<NodeGraphicsItem*>(item)) {
            selectedNodes.append(nodeItem);
        }
    }

    if (selectedNodes.size() != 2) {
        return false;
    }

    return createEdgeBetween(selectedNodes.at(0)->nodeId(), selectedNodes.at(1)->nodeId());
}

bool WorkflowCanvas::createEdgeBetween(const QString& sourceNodeId, const QString& targetNodeId)
{
    auto* sourceItem = m_nodeItems.value(sourceNodeId, nullptr);
    auto* targetItem = m_nodeItems.value(targetNodeId, nullptr);
    if (sourceItem == nullptr || targetItem == nullptr || sourceNodeId == targetNodeId) {
        return false;
    }

    const auto fromNode = sourceItem->node();
    const auto toNode = targetItem->node();
    if (fromNode.outputPorts.isEmpty() || toNode.inputPorts.isEmpty()) {
        return false;
    }

    pushUndoState();

    domain::Edge edge;
    edge.edgeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    edge.fromNode = fromNode.nodeId;
    edge.fromPort = fromNode.outputPorts.first();
    edge.toNode = toNode.nodeId;
    edge.toPort = toNode.inputPorts.first();

    m_workflow.edges.append(edge);
    addEdgeItem(edge);
    emit workflowChanged(workflow());
    return true;
}

void WorkflowCanvas::clearWorkflow()
{
    pushUndoState();
    m_workflow = {};
    m_undoStack.clear();
    rebuildSceneFromWorkflow();
    emit workflowChanged(m_workflow);
}

bool WorkflowCanvas::updateNode(const domain::Node& node)
{
    auto* item = m_nodeItems.value(node.nodeId, nullptr);
    if (item == nullptr) {
        return false;
    }

    for (auto& workflowNode : m_workflow.nodes) {
        if (workflowNode.nodeId != node.nodeId) {
            continue;
        }

        // 只替换这个节点的数据，不重建整张 QGraphicsScene。
        // 代码编辑器保存时只改变 config.code；整场景重建会删除图元，
        // 容易和 Qt 当前正在分发的选择/鼠标/对话框信号交错，导致闪退。
        pushUndoState();
        workflowNode = node;

        // 保存代码只改变节点数据，不应该触发选择变化或拖线状态变化。
        // 这里显式屏蔽 scene 信号，避免 Qt 在图元刷新过程中同步分发 selectionChanged。
        const QSignalBlocker sceneBlocker(m_scene);
        item->setNode(node);
        updateAllEdgeRoutes();
        return true;
    }

    return false;
}

void WorkflowCanvas::setNodeStatus(const QString& nodeId, const QString& status)
{
    auto* nodeItem = m_nodeItems.value(nodeId, nullptr);
    if (nodeItem == nullptr) {
        return;
    }

    nodeItem->setVisualState(visualStateFromStatus(status));
}

void WorkflowCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    const auto scenePos = mapToScene(event->pos());

    QMenu menu(this);
    auto* starterMenu = menu.addMenu(tr("Add Starter Node"));
    auto* addStarterEmptyAction = starterMenu->addAction(tr("Empty Output"));
    auto* addStarterDataAction = starterMenu->addAction(tr("Business Data Output"));
    auto* addStarterFileAction = starterMenu->addAction(tr("File Output"));

    auto* functionMenu = menu.addMenu(tr("Add Function Node"));
    auto* addFunctionDataToDataAction = functionMenu->addAction(tr("Data to Data"));
    auto* addFunctionDataToFileAction = functionMenu->addAction(tr("Data to File"));
    auto* addFunctionFileToDataAction = functionMenu->addAction(tr("File to Data"));
    auto* addFunctionFileToFileAction = functionMenu->addAction(tr("File to File"));

    auto* agentMenu = menu.addMenu(tr("Add Agent Node"));
    auto* addAgentDataToDataAction = agentMenu->addAction(tr("Data to Data"));
    auto* addAgentDataToFileAction = agentMenu->addAction(tr("Data to File"));
    auto* addAgentFileToDataAction = agentMenu->addAction(tr("File to Data"));
    auto* addAgentFileToFileAction = agentMenu->addAction(tr("File to File"));
    menu.addSeparator();

    auto* connectAction = menu.addAction(tr("Connect Selected Nodes"));
    connectAction->setEnabled(m_scene->selectedItems().size() == 2);

    auto* deleteAction = menu.addAction(tr("Delete Selected"));
    deleteAction->setEnabled(!m_scene->selectedItems().isEmpty());

    const auto* selectedAction = menu.exec(event->globalPos());
    if (selectedAction == addStarterEmptyAction) {
        addNode(createStarterNode(scenePos, StarterTemplateKind::EmptyOutput));
        return;
    }
    if (selectedAction == addStarterDataAction) {
        addNode(createStarterNode(scenePos, StarterTemplateKind::DataOutput));
        return;
    }
    if (selectedAction == addStarterFileAction) {
        addNode(createStarterNode(scenePos, StarterTemplateKind::FileOutput));
        return;
    }
    if (selectedAction == addFunctionDataToDataAction) {
        addNode(createFunctionNode(scenePos, DataTransferTemplate::DataToData));
        return;
    }
    if (selectedAction == addFunctionDataToFileAction) {
        addNode(createFunctionNode(scenePos, DataTransferTemplate::DataToFile));
        return;
    }
    if (selectedAction == addFunctionFileToDataAction) {
        addNode(createFunctionNode(scenePos, DataTransferTemplate::FileToData));
        return;
    }
    if (selectedAction == addFunctionFileToFileAction) {
        addNode(createFunctionNode(scenePos, DataTransferTemplate::FileToFile));
        return;
    }
    if (selectedAction == addAgentDataToDataAction) {
        addNode(createAgentNode(scenePos, DataTransferTemplate::DataToData));
        return;
    }
    if (selectedAction == addAgentDataToFileAction) {
        addNode(createAgentNode(scenePos, DataTransferTemplate::DataToFile));
        return;
    }
    if (selectedAction == addAgentFileToDataAction) {
        addNode(createAgentNode(scenePos, DataTransferTemplate::FileToData));
        return;
    }
    if (selectedAction == addAgentFileToFileAction) {
        addNode(createAgentNode(scenePos, DataTransferTemplate::FileToFile));
        return;
    }
    if (selectedAction == connectAction) {
        connectSelectedNodes();
        return;
    }
    if (selectedAction == deleteAction) {
        deleteSelectedItems();
        return;
    }
}

void WorkflowCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Save)) {
        emit saveRequested();
        event->accept();
        return;
    }

    if (event->matches(QKeySequence::Undo)) {
        undoLastChange();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelectedItems();
        event->accept();
        return;
    }

    QGraphicsView::keyPressEvent(event);
}

void WorkflowCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        m_rightButtonPanning = true;
        m_lastPanViewportPos = event->pos();
        m_previousCursor = cursor().shape();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const auto scenePos = mapToScene(event->pos());
        if (auto* sourceNode = outputNodeAt(scenePos)) {
            // 从输出端口按下时进入“拖线”模式。
            // 此时不把事件继续交给节点图元，避免节点被拖动。
            m_edgeDragSource = sourceNode;
            m_edgeDragStartScenePos = sourceNode->outputAnchorScenePos();
            m_edgePreviewItem = new QGraphicsPathItem();
            m_edgePreviewItem->setZValue(0.5);
            auto* tm = ThemeManager::instance();
            m_edgePreviewItem->setPen(QPen(
                tm ? tm->color("edge-preview") : QColor("#2563eb"),
                2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            m_edgePreviewItem->setPath(edgePreviewPath(m_edgeDragStartScenePos, scenePos));
            m_scene->addItem(m_edgePreviewItem);
            event->accept();
            return;
        }

        if (dynamic_cast<NodeGraphicsItem*>(itemAt(event->pos())) != nullptr) {
            pushUndoState();
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void WorkflowCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_rightButtonPanning) {
        const auto delta = event->pos() - m_lastPanViewportPos;
        m_lastPanViewportPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }

    if (m_edgeDragSource != nullptr && m_edgePreviewItem != nullptr) {
        m_edgePreviewItem->setPath(edgePreviewPath(m_edgeDragStartScenePos, mapToScene(event->pos())));
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void WorkflowCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton && m_rightButtonPanning) {
        m_rightButtonPanning = false;
        setCursor(m_previousCursor);
        event->accept();
        return;
    }

    if (m_edgeDragSource != nullptr) {
        const auto scenePos = mapToScene(event->pos());
        if (auto* targetNode = inputNodeAt(scenePos, m_edgeDragSource->nodeId())) {
            createEdgeBetween(m_edgeDragSource->nodeId(), targetNode->nodeId());
        }

        if (m_edgePreviewItem != nullptr) {
            m_scene->removeItem(m_edgePreviewItem);
            delete m_edgePreviewItem;
            m_edgePreviewItem = nullptr;
        }
        m_edgeDragSource = nullptr;
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void WorkflowCanvas::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        zoomAtCursor(event->angleDelta().y());
        event->accept();
        return;
    }

    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        const auto delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->angleDelta().x();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta);
        event->accept();
        return;
    }

    QGraphicsView::wheelEvent(event);
}

void WorkflowCanvas::buildScene()
{
    m_scene->setSceneRect(-2000, -2000, 4000, 4000);
    auto* tm = ThemeManager::instance();
    m_scene->setBackgroundBrush(tm ? tm->color("canvas-bg") : QColor("#f7f8fa"));
}

void WorkflowCanvas::rebuildSceneFromWorkflow()
{
    clearEdgeDragState();
    m_nodeItems.clear();
    m_edgeItems.clear();
    m_scene->clear();
    buildScene();

    for (const auto& node : m_workflow.nodes) {
        addNodeItem(node);
    }

    for (const auto& edge : m_workflow.edges) {
        addEdgeItem(edge);
    }
}

void WorkflowCanvas::addNodeItem(const domain::Node& node)
{
    auto* item = new NodeGraphicsItem(node);
    m_scene->addItem(item);
    m_nodeItems.insert(node.nodeId, item);

    connect(item, &NodeGraphicsItem::nodeMoved, this, [this](const QString& nodeId, const QPointF&) {
        updateEdgesForNode(nodeId);
        syncWorkflowFromItems();
        emit workflowChanged(m_workflow);
    });
    connect(item, &NodeGraphicsItem::nodeSelected, this, [this](const domain::Node& selectedNode) {
        emit nodeSelected(selectedNode);
    });
    connect(item, &NodeGraphicsItem::nodeDoubleClicked, this, [this](const domain::Node& node) {
        emit nodeDoubleClicked(node);
    });
}

void WorkflowCanvas::addEdgeItem(const domain::Edge& edge)
{
    auto* sourceNode = m_nodeItems.value(edge.fromNode, nullptr);
    auto* targetNode = m_nodeItems.value(edge.toNode, nullptr);
    if (sourceNode == nullptr || targetNode == nullptr) {
        return;
    }

    auto* item = new EdgeGraphicsItem(edge, sourceNode, targetNode);
    m_scene->addItem(item);
    m_edgeItems.insert(edge.edgeId, item);
    item->setRoutingContext(nodeObstacleRectsForEdge(edge), parallelEdgeIndex(edge));
}

void WorkflowCanvas::updateEdgesForNode(const QString& nodeId)
{
    Q_UNUSED(nodeId);
    updateAllEdgeRoutes();
}

void WorkflowCanvas::updateAllEdgeRoutes()
{
    for (auto* edgeItem : m_edgeItems) {
        const auto edge = edgeItem->edge();
        edgeItem->setRoutingContext(nodeObstacleRectsForEdge(edge), parallelEdgeIndex(edge));
    }
}

QList<QRectF> WorkflowCanvas::nodeObstacleRectsForEdge(const domain::Edge& edge) const
{
    QList<QRectF> rects;
    for (auto* nodeItem : m_nodeItems) {
        if (nodeItem == nullptr) {
            continue;
        }
        const auto nodeId = nodeItem->nodeId();
        if (nodeId == edge.fromNode || nodeId == edge.toNode) {
            continue;
        }
        rects.append(nodeItem->bodySceneRect());
    }
    return rects;
}

int WorkflowCanvas::parallelEdgeIndex(const domain::Edge& edge) const
{
    int index = 0;
    for (const auto& existingEdge : m_workflow.edges) {
        if (existingEdge.edgeId == edge.edgeId) {
            return index;
        }
        if (existingEdge.fromNode == edge.fromNode && existingEdge.toNode == edge.toNode) {
            ++index;
        }
    }
    return index;
}

void WorkflowCanvas::clearEdgeDragState()
{
    m_edgeDragSource = nullptr;
    if (m_edgePreviewItem == nullptr) {
        return;
    }

    if (m_edgePreviewItem->scene() == m_scene) {
        m_scene->removeItem(m_edgePreviewItem);
    }
    delete m_edgePreviewItem;
    m_edgePreviewItem = nullptr;
}

void WorkflowCanvas::deleteSelectedItems()
{
    const auto selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    pushUndoState();
    QStringList nodeIds;
    QStringList edgeIds;

    for (auto* item : selected) {
        if (auto* nodeItem = dynamic_cast<NodeGraphicsItem*>(item)) {
            nodeIds.append(nodeItem->nodeId());
        } else if (auto* edgeItem = dynamic_cast<EdgeGraphicsItem*>(item)) {
            edgeIds.append(edgeItem->edgeId());
        }
    }

    for (const auto& nodeId : nodeIds) {
        removeNode(nodeId);
    }
    for (const auto& edgeId : edgeIds) {
        removeEdge(edgeId);
    }

    syncWorkflowFromItems();
    emit workflowChanged(m_workflow);
}

void WorkflowCanvas::removeEdge(const QString& edgeId)
{
    auto* item = m_edgeItems.take(edgeId);
    if (item != nullptr) {
        m_scene->removeItem(item);
        delete item;
    }

    for (qsizetype index = m_workflow.edges.size() - 1; index >= 0; --index) {
        if (m_workflow.edges.at(index).edgeId == edgeId) {
            m_workflow.edges.removeAt(index);
        }
    }
}

void WorkflowCanvas::removeNode(const QString& nodeId)
{
    if (m_edgeDragSource != nullptr && m_edgeDragSource->nodeId() == nodeId) {
        clearEdgeDragState();
    }

    QStringList connectedEdges;
    for (const auto& edgeId : m_edgeItems.keys()) {
        if (m_edgeItems.value(edgeId)->touchesNode(nodeId)) {
            connectedEdges.append(edgeId);
        }
    }

    for (const auto& edgeId : connectedEdges) {
        removeEdge(edgeId);
    }

    auto* item = m_nodeItems.take(nodeId);
    if (item != nullptr) {
        m_scene->removeItem(item);
        delete item;
    }

    for (qsizetype index = m_workflow.nodes.size() - 1; index >= 0; --index) {
        if (m_workflow.nodes.at(index).nodeId == nodeId) {
            m_workflow.nodes.removeAt(index);
        }
    }
    updateAllEdgeRoutes();
}

void WorkflowCanvas::syncWorkflowFromItems()
{
    for (auto& node : m_workflow.nodes) {
        auto* item = m_nodeItems.value(node.nodeId, nullptr);
        if (item != nullptr) {
            node = item->node();
        }
    }
}

void WorkflowCanvas::pushUndoState()
{
    if (m_restoringHistory) {
        return;
    }

    syncWorkflowFromItems();
    m_undoStack.append(m_workflow);
    constexpr int MaxUndoStates = 50;
    while (m_undoStack.size() > MaxUndoStates) {
        m_undoStack.removeFirst();
    }
}

void WorkflowCanvas::undoLastChange()
{
    if (m_undoStack.isEmpty()) {
        return;
    }

    m_restoringHistory = true;
    m_workflow = m_undoStack.takeLast();
    rebuildSceneFromWorkflow();
    m_restoringHistory = false;
    emit workflowChanged(m_workflow);
}

void WorkflowCanvas::zoomAtCursor(int wheelDelta)
{
    if (wheelDelta == 0) {
        return;
    }

    const auto currentZoom = transform().m11();
    const auto factor = wheelDelta > 0 ? ZoomStep : 1.0 / ZoomStep;
    const auto nextZoom = currentZoom * factor;
    if (nextZoom < MinZoom || nextZoom > MaxZoom) {
        return;
    }

    scale(factor, factor);
}

domain::Node WorkflowCanvas::createFunctionNode(const QPointF& scenePos, DataTransferTemplate templateKind) const
{
    domain::Node node;
    node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    node.type = "function";
    node.name = tr("Function Node %1").arg(m_workflow.nodes.size() + 1);
    node.description = tr("Python function node created from the canvas context menu.");
    node.position.x = scenePos.x();
    node.position.y = scenePos.y();
    node.inputPorts = {"input"};
    node.outputPorts = {"output"};
    node.config = {
        {"language", "python"},
        {"entry", "run"},
        {"io_template", PythonCodeTemplates::templateKey(templateKind)},
        {"code", PythonCodeTemplates::codeForTemplate(templateKind)},
    };
    return node;
}

domain::Node WorkflowCanvas::createStarterNode(const QPointF& scenePos, StarterTemplateKind templateKind) const
{
    domain::Node node;
    node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    node.type = "starter";
    node.name = tr("Starter Node %1").arg(m_workflow.nodes.size() + 1);
    node.description = tr("Output-only starter node created from the canvas context menu.");
    node.position.x = scenePos.x();
    node.position.y = scenePos.y();
    node.inputPorts = {};
    node.outputPorts = {"output"};
    DataTransferTemplate codeTemplate = DataTransferTemplate::DataOutput;
    if (templateKind == StarterTemplateKind::EmptyOutput) {
        codeTemplate = DataTransferTemplate::EmptyOutput;
    } else if (templateKind == StarterTemplateKind::FileOutput) {
        codeTemplate = DataTransferTemplate::FileOutput;
    }

    node.config = {
        {"language", "python"},
        {"entry", "run"},
        {"io_template", PythonCodeTemplates::templateKey(codeTemplate)},
        {"code", PythonCodeTemplates::codeForTemplate(codeTemplate)},
    };
    return node;
}

domain::Node WorkflowCanvas::createAgentNode(const QPointF& scenePos, DataTransferTemplate templateKind) const
{
    domain::Node node;
    node.nodeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    node.type = "agent";
    node.name = tr("Agent Node %1").arg(m_workflow.nodes.size() + 1);
    node.description = tr("Agent node created from the canvas context menu.");
    node.position.x = scenePos.x();
    node.position.y = scenePos.y();
    node.inputPorts = {"input"};
    node.outputPorts = {"output"};
    node.config = {
        {"language", "python"},
        {"entry", "run"},
        {"agent_url", PythonCodeTemplates::defaultAgentUrl()},
        {"agent_model", PythonCodeTemplates::defaultAgentModel()},
        {"agent_api_key", ""},
        {"agent_background_prompt", PythonCodeTemplates::defaultAgentBackgroundPrompt()},
        {"agent_task_prompt", PythonCodeTemplates::defaultAgentTaskPrompt()},
        {"io_template", PythonCodeTemplates::templateKey(templateKind)},
        {"code", PythonCodeTemplates::agentCode(
                PythonCodeTemplates::defaultAgentUrl(),
                PythonCodeTemplates::defaultAgentModel(),
                QString(),
                PythonCodeTemplates::defaultAgentBackgroundPrompt(),
                PythonCodeTemplates::defaultAgentTaskPrompt(),
                templateKind)},
    };
    return node;
}

NodeGraphicsItem* WorkflowCanvas::outputNodeAt(const QPointF& scenePos) const
{
    for (auto* nodeItem : m_nodeItems) {
        if (nodeItem->node().outputPorts.isEmpty()) {
            continue;
        }
        if (QLineF(scenePos, nodeItem->outputAnchorScenePos()).length() <= PortHitRadius) {
            return nodeItem;
        }
    }
    return nullptr;
}

NodeGraphicsItem* WorkflowCanvas::inputNodeAt(const QPointF& scenePos, const QString& excludedNodeId) const
{
    for (auto* nodeItem : m_nodeItems) {
        if (!excludedNodeId.isEmpty() && nodeItem->nodeId() == excludedNodeId) {
            continue;
        }
        if (nodeItem->node().inputPorts.isEmpty()) {
            continue;
        }
        if (QLineF(scenePos, nodeItem->inputAnchorScenePos()).length() <= PortHitRadius) {
            return nodeItem;
        }
    }
    return nullptr;
}

QPainterPath WorkflowCanvas::edgePreviewPath(const QPointF& start, const QPointF& end) const
{
    const auto dx = qMax<qreal>(80.0, qAbs(end.x() - start.x()) * 0.45);
    QPainterPath path(start);
    path.cubicTo(
        QPointF(start.x() + dx, start.y()),
        QPointF(end.x() - dx, end.y()),
        end);
    return path;
}

NodeVisualState WorkflowCanvas::visualStateFromStatus(const QString& status) const
{
    const auto normalized = status.trimmed().toLower();
    if (normalized == "pending" || normalized == "waiting") {
        return NodeVisualState::Pending;
    }
    if (normalized == "queued") {
        return NodeVisualState::Queued;
    }
    if (normalized == "running") {
        return NodeVisualState::Running;
    }
    if (normalized == "succeeded" || normalized == "success") {
        return NodeVisualState::Succeeded;
    }
    if (normalized == "failed" || normalized == "failure") {
        return NodeVisualState::Failed;
    }
    if (normalized == "timeout" || normalized == "skipped" || normalized == "cancelled") {
        return NodeVisualState::Failed;
    }
    return NodeVisualState::Idle;
}

void WorkflowCanvas::refreshTheme()
{
    auto* tm = ThemeManager::instance();
    if (m_scene != nullptr) {
        m_scene->setBackgroundBrush(tm ? tm->color("canvas-bg") : QColor("#F7F8FB"));
        for (auto* item : m_scene->items()) {
            item->update();
        }
    }

    if (m_edgePreviewItem != nullptr) {
        m_edgePreviewItem->setPen(QPen(
            tm ? tm->color("edge-preview") : QColor("#2563EB"),
            2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    }

    viewport()->update();
}

} // namespace vws::ui
