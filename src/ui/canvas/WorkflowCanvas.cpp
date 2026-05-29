#include "ui/canvas/WorkflowCanvas.h"

#include "application/WorkflowEditService.h"
#include "domain/NodeTypes.h"
#include "ui/canvas/EdgeGraphicsItem.h"
#include "ui/canvas/NodePortSlotViewModelBuilder.h"
#include "ui/canvas/WorkflowCanvasContextMenu.h"
#include "ui/canvas/WorkflowSceneController.h"
#include "ui/theme/ThemeManager.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFrame>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSet>
#include <QSignalBlocker>
#include <QWheelEvent>
#include <algorithm>

namespace vws::ui {

namespace {

constexpr qreal PortHitRadius = 14.0;

} // namespace

WorkflowCanvas::WorkflowCanvas(QWidget* parent)
    : QGraphicsView(parent)
{
    setObjectName(QStringLiteral("workflowCanvas"));
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    m_sceneController = new WorkflowSceneController(m_scene, this);
    connect(m_sceneController, &WorkflowSceneController::nodeMoved, this, [this](const QString& nodeId) {
        updateEdgesForNode(nodeId);
        syncWorkflowFromItems();
        emit workflowChanged(m_workflow);
    });
    connect(m_sceneController, &WorkflowSceneController::nodeSelected, this, &WorkflowCanvas::nodeSelected);
    connect(m_sceneController, &WorkflowSceneController::nodeDoubleClicked, this, &WorkflowCanvas::nodeDoubleClicked);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]() {
        for (auto* item : m_scene->selectedItems()) {
            if (dynamic_cast<NodeGraphicsItem*>(item) != nullptr) {
                return;
            }
        }
        emit nodeSelectionCleared();
    });
    setFrameShape(QFrame::NoFrame);
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setAcceptDrops(true);
    buildScene();
}

WorkflowCanvas::~WorkflowCanvas()
{
    if (m_scene != nullptr) {
        QObject::disconnect(m_scene, nullptr, this, nullptr);
    }
}

void WorkflowCanvas::setWorkflow(const domain::Workflow& workflow)
{
    m_workflow = workflow;
    m_history.clear();
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
    if (m_sceneController != nullptr) {
        m_sceneController->syncWorkflowNodes(workflow);
    }
    return workflow;
}

application::WorkflowHistory WorkflowCanvas::history() const
{
    return m_history;
}

void WorkflowCanvas::setHistory(const application::WorkflowHistory& history)
{
    m_history = history;
}

std::optional<domain::Node> WorkflowCanvas::selectedNode() const
{
    return m_sceneController != nullptr ? m_sceneController->selectedNode() : std::nullopt;
}

void WorkflowCanvas::addNode(const domain::Node& node)
{
    pushUndoState();
    const auto nodeToAdd = application::WorkflowEditService::addNode(m_workflow, node);
    addNodeItem(nodeToAdd);
    updateAllEdgeRoutes();
    if (m_sceneController != nullptr) {
        m_sceneController->clearSelection();
        m_sceneController->selectNode(nodeToAdd.nodeId);
    }
    emit workflowChanged(workflow());
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
    std::sort(selectedNodes.begin(), selectedNodes.end(), [](const NodeGraphicsItem* left, const NodeGraphicsItem* right) {
        if (left == nullptr || right == nullptr) {
            return left != nullptr;
        }
        const auto leftCenter = left->sceneBoundingRect().center();
        const auto rightCenter = right->sceneBoundingRect().center();
        if (!qFuzzyCompare(leftCenter.x(), rightCenter.x())) {
            return leftCenter.x() < rightCenter.x();
        }
        return leftCenter.y() < rightCenter.y();
    });

    const auto* sourceItem = selectedNodes.at(0);
    const auto* targetItem = selectedNodes.at(1);
    const auto sourceNode = sourceItem->node();
    const auto targetNode = targetItem->node();
    if (sourceNode.outputPorts.isEmpty() || targetNode.inputPorts.isEmpty()) {
        return false;
    }

    return createEdgeBetween(
        domain::EdgeEndpoint{sourceNode.nodeId, sourceNode.outputPorts.first(), 0},
        domain::EdgeEndpoint{targetNode.nodeId, targetNode.inputPorts.first(), 0});
}

