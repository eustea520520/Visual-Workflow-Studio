#pragma once

#include "application/WorkflowClipboard.h"
#include "application/WorkflowHistory.h"
#include "domain/Workflow.h"
#include "ui/canvas/NodeGraphicsItem.h"

#include <QGraphicsView>
#include <QList>
#include <optional>
#include <QPointF>
#include <QString>

class QGraphicsScene;
class QContextMenuEvent;
class QGraphicsPathItem;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace vws::ui {

class WorkflowSceneController;

// 中央工作流画布。
//
// WorkflowCanvas 管理 QGraphicsScene，把 domain::Workflow 渲染成节点和边。
// 它仍然属于 UI 层：负责交互和展示，不负责读写 JSON，也不负责执行工作流。
class WorkflowCanvas final : public QGraphicsView {
    Q_OBJECT

public:
    explicit WorkflowCanvas(QWidget* parent = nullptr);
    ~WorkflowCanvas() override;

    void setWorkflow(const domain::Workflow& workflow);
    domain::Workflow workflow() const;
    std::optional<domain::Node> selectedNode() const;

    void addNode(const domain::Node& node);
    void addStarterNodeAt(const QPointF& scenePos);
    void addFunctionNodeAt(const QPointF& scenePos);
    void addAgentNodeAt(const QPointF& scenePos);
    bool connectSelectedNodes();
    void clearWorkflow();
    bool updateNode(const domain::Node& node);
    void setNodeStatus(const QString& nodeId, const QString& status);
    void refreshTheme();

signals:
    void nodeSelected(const domain::Node& node);
    void nodeSelectionCleared();
    void nodeDoubleClicked(const domain::Node& node);
    void workflowChanged(const domain::Workflow& workflow);
    void saveRequested();
    void nodeTemplateDropped(const QString& templateId, const QPointF& scenePos);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void buildScene();
    void rebuildSceneFromWorkflow();
    void addNodeItem(const domain::Node& node);
    void addEdgeItem(const domain::Edge& edge);
    bool createEdgeBetween(const QString& sourceNodeId, const QString& targetNodeId);
    void updateEdgesForNode(const QString& nodeId);
    void updateAllEdgeRoutes();
    void clearEdgeDragState();
    void deleteSelectedItems();
    void removeEdge(const QString& edgeId);
    void removeNode(const QString& nodeId);
    void syncWorkflowFromItems();
    void pushUndoState();
    void undoLastChange();
    void zoomAtCursor(int wheelDelta);
    NodeGraphicsItem* outputNodeAt(const QPointF& scenePos) const;
    NodeGraphicsItem* inputNodeAt(const QPointF& scenePos, const QString& excludedNodeId = {}) const;
    QPainterPath edgePreviewPath(const QPointF& start, const QPointF& end) const;
    NodeVisualState visualStateFromStatus(const QString& status) const;
    QList<NodeGraphicsItem*> selectedNodeItems() const;
    void copySelectedNodes();
    void cutSelectedNodes();
    void pasteClipboardNodes();

    QGraphicsScene* m_scene = nullptr;
    WorkflowSceneController* m_sceneController = nullptr;
    domain::Workflow m_workflow;
    application::WorkflowHistory m_history;
    NodeGraphicsItem* m_edgeDragSource = nullptr;
    QGraphicsPathItem* m_edgePreviewItem = nullptr;
    QPointF m_edgeDragStartScenePos;
    bool m_rightButtonPanning = false;
    QPoint m_lastPanViewportPos;
    Qt::CursorShape m_previousCursor = Qt::ArrowCursor;
    application::WorkflowClipboard m_clipboard;
};

} // namespace vws::ui
