#include "ui/canvas/ArrowHeadBuilder.h"

#include <QLineF>

namespace vws::ui {

QPolygonF ArrowHeadBuilder::build(const QPointF& tip, const QPointF& direction, qreal length, qreal width)
{
    auto line = QLineF(QPointF(0, 0), direction);
    if (line.length() <= 0.01) {
        line = QLineF(QPointF(0, 0), QPointF(1, 0));
    }
    line.setLength(1.0);

    const QPointF unit(line.dx(), line.dy());
    const QPointF normal(-unit.y(), unit.x());
    const auto base = tip - unit * length;

    return QPolygonF{
        tip,
        base + normal * (width / 2.0),
        base - normal * (width / 2.0),
    };
}

} // namespace vws::ui
