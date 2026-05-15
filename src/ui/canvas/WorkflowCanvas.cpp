#include "ui/canvas/WorkflowCanvas.h"

#include "application/NodeFactory.h"
#include "application/WorkflowEditService.h"
#include "ui/canvas/EdgeGraphicsItem.h"
#include "ui/canvas/WorkflowCanvasContextMenu.h"
#include "ui/canvas/WorkflowSceneController.h"
#include "ui/theme/ThemeManager.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFrame>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QWheelEvent>

namespace vws::ui {

using application::DataTransferTemplate;
using StarterTemplateKind = application::NodeFactory::StarterTemplateKind;

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

void WorkflowCanvas::addStarterNodeAt(const QPointF& scenePos)
{
    addNode(application::NodeFactory::createStarterNode(
        scenePos,
        m_workflow.nodes.size(),
        StarterTemplateKind::DataOutput));
}

void WorkflowCanvas::addFunctionNodeAt(const QPointF& scenePos)
{
    addNode(application::NodeFactory::createFunctionNode(
        scenePos,
        m_workflow.nodes.size(),
        DataTransferTemplate::DataToData));
}

void WorkflowCanvas::addAgentNodeAt(const QPointF& scenePos)
{
    addNode(application::NodeFactory::createAgentNode(
        scenePos,
        m_workflow.nodes.size(),
        DataTransferTemplate::DataToData));
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
    auto* sourceItem = m_sceneController != nullptr ? m_sceneController->nodeItem(sourceNodeId) : nullptr;
    auto* targetItem = m_sceneController != nullptr ? m_sceneController->nodeItem(targetNodeId) : nullptr;
    if (sourceItem == nullptr || targetItem == nullptr || sourceNodeId == targetNodeId) {
        return false;
    }
    if (sourceItem->node().outputPorts.isEmpty() || targetItem->node().inputPorts.isEmpty()) {
        return false;
    }

    pushUndoState();

    domain::Edge edge;
    if (!application::WorkflowEditService::connectNodes(
            m_workflow,
            sourceItem->node(),
            targetItem->node(),
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
    updateAllEdgeRoutes();
    return true;
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
    const auto action = WorkflowCanvasContextMenu::exec(
        this,
        event->globalPos(),
        m_scene->selectedItems().size() == 2,
        !m_scene->selectedItems().isEmpty());

    switch (action.type) {
    case WorkflowCanvasContextAction::Type::AddStarter:
        addNode(application::NodeFactory::createStarterNode(scenePos, m_workflow.nodes.size(), action.starterTemplate));
        return;
    case WorkflowCanvasContextAction::Type::AddFunction:
        addNode(application::NodeFactory::createFunctionNode(scenePos, m_workflow.nodes.size(), action.dataTransferTemplate));
        return;
    case WorkflowCanvasContextAction::Type::AddAgent:
        addNode(application::NodeFactory::createAgentNode(scenePos, m_workflow.nodes.size(), action.dataTransferTemplate));
        return;
    case WorkflowCanvasContextAction::Type::ConnectSelected:
        connectSelectedNodes();
        return;
    case WorkflowCanvasContextAction::Type::DeleteSelected:
        deleteSelectedItems();
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
    // Rebuilding deletes every QGraphicsItem through QGraphicsScene::clear().
    // Block scene signals during the rebuild so selectionChanged cannot fire
    // while the item maps are intentionally empty.
    const QSignalBlocker sceneBlocker(m_scene);
    clearEdgeDragState();
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
    if (m_sceneController != nullptr) {
        m_sceneController->removeEdgeItem(edgeId);
    }

    application::WorkflowEditService::removeEdges(m_workflow, QSet<QString>{edgeId});
}

void WorkflowCanvas::removeNode(const QString& nodeId)
{
    if (m_edgeDragSource != nullptr && m_edgeDragSource->nodeId() == nodeId) {
        clearEdgeDragState();
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
        if (m_sceneController != nullptr) {
            m_sceneController->refreshItems();
        }
    }

    if (m_edgePreviewItem != nullptr) {
        m_edgePreviewItem->setPen(QPen(
            tm ? tm->color("edge-preview") : QColor("#2563EB"),
            2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    }

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

    const auto duplicate = m_clipboard.createPasteSubgraph(tr(" Copy"));

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