bool WorkflowCanvas::createEdgeBetween(const domain::EdgeEndpoint& source, const domain::EdgeEndpoint& target)
{
    if (!application::WorkflowEditService::canConnect(m_workflow, source, target)) {
        return false;
    }

    pushUndoState();

    domain::Edge edge;
    if (!application::WorkflowEditService::connectNodes(
            m_workflow,
            source,
            target,
            edge)) {
        return false;
    }
    addEdgeItem(edge);
    emit workflowChanged(workflow());
    return true;
}

void WorkflowCanvas::clearWorkflow()
{
    pushUndoState();
    m_workflow = {};
    m_history.clear();
    rebuildSceneFromWorkflow();
    emit workflowChanged(m_workflow);
}

bool WorkflowCanvas::updateNode(const domain::Node& node)
{
    auto* item = m_sceneController != nullptr ? m_sceneController->nodeItem(node.nodeId) : nullptr;
    if (item == nullptr) {
        return false;
    }

    // 只替换这个节点的数据，不重建整张 QGraphicsScene。
    // 代码编辑器保存时只改变 config.code；整场景重建会删除图元，
    // 容易和 Qt 当前正在分发的选择/鼠标/对话框信号交错，导致闪退。
    pushUndoState();
    if (!application::WorkflowEditService::updateNode(m_workflow, node)) {
        return false;
    }

    // 保存代码只改变节点数据，不应该触发选择变化或拖线状态变化。
    // 这里显式屏蔽 scene 信号，避免 Qt 在图元刷新过程中同步分发 selectionChanged。
    const QSignalBlocker sceneBlocker(m_scene);
    item->setNode(node);
    setNodeIoSpec(node.nodeId, node.ioSpec);
    updateAllEdgeRoutes();
    return true;
}

void WorkflowCanvas::setNodeIoSpec(const QString& nodeId, const domain::NodeIoSpec& spec)
{
    if (m_sceneController == nullptr) {
        return;
    }

    for (auto& node : m_workflow.nodes) {
        if (node.nodeId != nodeId) {
            continue;
        }
        const auto portSlots = NodePortSlotViewModelBuilder().build(node, spec);
        m_sceneController->setNodePortSlots(nodeId, portSlots.inputs, portSlots.outputs);
        updateAllEdgeRoutes();
        return;
    }
}

void WorkflowCanvas::applyRuntimeIoSpecs(const QHash<QString, domain::NodeIoSpec>& specsByNodeId)
{
    for (auto it = specsByNodeId.constBegin(); it != specsByNodeId.constEnd(); ++it) {
        setNodeIoSpec(it.key(), it.value());
    }
}

void WorkflowCanvas::setNodeStatus(const QString& nodeId, const QString& status)
{
    if (m_sceneController == nullptr) {
        return;
    }

    m_sceneController->setNodeStatus(nodeId, visualStateFromStatus(status));
}

void WorkflowCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    const auto scenePos = mapToScene(event->pos());
    const auto selectedNodes = selectedNodeItems();
    const auto canRetitleSubsystem = selectedNodes.size() == 1
        && selectedNodes.first()->node().type.trimmed().toLower() == domain::NodeTypes::Subsystem;
    const auto action = WorkflowCanvasContextMenu::exec(
        this,
        event->globalPos(),
        m_scene->selectedItems().size() == 2,
        !m_scene->selectedItems().isEmpty(),
        selectedNodes.size() == 1 && m_scene->selectedItems().size() == 1,
        canRetitleSubsystem);

    switch (action.type) {
    case WorkflowCanvasContextAction::Type::AddStarter:
        emit starterNodeRequested(scenePos, action.starterTemplate);
        return;
    case WorkflowCanvasContextAction::Type::AddFunction:
        emit functionNodeRequested(scenePos, action.dataTransferTemplate);
        return;
    case WorkflowCanvasContextAction::Type::AddAgent:
        emit agentNodeRequested(scenePos, action.dataTransferTemplate);
        return;
    case WorkflowCanvasContextAction::Type::AddSubsystem:
        emit subsystemNodeRequested(scenePos);
        return;
    case WorkflowCanvasContextAction::Type::AddLoop:
        emit loopNodeRequested(scenePos);
        return;
    case WorkflowCanvasContextAction::Type::ConnectSelected:
        connectSelectedNodes();
        return;
    case WorkflowCanvasContextAction::Type::DeleteSelected:
        deleteSelectedItems();
        return;
    case WorkflowCanvasContextAction::Type::RotateSelected:
        rotateSelectedNode(action.rotationDeltaDegrees);
        return;
    case WorkflowCanvasContextAction::Type::RetitleSubsystem:
        if (canRetitleSubsystem) {
            emit subsystemNodeRetitleRequested(selectedNodes.first()->node());
        }
        return;
    case WorkflowCanvasContextAction::Type::None:
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

    if (event->matches(QKeySequence::Copy)) {
        copySelectedNodes();
        event->accept();
        return;
    }

    if (event->matches(QKeySequence::Cut)) {
        cutSelectedNodes();
        event->accept();
        return;
    }

    if (event->matches(QKeySequence::Paste)) {
        pasteClipboardNodes();
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
        m_interactionController.beginRightButtonPan(*this, event->pos());
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const auto scenePos = mapToScene(event->pos());
        if (auto* nodeItem = dynamic_cast<NodeGraphicsItem*>(itemAt(event->pos()));
                nodeItem != nullptr && nodeItem->isSelected() && nodeItem->hasResizeHandleAt(scenePos)) {
            pushUndoState();
            QGraphicsView::mousePressEvent(event);
            return;
        }

        if (auto sourceHit = outputSlotAt(scenePos); sourceHit.has_value()) {
            // 从输出端口按下时进入“拖线”模式。
            // 此时不把事件继续交给节点图元，避免节点被拖动。
            const auto* sourceNode = m_sceneController != nullptr
                ? m_sceneController->nodeItem(sourceHit->nodeId)
                : nullptr;
            const auto startPos = sourceNode != nullptr
                ? sourceNode->outputAnchorScenePos(sourceHit->portName, sourceHit->slotIndex)
                : scenePos;
            m_edgeDragController.begin(m_scene, sourceHit->toEndpoint(), startPos);
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
    if (m_interactionController.updateRightButtonPan(*this, event->pos())) {
        event->accept();
        return;
    }

    if (m_edgeDragController.isDragging()) {
        m_edgeDragController.update(mapToScene(event->pos()));
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void WorkflowCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton && m_interactionController.endRightButtonPan(*this)) {
        event->accept();
        return;
    }

    if (m_edgeDragController.isDragging()) {
        const auto scenePos = mapToScene(event->pos());
        const auto source = m_edgeDragController.sourceEndpoint();
        if (auto targetHit = inputSlotAt(scenePos, source.nodeId); targetHit.has_value()) {
            createEdgeBetween(source, targetHit->toEndpoint());
        }

        m_edgeDragController.clear(m_scene);
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void WorkflowCanvas::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        m_interactionController.zoomAtCursor(*this, event->angleDelta().y());
        event->accept();
        return;
    }

    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        const auto delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->angleDelta().x();
        m_interactionController.panHorizontally(*this, delta);
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
    // Rebuilding deletes every QGraphicsItem through QGraphicsScene::clear().
    // Block scene signals during the rebuild so selectionChanged cannot fire
    // while the item maps are intentionally empty.
    const QSignalBlocker sceneBlocker(m_scene);
    m_edgeDragController.clear(m_scene);
    if (m_sceneController != nullptr) {
        m_sceneController->clear();
    }
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
    if (m_sceneController != nullptr) {
        m_sceneController->addNodeItem(node);
        const auto portSlots = NodePortSlotViewModelBuilder().build(node, node.ioSpec);
        m_sceneController->setNodePortSlots(node.nodeId, portSlots.inputs, portSlots.outputs);
    }
}

void WorkflowCanvas::addEdgeItem(const domain::Edge& edge)
{
    if (m_sceneController != nullptr) {
        m_sceneController->addEdgeItem(edge, m_workflow);
    }
}

void WorkflowCanvas::updateEdgesForNode(const QString& nodeId)
{
    Q_UNUSED(nodeId);
    updateAllEdgeRoutes();
}

void WorkflowCanvas::updateAllEdgeRoutes()
{
    if (m_sceneController != nullptr) {
        m_sceneController->updateAllEdgeRoutes(m_workflow);
    }
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
    if (m_sceneController != nullptr) {
        m_sceneController->removeEdgeItem(edgeId);
    }

    application::WorkflowEditService::removeEdges(m_workflow, QSet<QString>{edgeId});
}

void WorkflowCanvas::removeNode(const QString& nodeId)
{
    if (m_edgeDragController.sourceIs(nodeId)) {
        m_edgeDragController.clear(m_scene);
    }

    const auto connectedEdges = m_sceneController != nullptr
        ? m_sceneController->connectedEdgeIdsForNode(nodeId)
        : QStringList();

    for (const auto& edgeId : connectedEdges) {
        removeEdge(edgeId);
    }

    if (m_sceneController != nullptr) {
        m_sceneController->removeNodeItem(nodeId);
    }

    application::WorkflowEditService::removeNodes(m_workflow, QSet<QString>{nodeId});
    updateAllEdgeRoutes();
}

bool WorkflowCanvas::rotateSelectedNode(int deltaDegrees)
{
    const auto selectedNodes = selectedNodeItems();
    if (selectedNodes.size() != 1) {
        return false;
    }
    return rotateNode(selectedNodes.first()->nodeId(), deltaDegrees);
}

bool WorkflowCanvas::rotateNode(const QString& nodeId, int deltaDegrees)
{
    auto* item = m_sceneController != nullptr ? m_sceneController->nodeItem(nodeId) : nullptr;
    if (item == nullptr) {
        return false;
    }

    pushUndoState();
    if (!application::WorkflowEditService::rotateNode(m_workflow, nodeId, deltaDegrees)) {
        return false;
    }

    for (const auto& node : m_workflow.nodes) {
        if (node.nodeId == nodeId) {
            item->setNode(node);
            item->setSelected(true);
            updateAllEdgeRoutes();
            emit workflowChanged(workflow());
            return true;
        }
    }

    return false;
}

void WorkflowCanvas::syncWorkflowFromItems()
{
    if (m_sceneController != nullptr) {
        m_sceneController->syncWorkflowNodes(m_workflow);
    }
}

void WorkflowCanvas::pushUndoState()
{
    syncWorkflowFromItems();
    m_history.push(m_workflow);
}

void WorkflowCanvas::undoLastChange()
{
    const auto snapshot = m_history.takeUndoSnapshot();
    if (!snapshot.has_value()) {
        return;
    }

    m_history.setRestoring(true);
    m_workflow = snapshot.value();
    rebuildSceneFromWorkflow();
    m_history.setRestoring(false);
    emit workflowChanged(m_workflow);
}

NodeGraphicsItem* WorkflowCanvas::outputNodeAt(const QPointF& scenePos) const
{
    return m_sceneController != nullptr
        ? m_sceneController->outputNodeAt(scenePos, PortHitRadius)
        : nullptr;
}

NodeGraphicsItem* WorkflowCanvas::inputNodeAt(const QPointF& scenePos, const QString& excludedNodeId) const
{
    return m_sceneController != nullptr
        ? m_sceneController->inputNodeAt(scenePos, PortHitRadius, excludedNodeId)
        : nullptr;
}

std::optional<PortSlotHit> WorkflowCanvas::outputSlotAt(const QPointF& scenePos) const
{
    return m_sceneController != nullptr
        ? m_sceneController->outputSlotAt(scenePos, PortHitRadius)
        : std::nullopt;
}

std::optional<PortSlotHit> WorkflowCanvas::inputSlotAt(const QPointF& scenePos, const QString& excludedNodeId) const
{
    return m_sceneController != nullptr
        ? m_sceneController->inputSlotAt(scenePos, PortHitRadius, excludedNodeId)
        : std::nullopt;
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
        if (m_sceneController != nullptr) {
            m_sceneController->refreshItems();
        }
    }

    m_edgeDragController.refreshTheme();

    viewport()->update();
}

QList<NodeGraphicsItem*> WorkflowCanvas::selectedNodeItems() const
{
    return m_sceneController != nullptr ? m_sceneController->selectedNodeItems() : QList<NodeGraphicsItem*>();
}

void WorkflowCanvas::copySelectedNodes()
{
    const auto selectedNodes = selectedNodeItems();
    if (selectedNodes.isEmpty()) {
        return;
    }

    QSet<QString> selectedNodeIds;
    for (auto* item : selectedNodes) {
        selectedNodeIds.insert(item->nodeId());
    }

    m_clipboard.capture(m_workflow, selectedNodeIds);
}

void WorkflowCanvas::cutSelectedNodes()
{
    if (selectedNodeItems().isEmpty()) {
        return;
    }

    copySelectedNodes();
    deleteSelectedItems();
}

void WorkflowCanvas::pasteClipboardNodes()
{
    if (!m_clipboard.hasNodes()) {
        return;
    }

    pushUndoState();

    const auto duplicate = m_clipboard.createPasteSubgraph();

    if (m_sceneController != nullptr) {
        m_sceneController->clearSelection();
    }

    for (const auto& node : duplicate.nodes) {
        addNodeItem(node);
        if (m_sceneController != nullptr) {
            m_sceneController->selectNode(node.nodeId);
        }
    }

    application::WorkflowEditService::appendSubgraph(m_workflow, duplicate);

    for (const auto& edge : duplicate.edges) {
        addEdgeItem(edge);
    }

    updateAllEdgeRoutes();
    emit workflowChanged(workflow());
}

namespace {
constexpr const char* NodeTemplateMimeType = "application/x-vws-node-template-id";
}

void WorkflowCanvas::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(NodeTemplateMimeType)) {
        setDragMode(QGraphicsView::NoDrag);
        event->acceptProposedAction();
        return;
    }

    QGraphicsView::dragEnterEvent(event);
}

void WorkflowCanvas::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(NodeTemplateMimeType)) {
        event->acceptProposedAction();
        return;
    }

    QGraphicsView::dragMoveEvent(event);
}

void WorkflowCanvas::dragLeaveEvent(QDragLeaveEvent* event)
{
    setDragMode(QGraphicsView::RubberBandDrag);
    QGraphicsView::dragLeaveEvent(event);
}

void WorkflowCanvas::dropEvent(QDropEvent* event)
{
    setDragMode(QGraphicsView::RubberBandDrag);

    if (!event->mimeData()->hasFormat(NodeTemplateMimeType)) {
        QGraphicsView::dropEvent(event);
        return;
    }

    const auto templateId = QString::fromUtf8(
        event->mimeData()->data(NodeTemplateMimeType)).trimmed();

    if (templateId.isEmpty()) {
        event->ignore();
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const auto scenePos = mapToScene(event->position().toPoint());
#else
    const auto scenePos = mapToScene(event->pos());
#endif

    emit nodeTemplateDropped(templateId, scenePos);
    event->acceptProposedAction();
}

} // namespace vws::ui
