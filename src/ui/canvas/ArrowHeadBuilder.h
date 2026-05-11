#pragma once

#include <QPointF>
#include <QPolygonF>

namespace vws::ui {

class ArrowHeadBuilder final {
public:
    static QPolygonF build(const QPointF& tip, const QPointF& direction, qreal length = 12.0, qreal width = 8.0);
};

} // namespace vws::ui
