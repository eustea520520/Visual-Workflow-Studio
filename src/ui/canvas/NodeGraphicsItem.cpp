#include "ui/canvas/NodeGraphicsItem.h"
#include "ui/canvas/CanvasZ.h"
#include "ui/theme/ThemeManager.h"

#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QLineF>
#include <QPainter>
#include <QSignalBlocker>
#include <QStyleOptionGraphicsItem>

#include <utility>

namespace vws::ui {

namespace {

constexpr qreal NodeWidth = 188.0;
constexpr qreal NodeHeight = 96.0;
constexpr qreal RotatedNodeWidth = 150.0;
constexpr qreal RotatedNodeHeight = 150.0;
constexpr qreal GlowPadding = 10.0;
constexpr qreal SlotStep = 18.0;
constexpr qreal SlotMargin = 18.0;

QString nodeTypeLabel(const QString& type)
{
    return type.trimmed().isEmpty() ? QStringLiteral("node") : type.trimmed();
}

int normalizedRotation(int degrees)
{
    int normalized = degrees % 360;
    if (normalized < 0) {
        normalized += 360;
    }
    return (((normalized + 45) / 90) * 90) % 360;
}

} // namespace

NodeGraphicsItem::NodeGraphicsItem(domain::Node node, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_node(std::move(node))
{
    setPos(m_node.position.x, m_node.position.y);
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges | ItemIsFocusable);
    setAcceptHoverEvents(true);
    setZValue(CanvasZ::Node);
    setToolTip(m_node.description.trimmed().isEmpty()
            ? m_node.name
            : QString("%1\n\n%2").arg(m_node.name, m_node.description));
    for (const auto& port : m_node.inputPorts) {
        m_inputSlots.append({port, 0, QStringLiteral("1")});
    }
    for (const auto& port : m_node.outputPorts) {
        m_outputSlots.append({port, 0, QStringLiteral("1")});
    }
}

QRectF NodeGraphicsItem::boundingRect() const
{
    const auto size = bodySize();
    return QRectF(-GlowPadding, -GlowPadding, size.width() + GlowPadding * 2, size.height() + GlowPadding * 2);
}

void NodeGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);

    auto* tm = ThemeManager::instance();
    const QRectF body = bodyRect();
    if (m_state == NodeVisualState::Running) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(tm ? tm->color("node-glow-running") : QColor(37, 99, 235, 44));
        painter->drawRoundedRect(body.adjusted(-7, -7, 7, 7), 12, 12);
    }

    painter->setPen(QPen(borderColor(), isSelected() ? 2.6 : 1.6));
    painter->setBrush(fillColor());
    painter->drawRoundedRect(body, 8, 8);

    painter->setPen(Qt::NoPen);
    painter->setBrush(stateStripColor());
    switch (normalizedRotation(m_node.rotationDegrees)) {
    case 90:
        painter->drawRoundedRect(QRectF(body.left(), body.top(), body.width(), 8), 8, 8);
        painter->drawRect(QRectF(body.left(), body.top() + 4, body.width(), 4));
        break;
    case 180:
        painter->drawRoundedRect(QRectF(body.right() - 8, body.top(), 8, body.height()), 8, 8);
        painter->drawRect(QRectF(body.right() - 8, body.top(), 4, body.height()));
        break;
    case 270:
        painter->drawRoundedRect(QRectF(body.left(), body.bottom() - 8, body.width(), 8), 8, 8);
        painter->drawRect(QRectF(body.left(), body.bottom() - 8, body.width(), 4));
        break;
    case 0:
    default:
        painter->drawRoundedRect(QRectF(body.left(), body.top(), 8, body.height()), 8, 8);
        painter->drawRect(QRectF(body.left() + 4, body.top(), 4, body.height()));
        break;
    }

    const qreal textLeft = body.left() + 18;
    const qreal textWidth = body.width() - 36;
    const qreal contentHeight = 68;
    const qreal contentTop = body.top() + (body.height() - contentHeight) / 2.0;

    QFont titleFont = painter->font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    painter->setFont(titleFont);
    painter->setPen(tm ? tm->color("node-text-title") : QColor("#111827"));
    painter->drawText(
        QRectF(textLeft, contentTop, textWidth, 24),
        Qt::AlignLeft | Qt::AlignVCenter,
        m_node.name);

    QFont metaFont = painter->font();
    metaFont.setBold(false);
    metaFont.setPointSize(8);
    painter->setFont(metaFont);
    painter->setPen(tm ? tm->color("node-text-type") : QColor("#4b5563"));
    painter->drawText(
        QRectF(textLeft, contentTop + 28, textWidth, 18),
        Qt::AlignLeft | Qt::AlignVCenter,
        nodeTypeLabel(m_node.type));

    paintPortSlots(painter, true);
    paintPortSlots(painter, false);

    painter->setPen(tm ? tm->color("node-text-id") : QColor("#6b7280"));
    painter->drawText(
        QRectF(textLeft, contentTop + 53, textWidth, 18),
        Qt::AlignLeft | Qt::AlignVCenter,
        m_node.nodeId);
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
    node.rotationDegrees = normalizedRotation(node.rotationDegrees);
    return node;
}

