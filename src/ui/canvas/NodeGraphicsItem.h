#pragma once

#include "domain/Node.h"
#include "ui/canvas/NodePortSlotViewModel.h"
#include "ui/canvas/PortSlotHit.h"

#include <QGraphicsObject>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <optional>

namespace vws::ui {

// 节点在画布上的可视化状态。
// 它独立于 execution::NodeStatus，避免 UI 图元直接依赖执行层。
enum class NodeVisualState {
    Idle,
    Pending,
    Queued,
    Running,
    Succeeded,
    Failed,
};

// NodeGraphicsItem 是画布上的节点方框。
//
// 它只负责“一个节点怎么画、怎么拖、怎么选中”。
// 不负责保存文件、不负责执行节点，也不直接改 WorkflowService。
class NodeGraphicsItem final : public QGraphicsObject {
    Q_OBJECT

public:
    explicit NodeGraphicsItem(domain::Node node, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QString nodeId() const;
    domain::Node node() const;
    void setNode(const domain::Node& node);
    void setPortSlots(const QList<NodePortSlotViewModel>& inputs, const QList<NodePortSlotViewModel>& outputs);
    void setVisualState(NodeVisualState state);
    NodeVisualState visualState() const;

    QPointF inputAnchorScenePos() const;
    QPointF outputAnchorScenePos() const;
    QPointF inputAnchorScenePos(int slotIndex) const;
    QPointF outputAnchorScenePos(int slotIndex) const;
    QPointF inputSlotAnchorScenePos(int slotIndex) const;
    QPointF outputSlotAnchorScenePos(int slotIndex) const;
    std::optional<PortSlotHit> inputSlotAt(const QPointF& scenePos, qreal hitRadius) const;
    std::optional<PortSlotHit> outputSlotAt(const QPointF& scenePos, qreal hitRadius) const;
    int inputSlotCount(const QString& portName = QStringLiteral("input")) const;
    int outputSlotCount(const QString& portName = QStringLiteral("output")) const;
    QRectF bodySceneRect() const;

signals:
    void nodeMoved(const QString& nodeId, const QPointF& scenePos);
    void nodeSelected(const domain::Node& node);
    void nodeDoubleClicked(const domain::Node& node);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
    QColor borderColor() const;
    QColor fillColor() const;
    QColor stateStripColor() const;
    QRectF bodyRect() const;
    QSizeF bodySize() const;
    QPointF inputAnchorLocalPos() const;
    QPointF outputAnchorLocalPos() const;
    QPointF slotAnchorLocalPos(bool inputSide, int slotIndex) const;
    std::optional<PortSlotHit> slotAt(const QPointF& scenePos, qreal hitRadius, bool inputSide) const;
    void paintPortSlots(QPainter* painter, bool inputSide) const;
    static QRectF boundingRectFor(const domain::Node& node);
    static QSizeF bodySizeForRotation(int rotationDegrees);
    int maxSlotCount() const;

    domain::Node m_node;
    NodeVisualState m_state = NodeVisualState::Idle;
    QList<NodePortSlotViewModel> m_inputSlots;
    QList<NodePortSlotViewModel> m_outputSlots;
};

} // namespace vws::ui
