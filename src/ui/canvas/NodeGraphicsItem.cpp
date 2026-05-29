#include "ui/canvas/NodeGraphicsItem.h"
#include "ui/canvas/CanvasZ.h"
#include "ui/theme/ThemeManager.h"

#include <QFont>
#include <QGraphicsSceneHoverEvent>
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
constexpr qreal ResizePadding = 28.0;
constexpr qreal ResizeHandleRadius = 6.0;
constexpr qreal ResizeHandleOutset = ResizeHandleRadius;
constexpr qreal ResizeHitRadius = 10.0;
constexpr qreal MinNodeWidth = 120.0;
constexpr qreal MinNodeHeight = 72.0;
constexpr qreal MaxNodeWidth = 640.0;
constexpr qreal MaxNodeHeight = 420.0;
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
    return QRectF(-ResizePadding, -ResizePadding, size.width() + ResizePadding * 2, size.height() + ResizePadding * 2);
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
    paintResizeHandles(painter);

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

QPointF NodeGraphicsItem::inputAnchorScenePos(const QString& portName, int slotIndex) const
{
    return mapToScene(slotAnchorLocalPos(true, portName, slotIndex));
}

QPointF NodeGraphicsItem::outputAnchorScenePos(const QString& portName, int slotIndex) const
{
    return mapToScene(slotAnchorLocalPos(false, portName, slotIndex));
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

bool NodeGraphicsItem::hasResizeHandleAt(const QPointF& scenePos) const
{
    return resizeHandleAt(mapFromScene(scenePos)) != ResizeEdge::None;
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

void NodeGraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    const auto handle = resizeHandleAt(event->pos());
    if (handle == ResizeEdge::Left || handle == ResizeEdge::Right) {
        setCursor(Qt::SizeHorCursor);
    } else if (handle == ResizeEdge::Top || handle == ResizeEdge::Bottom) {
        setCursor(Qt::SizeVerCursor);
    } else {
        unsetCursor();
    }
    QGraphicsObject::hoverMoveEvent(event);
}

void NodeGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}

void NodeGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isSelected()) {
        const auto handle = resizeHandleAt(event->pos());
        if (handle != ResizeEdge::None) {
            m_resizing = true;
            m_resizeEdge = handle;
            m_resizeStartScenePos = event->scenePos();
            m_resizeStartItemPos = pos();
            m_resizeStartSize = rawBodySize();
            event->accept();
            return;
        }
    }

    QGraphicsObject::mousePressEvent(event);
}

void NodeGraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_resizing) {
        resizeFromSceneDelta(event->scenePos() - m_resizeStartScenePos);
        event->accept();
        return;
    }

    QGraphicsObject::mouseMoveEvent(event);
}

void NodeGraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_resizing && event->button() == Qt::LeftButton) {
        m_resizing = false;
        m_resizeEdge = ResizeEdge::None;
        unsetCursor();
        emit nodeMoved(m_node.nodeId, pos());
        event->accept();
        return;
    }

    QGraphicsObject::mouseReleaseEvent(event);
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
    auto size = rawBodySize();
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