void NodeGraphicsItem::setNode(const domain::Node& node)
{
    const QSignalBlocker blocker(this);
    prepareGeometryChange();
    m_node = node;
    m_node.rotationDegrees = normalizedRotation(m_node.rotationDegrees);
    setToolTip(m_node.description.trimmed().isEmpty()
            ? m_node.name
            : QString("%1\n\n%2").arg(m_node.name, m_node.description));
    const QPointF nextPos(m_node.position.x, m_node.position.y);
    if (pos() != nextPos) {
        setPos(nextPos);
    }
    update();
}

void NodeGraphicsItem::setPortSlots(const QList<NodePortSlotViewModel>& inputs, const QList<NodePortSlotViewModel>& outputs)
{
    prepareGeometryChange();
    m_inputSlots = inputs;
    m_outputSlots = outputs;
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
    return inputAnchorScenePos(-1);
}

QPointF NodeGraphicsItem::outputAnchorScenePos() const
{
    return outputAnchorScenePos(-1);
}

QPointF NodeGraphicsItem::inputAnchorScenePos(int slotIndex) const
{
    return mapToScene(slotAnchorLocalPos(true, slotIndex < 0 ? 0 : slotIndex));
}

QPointF NodeGraphicsItem::outputAnchorScenePos(int slotIndex) const
{
    return mapToScene(slotAnchorLocalPos(false, slotIndex < 0 ? 0 : slotIndex));
}

QPointF NodeGraphicsItem::inputSlotAnchorScenePos(int slotIndex) const
{
    return inputAnchorScenePos(slotIndex);
}

QPointF NodeGraphicsItem::outputSlotAnchorScenePos(int slotIndex) const
{
    return outputAnchorScenePos(slotIndex);
}

std::optional<PortSlotHit> NodeGraphicsItem::inputSlotAt(const QPointF& scenePos, qreal hitRadius) const
{
    return slotAt(scenePos, hitRadius, true);
}

std::optional<PortSlotHit> NodeGraphicsItem::outputSlotAt(const QPointF& scenePos, qreal hitRadius) const
{
    return slotAt(scenePos, hitRadius, false);
}

int NodeGraphicsItem::inputSlotCount(const QString& portName) const
{
    int count = 0;
    for (const auto& slot : m_inputSlots) {
        if (portName.isEmpty() || slot.portName == portName) {
            ++count;
        }
    }
    return count;
}

int NodeGraphicsItem::outputSlotCount(const QString& portName) const
{
    int count = 0;
    for (const auto& slot : m_outputSlots) {
        if (portName.isEmpty() || slot.portName == portName) {
            ++count;
        }
    }
    return count;
}

