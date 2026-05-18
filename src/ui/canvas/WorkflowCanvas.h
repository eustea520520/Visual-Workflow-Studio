#pragma once

#include "application/WorkflowClipboard.h"
#include "application/WorkflowHistory.h"
#include "domain/EdgeEndpoint.h"
#include "domain/Workflow.h"
#include "ui/canvas/EdgeDragController.h"
#include "ui/canvas/NodeGraphicsItem.h"
#include "ui/canvas/WorkflowCanvasContextMenu.h"
#include "ui/canvas/WorkflowCanvasInteractionController.h"

#include <QGraphicsView>
#include <QHash>
#include <QList>
#include <optional>
#include <QPointF>
#include <QString>

class QGraphicsScene;
class QContextMenuEvent;
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
    bool connectSelectedNodes();
    void clearWorkflow();
    bool updateNode(const domain::Node& node);
    void setNodeIoSpec(const QString& nodeId, const domain::NodeIoSpec& spec);
    void applyRuntimeIoSpecs(const QHash<QString, domain::NodeIoSpec>& specsByNodeId);
    void setNodeStatus(const QString& nodeId, const QString& status);
    void refreshTheme();

signals:
    void nodeSelected(const domain::Node& node);
    void nodeSelectionCleared();
    void nodeDoubleClicked(const domain::Node& node);
    void workflowChanged(const domain::Workflow& workflow);
    void saveRequested();
    void nodeTemplateDropped(const QString& templateId, const QPointF& scenePos);
    void starterNodeRequested(const QPointF& scenePos, StarterNodeTemplate templateKind);
    void functionNodeRequested(const QPointF& scenePos, DataTransferNodeTemplate templateKind);
    void agentNodeRequested(const QPointF& scenePos, DataTransferNodeTemplate templateKind);

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
    bool createEdgeBetween(const domain::EdgeEndpoint& source, const domain::EdgeEndpoint& target);
    void updateEdgesForNode(const QString& nodeId);
    void updateAllEdgeRoutes();
    void deleteSelectedItems();
    void removeEdge(const QString& edgeId);
    void removeNode(const QString& nodeId);
    bool rotateSelectedNode(int deltaDegrees);
    bool rotateNode(const QString& nodeId, int deltaDegrees);
    void syncWorkflowFromItems();
    void pushUndoState();
    void undoLastChange();
    NodeGraphicsItem* outputNodeAt(const QPointF& scenePos) const;
    NodeGraphicsItem* inputNodeAt(const QPointF& scenePos, const QString& excludedNodeId = {}) const;
    std::optional<PortSlotHit> outputSlotAt(const QPointF& scenePos) const;
    std::optional<PortSlotHit> inputSlotAt(const QPointF& scenePos, const QString& excludedNodeId = {}) const;
    NodeVisualState visualStateFromStatus(const QString& status) const;
    QList<NodeGraphicsItem*> selectedNodeItems() const;
    void copySelectedNodes();
    void cutSelectedNodes();
    void pasteClipboardNodes();

    QGraphicsScene* m_scene = nullptr;
    WorkflowSceneController* m_sceneController = nullptr;
    domain::Workflow m_workflow;
    application::WorkflowHistory m_history;
    WorkflowCanvasInteractionController m_interactionController;
    EdgeDragController m_edgeDragController;
    application::WorkflowClipboard m_clipboard;
};

} // namespace vws::ui