QSizeF NodeGraphicsItem::rawBodySize() const
{
    auto size = m_node.size.isValid()
        ? QSizeF(m_node.size.width, m_node.size.height)
        : bodySizeForRotation(m_node.rotationDegrees);
    size.setWidth(qBound(MinNodeWidth, size.width(), MaxNodeWidth));
    size.setHeight(qBound(MinNodeHeight, size.height(), MaxNodeHeight));
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

QPointF NodeGraphicsItem::slotAnchorLocalPos(bool inputSide, const QString& portName, int slotIndex) const
{
    return slotAnchorLocalPos(inputSide, visualSlotIndex(inputSide, portName, slotIndex));
}

int NodeGraphicsItem::visualSlotIndex(bool inputSide, const QString& portName, int slotIndex) const
{
    const auto portSlots = inputSide ? m_inputSlots : m_outputSlots;
    const auto normalizedPort = portName.trimmed();
    for (int index = 0; index < portSlots.size(); ++index) {
        const auto& slot = portSlots.at(index);
        if (slot.portName == normalizedPort && slot.slotIndex == slotIndex) {
            return index;
        }
    }
    return qBound(0, slotIndex, qMax(0, portSlots.size() - 1));
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

void NodeGraphicsItem::paintResizeHandles(QPainter* painter) const
{
    if (!isSelected()) {
        return;
    }

    auto* tm = ThemeManager::instance();
    const auto color = tm ? tm->color("primary") : QColor("#2563eb");
    const auto body = bodyRect();

    painter->save();
    QPen railPen(color, 1.5);
    railPen.setCapStyle(Qt::RoundCap);
    painter->setPen(railPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QLineF(body.left(), body.top(), body.right(), body.top()));
    painter->drawLine(QLineF(body.left(), body.bottom(), body.right(), body.bottom()));
    painter->drawLine(QLineF(body.left(), body.top(), body.left(), body.bottom()));
    painter->drawLine(QLineF(body.right(), body.top(), body.right(), body.bottom()));

    painter->setPen(QPen(color, 1.6));
    painter->setBrush(tm ? tm->color("node-port-fill") : QColor("#ffffff"));
    const QList<ResizeEdge> edges = {
        ResizeEdge::Left,
        ResizeEdge::Right,
        ResizeEdge::Top,
        ResizeEdge::Bottom,
    };
    for (const auto edge : edges) {
        painter->drawEllipse(resizeHandleCenter(edge), ResizeHandleRadius, ResizeHandleRadius);
    }
    painter->restore();
}

NodeGraphicsItem::ResizeEdge NodeGraphicsItem::resizeHandleAt(const QPointF& localPos) const
{
    if (!isSelected()) {
        return ResizeEdge::None;
    }

    const auto body = bodyRect();
    const QList<ResizeEdge> edges = {
        ResizeEdge::Left,
        ResizeEdge::Right,
        ResizeEdge::Top,
        ResizeEdge::Bottom,
    };
    for (const auto edge : edges) {
        bool onHandleSide = false;
        switch (edge) {
        case ResizeEdge::Left:
            onHandleSide = localPos.x() < body.left();
            break;
        case ResizeEdge::Right:
            onHandleSide = localPos.x() > body.right();
            break;
        case ResizeEdge::Top:
            onHandleSide = localPos.y() < body.top();
            break;
        case ResizeEdge::Bottom:
            onHandleSide = localPos.y() > body.bottom();
            break;
        case ResizeEdge::None:
            break;
        }

        if (onHandleSide && QLineF(localPos, resizeHandleCenter(edge)).length() <= ResizeHitRadius) {
            return edge;
        }
    }
    return ResizeEdge::None;
}

QPointF NodeGraphicsItem::resizeHandleCenter(ResizeEdge edge) const
{
    const auto body = bodyRect();
    switch (edge) {
    case ResizeEdge::Left:
        return QPointF(body.left() - ResizeHandleOutset, body.center().y());
    case ResizeEdge::Right:
        return QPointF(body.right() + ResizeHandleOutset, body.center().y());
    case ResizeEdge::Top:
        return QPointF(body.center().x(), body.top() - ResizeHandleOutset);
    case ResizeEdge::Bottom:
        return QPointF(body.center().x(), body.bottom() + ResizeHandleOutset);
    case ResizeEdge::None:
    default:
        return body.center();
    }
}

void NodeGraphicsItem::resizeFromSceneDelta(const QPointF& sceneDelta)
{
    auto nextPos = m_resizeStartItemPos;
    auto nextSize = m_resizeStartSize;

    switch (m_resizeEdge) {
    case ResizeEdge::Left: {
        const auto dx = qBound(m_resizeStartSize.width() - MaxNodeWidth, sceneDelta.x(), m_resizeStartSize.width() - MinNodeWidth);
        nextPos.setX(m_resizeStartItemPos.x() + dx);
        nextSize.setWidth(m_resizeStartSize.width() - dx);
        break;
    }
    case ResizeEdge::Right:
        nextSize.setWidth(qBound(MinNodeWidth, m_resizeStartSize.width() + sceneDelta.x(), MaxNodeWidth));
        break;
    case ResizeEdge::Top: {
        const auto dy = qBound(m_resizeStartSize.height() - MaxNodeHeight, sceneDelta.y(), m_resizeStartSize.height() - MinNodeHeight);
        nextPos.setY(m_resizeStartItemPos.y() + dy);
        nextSize.setHeight(m_resizeStartSize.height() - dy);
        break;
    }
    case ResizeEdge::Bottom:
        nextSize.setHeight(qBound(MinNodeHeight, m_resizeStartSize.height() + sceneDelta.y(), MaxNodeHeight));
        break;
    case ResizeEdge::None:
        return;
    }

    prepareGeometryChange();
    m_node.size.width = nextSize.width();
    m_node.size.height = nextSize.height();
    if (pos() != nextPos) {
        setPos(nextPos);
    }
    update();
    emit nodeMoved(m_node.nodeId, pos());
}

QRectF NodeGraphicsItem::boundingRectFor(const domain::Node& node)
{
    const auto size = node.size.isValid()
        ? QSizeF(
            qBound(MinNodeWidth, node.size.width, MaxNodeWidth),
            qBound(MinNodeHeight, node.size.height, MaxNodeHeight))
        : bodySizeForRotation(node.rotationDegrees);
    return QRectF(
        -ResizePadding,
        -ResizePadding,
        size.width() + ResizePadding * 2,
        size.height() + ResizePadding * 2);
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
