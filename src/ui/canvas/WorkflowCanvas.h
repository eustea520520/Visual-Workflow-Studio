#pragma once

#include "domain/Workflow.h"
#include "ui/canvas/NodeGraphicsItem.h"
#include "ui/editor/PythonCodeTemplates.h"

#include <QGraphicsView>
#include <QHash>
#include <QList>
#include <optional>
#include <QPointF>
#include <QRectF>
#include <QString>

class QGraphicsScene;
class QContextMenuEvent;
class QGraphicsPathItem;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace vws::ui {

class EdgeGraphicsItem;

// 中央工作流画布。
//
// WorkflowCanvas 管理 QGraphicsScene，把 domain::Workflow 渲染成节点和边。
// 它仍然属于 UI 层：负责交互和展示，不负责读写 JSON，也不负责执行工作流。
class WorkflowCanvas final : public QGraphicsView {
    Q_OBJECT

public:
    explicit WorkflowCanvas(QWidget* parent = nullptr);

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
    void nodeDoubleClicked(const domain::Node& node);
    void workflowChanged(const domain::Workflow& workflow);
    void saveRequested();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    enum class StarterTemplateKind {
        EmptyOutput,
        DataOutput,
        FileOutput,
    };

    void buildScene();
    void rebuildSceneFromWorkflow();
    void addNodeItem(const domain::Node& node);
    void addEdgeItem(const domain::Edge& edge);
    bool createEdgeBetween(const QString& sourceNodeId, const QString& targetNodeId);
    void updateEdgesForNode(const QString& nodeId);
    void updateAllEdgeRoutes();
    QList<QRectF> nodeObstacleRectsForEdge(const domain::Edge& edge) const;
    int parallelEdgeIndex(const domain::Edge& edge) const;
    void clearEdgeDragState();
    void deleteSelectedItems();
    void removeEdge(const QString& edgeId);
    void removeNode(const QString& nodeId);
    void syncWorkflowFromItems();
    void pushUndoState();
    void undoLastChange();
    void zoomAtCursor(int wheelDelta);
    domain::Node createStarterNode(const QPointF& scenePos, StarterTemplateKind templateKind = StarterTemplateKind::DataOutput) const;
    domain::Node createFunctionNode(const QPointF& scenePos, DataTransferTemplate templateKind = DataTransferTemplate::DataToData) const;
    domain::Node createAgentNode(const QPointF& scenePos, DataTransferTemplate templateKind = DataTransferTemplate::DataToData) const;
    NodeGraphicsItem* outputNodeAt(const QPointF& scenePos) const;
    NodeGraphicsItem* inputNodeAt(const QPointF& scenePos, const QString& excludedNodeId = {}) const;
    QPainterPath edgePreviewPath(const QPointF& start, const QPointF& end) const;
    NodeVisualState visualStateFromStatus(const QString& status) const;

    QGraphicsScene* m_scene = nullptr;
    domain::Workflow m_workflow;
    QHash<QString, NodeGraphicsItem*> m_nodeItems;
    QHash<QString, EdgeGraphicsItem*> m_edgeItems;
    QList<domain::Workflow> m_undoStack;
    bool m_restoringHistory = false;
    NodeGraphicsItem* m_edgeDragSource = nullptr;
    QGraphicsPathItem* m_edgePreviewItem = nullptr;
    QPointF m_edgeDragStartScenePos;
    bool m_rightButtonPanning = false;
    QPoint m_lastPanViewportPos;
    Qt::CursorShape m_previousCursor = Qt::ArrowCursor;
};

} // namespace vws::ui
