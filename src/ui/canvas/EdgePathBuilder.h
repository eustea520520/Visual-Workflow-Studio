#pragma once

#include <QPainterPath>
#include <QPointF>
#include <QVector>

namespace vws::ui {

class EdgePathBuilder final {
public:
    static QPainterPath buildRoundedPath(const QVector<QPointF>& points, qreal radius);
};

} // namespace vws::ui