QRectF NodeGraphicsItem::bodySceneRect() const
{
    return mapRectToScene(bodyRect());
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

QRectF NodeGraphicsItem::bodyRect() const
{
    const auto size = bodySize();
    return QRectF(0, 0, size.width(), size.height());
}

QSizeF NodeGraphicsItem::bodySize() const
{
    auto size = bodySizeForRotation(m_node.rotationDegrees);
    const auto required = SlotMargin * 2 + qMax(1, maxSlotCount()) * SlotStep;
    switch (normalizedRotation(m_node.rotationDegrees)) {
    case 90:
    case 270:
        size.rwidth() = qMax(size.width(), required);
        break;
    case 0:
    case 180:
    default:
        size.rheight() = qMax(size.height(), required);
        break;
    }
    return size;
}

QPointF NodeGraphicsItem::inputAnchorLocalPos() const
{
    return slotAnchorLocalPos(true, 0);
}

QPointF NodeGraphicsItem::outputAnchorLocalPos() const
{
    return slotAnchorLocalPos(false, 0);
}

QPointF NodeGraphicsItem::slotAnchorLocalPos(bool inputSide, int slotIndex) const
{
    const auto body = bodyRect();
    const auto portSlots = inputSide ? m_inputSlots : m_outputSlots;
    const auto count = qMax(1, portSlots.size());
    const auto clampedIndex = qBound(0, slotIndex, count - 1);
    const auto sideLength = [body, inputSide, this]() {
        const auto rotation = normalizedRotation(m_node.rotationDegrees);
        const bool verticalSide = (rotation == 0 || rotation == 180);
        return verticalSide ? body.height() : body.width();
    }();
    const auto usable = qMax<qreal>(1.0, sideLength - SlotMargin * 2);
    const auto step = count == 1 ? 0.0 : usable / (count - 1);
    const auto offset = count == 1 ? sideLength / 2.0 : SlotMargin + clampedIndex * step;

    switch (normalizedRotation(m_node.rotationDegrees)) {
    case 90:
        return inputSide ? QPointF(offset, body.top()) : QPointF(offset, body.bottom());
    case 180:
        return inputSide ? QPointF(body.right(), offset) : QPointF(body.left(), offset);
    case 270:
        return inputSide ? QPointF(offset, body.bottom()) : QPointF(offset, body.top());
    case 0:
    default:
        return inputSide ? QPointF(body.left(), offset) : QPointF(body.right(), offset);
    }
}

std::optional<PortSlotHit> NodeGraphicsItem::slotAt(const QPointF& scenePos, qreal hitRadius, bool inputSide) const
{
    const auto localPos = mapFromScene(scenePos);
    const auto portSlots = inputSide ? m_inputSlots : m_outputSlots;
    for (int index = 0; index < portSlots.size(); ++index) {
        if (QLineF(localPos, slotAnchorLocalPos(inputSide, index)).length() <= hitRadius) {
            return PortSlotHit{
                m_node.nodeId,
                portSlots.at(index).portName,
                portSlots.at(index).slotIndex,
                inputSide ? PortDirection::Input : PortDirection::Output,
            };
        }
    }
    return std::nullopt;
}

void NodeGraphicsItem::paintPortSlots(QPainter* painter, bool inputSide) const
{
    const auto portSlots = inputSide ? m_inputSlots : m_outputSlots;
    if (portSlots.isEmpty()) {
        return;
    }

    auto* tm = ThemeManager::instance();
    painter->setPen(QPen(tm ? tm->color("node-port-border") : QColor("#9ca3af"), 1.4));
    painter->setBrush(tm ? tm->color("node-port-fill") : QColor("#ffffff"));

    QFont labelFont = painter->font();
    labelFont.setPointSize(7);
    labelFont.setBold(false);
    painter->setFont(labelFont);

    for (int index = 0; index < portSlots.size(); ++index) {
        const auto anchor = slotAnchorLocalPos(inputSide, index);
        painter->setPen(QPen(tm ? tm->color("node-port-border") : QColor("#9ca3af"), 1.4));
        painter->drawEllipse(anchor, 5.5, 5.5);
        painter->setPen(tm ? tm->color("node-text-id") : QColor("#6b7280"));

        const auto rotation = normalizedRotation(m_node.rotationDegrees);
        QRectF labelRect;
        Qt::Alignment alignment = Qt::AlignCenter;
        if (rotation == 0 || rotation == 180) {
            labelRect = inputSide
                ? QRectF(anchor.x() + 8, anchor.y() - 8, 42, 16)
                : QRectF(anchor.x() - 50, anchor.y() - 8, 42, 16);
            alignment = inputSide ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignRight | Qt::AlignVCenter;
        } else {
            labelRect = inputSide
                ? QRectF(anchor.x() - 21, anchor.y() + (rotation == 90 ? 8 : -24), 42, 16)
                : QRectF(anchor.x() - 21, anchor.y() + (rotation == 90 ? -24 : 8), 42, 16);
        }
        painter->drawText(labelRect, alignment, portSlots.at(index).label);
    }
}

QRectF NodeGraphicsItem::boundingRectFor(const domain::Node& node)
{
    const auto size = bodySizeForRotation(node.rotationDegrees);
    return QRectF(
        -GlowPadding,
        -GlowPadding,
        size.width() + GlowPadding * 2,
        size.height() + GlowPadding * 2);
}

QSizeF NodeGraphicsItem::bodySizeForRotation(int rotationDegrees)
{
    switch (normalizedRotation(rotationDegrees)) {
    case 90:
    case 270:
        return QSizeF(RotatedNodeWidth, RotatedNodeHeight);
    case 0:
    case 180:
    default:
        return QSizeF(NodeWidth, NodeHeight);
    }
}

int NodeGraphicsItem::maxSlotCount() const
{
    return qMax(m_inputSlots.size(), m_outputSlots.size());
}

} // namespace vws::ui
