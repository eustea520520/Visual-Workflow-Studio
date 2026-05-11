#include "ui/canvas/NodeGraphicsItem.h"
#include "ui/theme/ThemeManager.h"

#include <QFont>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QSignalBlocker>
#include <QStyleOptionGraphicsItem>

#include <utility>

namespace vws::ui {

namespace {

constexpr qreal NodeWidth = 188.0;
constexpr qreal NodeHeight = 96.0;
constexpr qreal GlowPadding = 10.0;

QString nodeTypeLabel(const QString& type)
{
    return type.trimmed().isEmpty() ? QStringLiteral("node") : type.trimmed();
}

} // namespace

NodeGraphicsItem::NodeGraphicsItem(domain::Node node, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_node(std::move(node))
{
    setPos(m_node.position.x, m_node.position.y);
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges | ItemIsFocusable);
    setAcceptHoverEvents(true);
    setZValue(10.0);
    setToolTip(m_node.description.trimmed().isEmpty()
            ? m_node.name
            : QString("%1\n\n%2").arg(m_node.name, m_node.description));
}

QRectF NodeGraphicsItem::boundingRect() const
{
    // Running 状态会画一圈半透明高亮，所以 boundingRect 预留一点外扩空间。
    return QRectF(-GlowPadding, -GlowPadding, NodeWidth + GlowPadding * 2, NodeHeight + GlowPadding * 2);
}

void NodeGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);

    auto* tm = ThemeManager::instance();

    const QRectF bodyRect(0, 0, NodeWidth, NodeHeight);
    if (m_state == NodeVisualState::Running) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(tm ? tm->color("node-glow-running") : QColor(37, 99, 235, 44));
        painter->drawRoundedRect(bodyRect.adjusted(-7, -7, 7, 7), 12, 12);
    }

    painter->setPen(QPen(borderColor(), isSelected() ? 2.6 : 1.6));
    painter->setBrush(fillColor());
    painter->drawRoundedRect(bodyRect, 8, 8);

    painter->setPen(Qt::NoPen);
    painter->setBrush(stateStripColor());
    painter->drawRoundedRect(QRectF(0, 0, 8, NodeHeight), 8, 8);
    painter->drawRect(QRectF(4, 0, 4, NodeHeight));

    QFont titleFont = painter->font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    painter->setFont(titleFont);
    painter->setPen(tm ? tm->color("node-text-title") : QColor("#111827"));
    painter->drawText(QRectF(18, 16, NodeWidth - 32, 22), Qt::AlignLeft | Qt::AlignVCenter, m_node.name);

    QFont metaFont = painter->font();
    metaFont.setBold(false);
    metaFont.setPointSize(8);
    painter->setFont(metaFont);
    painter->setPen(tm ? tm->color("node-text-type") : QColor("#4b5563"));
    painter->drawText(QRectF(18, 42, NodeWidth - 32, 18), Qt::AlignLeft | Qt::AlignVCenter, nodeTypeLabel(m_node.type));

    painter->setPen(QPen(tm ? tm->color("node-port-border") : QColor("#9ca3af"), 1.4));
    painter->setBrush(tm ? tm->color("node-port-fill") : QColor("#ffffff"));
    if (!m_node.inputPorts.isEmpty()) {
        painter->drawEllipse(QPointF(0, NodeHeight / 2), 5.5, 5.5);
    }
    if (!m_node.outputPorts.isEmpty()) {
        painter->drawEllipse(QPointF(NodeWidth, NodeHeight / 2), 5.5, 5.5);
    }

    painter->setPen(tm ? tm->color("node-text-id") : QColor("#6b7280"));
    painter->drawText(QRectF(18, 67, NodeWidth - 36, 18), Qt::AlignLeft | Qt::AlignVCenter, m_node.nodeId);
}

QString NodeGraphicsItem::nodeId() const
{
    return m_node.nodeId;
}

domain::Node NodeGraphicsItem::node() const
{
    auto node = m_node;
    node.position.x = pos().x();
    node.position.y = pos().y();
    return node;
}

