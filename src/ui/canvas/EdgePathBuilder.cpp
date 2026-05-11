#include "ui/canvas/EdgePathBuilder.h"

#include <QLineF>
#include <QtMath>

namespace vws::ui {

namespace {

qreal distanceBetween(const QPointF& a, const QPointF& b)
{
    return QLineF(a, b).length();
}

QPointF pointToward(const QPointF& from, const QPointF& to, qreal distance)
{
    const auto length = distanceBetween(from, to);
    if (length <= 0.01) {
        return from;
    }
    const auto ratio = distance / length;
    return QPointF(
        from.x() + (to.x() - from.x()) * ratio,
        from.y() + (to.y() - from.y()) * ratio);
}

} // namespace

QPainterPath EdgePathBuilder::buildRoundedPath(const QVector<QPointF>& points, qreal radius)
{
    QPainterPath path;
    if (points.isEmpty()) {
        return path;
    }
    if (points.size() == 1) {
        path.moveTo(points.first());
        return path;
    }

    path.moveTo(points.first());
    for (int i = 1; i < points.size() - 1; ++i) {
        const auto previous = points.at(i - 1);
        const auto corner = points.at(i);
        const auto next = points.at(i + 1);
        const auto incomingLength = distanceBetween(previous, corner);
        const auto outgoingLength = distanceBetween(corner, next);
        const auto effectiveRadius = qMin(radius, qMin(incomingLength, outgoingLength) / 2.0);

        if (effectiveRadius <= 0.5) {
            path.lineTo(corner);
            continue;
        }

        const auto beforeCorner = pointToward(corner, previous, effectiveRadius);
        const auto afterCorner = pointToward(corner, next, effectiveRadius);
        path.lineTo(beforeCorner);
        path.quadTo(corner, afterCorner);
    }

    path.lineTo(points.last());
    return path;
}

} // namespace vws::ui