void NodeGraphicsItem::setNode(const domain::Node& node)
{
    // setNode 是“数据刷新”，不是用户拖动节点。这里如果直接 setPos，
    // Qt 会触发 itemChange(ItemPositionHasChanged)，进而发出 nodeMoved，
    // 造成保存代码时 Canvas 一边更新节点、一边重入同步 Workflow。
    // Starter 节点没有输入端口，保存后刷新边和节点时更容易触发这个交错。
    const QSignalBlocker blocker(this);
    prepareGeometryChange();
    m_node = node;
    setToolTip(m_node.description.trimmed().isEmpty()
            ? m_node.name
            : QString("%1\n\n%2").arg(m_node.name, m_node.description));
    const QPointF nextPos(m_node.position.x, m_node.position.y);
    if (pos() != nextPos) {
        setPos(nextPos);
    }
    update();
}

void NodeGraphicsItem::setVisualState(NodeVisualState state)
{
    if (m_state == state) {
        return;
    }

    m_state = state;
    update();
}

NodeVisualState NodeGraphicsItem::visualState() const
{
    return m_state;
}

QPointF NodeGraphicsItem::inputAnchorScenePos() const
{
    return mapToScene(QPointF(0, NodeHeight / 2));
}

QPointF NodeGraphicsItem::outputAnchorScenePos() const
{
    return mapToScene(QPointF(NodeWidth, NodeHeight / 2));
}

QRectF NodeGraphicsItem::bodySceneRect() const
{
    return mapRectToScene(QRectF(0, 0, NodeWidth, NodeHeight));
}

QVariant NodeGraphicsItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged) {
        const auto scenePos = pos();
        m_node.position.x = scenePos.x();
        m_node.position.y = scenePos.y();
        emit nodeMoved(m_node.nodeId, scenePos);
    }

    if (change == ItemSelectedHasChanged && value.toBool()) {
        emit nodeSelected(node());
    }

    return QGraphicsObject::itemChange(change, value);
}

void NodeGraphicsItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    emit nodeDoubleClicked(node());
    QGraphicsObject::mouseDoubleClickEvent(event);
}

QColor NodeGraphicsItem::borderColor() const
{
    auto* tm = ThemeManager::instance();
    if (!tm) {
        return QColor("#6b7280");
    }

    if (isSelected()) {
        return tm->color("primary");
    }

    switch (m_state) {
    case NodeVisualState::Running:
        return tm->color("node-border-running");
    case NodeVisualState::Succeeded:
        return tm->color("node-border-succeeded");
    case NodeVisualState::Failed:
        return tm->color("node-border-failed");
    case NodeVisualState::Queued:
        return tm->color("node-border-queued");
    case NodeVisualState::Pending:
        return tm->color("node-border-pending");
    case NodeVisualState::Idle:
        return tm->color("node-border-idle");
    }

    return tm->color("node-border-idle");
}

QColor NodeGraphicsItem::fillColor() const
{
    auto* tm = ThemeManager::instance();
    if (!tm) {
        return QColor("#ffffff");
    }

    switch (m_state) {
    case NodeVisualState::Running:
        return tm->color("node-fill-running");
    case NodeVisualState::Succeeded:
        return tm->color("node-fill-succeeded");
    case NodeVisualState::Failed:
        return tm->color("node-fill-failed");
    case NodeVisualState::Queued:
        return tm->color("node-fill-queued");
    case NodeVisualState::Pending:
        return tm->color("node-fill-pending");
    case NodeVisualState::Idle:
        return tm->color("node-fill-idle");
    }

    return tm->color("node-fill-idle");
}

QColor NodeGraphicsItem::stateStripColor() const
{
    auto* tm = ThemeManager::instance();
    if (!tm) {
        return QColor("#64748b");
    }

    switch (m_state) {
    case NodeVisualState::Running:
        return tm->color("node-strip-running");
    case NodeVisualState::Succeeded:
        return tm->color("node-strip-succeeded");
    case NodeVisualState::Failed:
        return tm->color("node-strip-failed");
    case NodeVisualState::Queued:
        return tm->color("node-strip-queued");
    case NodeVisualState::Pending:
        return tm->color("node-strip-pending");
    case NodeVisualState::Idle:
        return tm->color("node-strip-idle");
    }

    return tm->color("node-strip-idle");
}

} // namespace vws::ui
