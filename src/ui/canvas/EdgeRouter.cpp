#include "ui/canvas/EdgeRouter.h"

#include "ui/canvas/ArrowHeadBuilder.h"
#include "ui/canvas/EdgePathBuilder.h"

#include <QHash>
#include <QLineF>
#include <QSet>
#include <QtMath>

#include <algorithm>
#include <limits>
#include <queue>

namespace vws::ui {

namespace {

constexpr qreal StubLength = 30.0;
constexpr qreal LanePadding = 8.0;
constexpr qreal BendPenalty = 20.0;
constexpr qreal Epsilon = 0.01;

struct SearchNode {
    int index = -1;
    qreal priority = 0.0;

    bool operator<(const SearchNode& other) const
    {
        return priority > other.priority;
    }
};

QRectF inflateRect(const QRectF& rect, qreal margin)
{
    return rect.adjusted(-margin, -margin, margin, margin);
}

QPointF outFromSource(const QPointF& point)
{
    return QPointF(point.x() + StubLength, point.y());
}

QPointF outFromTarget(const QPointF& point)
{
    return QPointF(point.x() - StubLength, point.y());
}

qreal manhattanDistance(const QPointF& a, const QPointF& b)
{
    return qAbs(a.x() - b.x()) + qAbs(a.y() - b.y());
}

bool fuzzyPointEqual(const QPointF& a, const QPointF& b)
{
    return qAbs(a.x() - b.x()) < Epsilon && qAbs(a.y() - b.y()) < Epsilon;
}

QVector<qreal> uniqueSorted(QVector<qreal> values)
{
    std::sort(values.begin(), values.end());
    QVector<qreal> result;
    for (const auto value : values) {
        if (result.isEmpty() || qAbs(result.last() - value) > 0.5) {
            result.append(value);
        }
    }
    return result;
}

bool pointInsideAnyObstacle(const QPointF& point, const QList<QRectF>& obstacles)
{
    for (const auto& obstacle : obstacles) {
        if (obstacle.adjusted(Epsilon, Epsilon, -Epsilon, -Epsilon).contains(point)) {
            return true;
        }
    }
    return false;
}

bool horizontalSegmentIntersects(const QPointF& a, const QPointF& b, const QRectF& rect)
{
    const auto y = a.y();
    if (y <= rect.top() + Epsilon || y >= rect.bottom() - Epsilon) {
        return false;
    }
    const auto left = qMin(a.x(), b.x());
    const auto right = qMax(a.x(), b.x());
    return right > rect.left() + Epsilon && left < rect.right() - Epsilon;
}

bool verticalSegmentIntersects(const QPointF& a, const QPointF& b, const QRectF& rect)
{
    const auto x = a.x();
    if (x <= rect.left() + Epsilon || x >= rect.right() - Epsilon) {
        return false;
    }
    const auto top = qMin(a.y(), b.y());
    const auto bottom = qMax(a.y(), b.y());
    return bottom > rect.top() + Epsilon && top < rect.bottom() - Epsilon;
}

bool segmentIntersectsAnyObstacle(const QPointF& a, const QPointF& b, const QList<QRectF>& obstacles)
{
    if (!qFuzzyCompare(a.x(), b.x()) && !qFuzzyCompare(a.y(), b.y())) {
        return true;
    }

    for (const auto& obstacle : obstacles) {
        if (qFuzzyCompare(a.y(), b.y())) {
            if (horizontalSegmentIntersects(a, b, obstacle)) {
                return true;
            }
        } else if (qFuzzyCompare(a.x(), b.x()) && verticalSegmentIntersects(a, b, obstacle)) {
            return true;
        }
    }
    return false;
}

QVector<QPointF> compactPolyline(const QVector<QPointF>& points)
{
    QVector<QPointF> compacted;
    for (const auto& point : points) {
        if (compacted.isEmpty() || !fuzzyPointEqual(compacted.last(), point)) {
            compacted.append(point);
        }
    }

    bool changed = true;
    while (changed && compacted.size() >= 3) {
        changed = false;
        for (int i = 1; i < compacted.size() - 1; ++i) {
            const auto a = compacted.at(i - 1);
            const auto b = compacted.at(i);
            const auto c = compacted.at(i + 1);
            const auto sameX = qAbs(a.x() - b.x()) < Epsilon && qAbs(b.x() - c.x()) < Epsilon;
            const auto sameY = qAbs(a.y() - b.y()) < Epsilon && qAbs(b.y() - c.y()) < Epsilon;
            if (sameX || sameY) {
                compacted.removeAt(i);
                changed = true;
                break;
            }
        }
    }
    return compacted;
}

QVector<QPointF> basicOrthogonalPath(const QPointF& start, const QPointF& startStub, const QPointF& endStub, const QPointF& end, qreal offset)
{
    const auto midX = (startStub.x() + endStub.x()) / 2.0 + offset;
    return compactPolyline({
        start,
        startStub,
        QPointF(midX, startStub.y()),
        QPointF(midX, endStub.y()),
        endStub,
        end,
    });
}

QVector<QPointF> fallbackBezierPolyline(const QPointF& start, const QPointF& end)
{
    const auto dx = qMax<qreal>(80.0, qAbs(end.x() - start.x()) * 0.45);
    return {
        start,
        QPointF(start.x() + dx, start.y()),
        QPointF(end.x() - dx, end.y()),
        end,
    };
}

bool polylineIntersectsAnyObstacle(const QVector<QPointF>& points, const QList<QRectF>& obstacles)
{
    for (int i = 0; i < points.size() - 1; ++i) {
        if (segmentIntersectsAnyObstacle(points.at(i), points.at(i + 1), obstacles)) {
            return true;
        }
    }
    return false;
}

QVector<QPointF> reconstructPath(const QVector<QPointF>& points, const QVector<int>& previous, int endIndex)
{
    QVector<QPointF> result;
    int current = endIndex;
    while (current >= 0) {
        result.prepend(points.at(current));
        current = previous.at(current);
    }
    return compactPolyline(result);
}

QVector<QPointF> routeOnCandidateGrid(const QPointF& startStub, const QPointF& endStub, const QList<QRectF>& obstacles, const EdgeRouteRequest& request)
{
    const auto centerX = (request.sourcePortScenePos.x() + request.targetPortScenePos.x()) / 2.0 + request.parallelOffset;
    QVector<qreal> xs = {startStub.x(), endStub.x(), request.sourcePortScenePos.x(), request.targetPortScenePos.x(), centerX};
    QVector<qreal> ys = {startStub.y(), endStub.y(), request.sourcePortScenePos.y(), request.targetPortScenePos.y()};

    QRectF bounds(startStub, endStub);
    bounds = bounds.normalized().adjusted(-160, -160, 160, 160);
    for (const auto& obstacle : obstacles) {
        bounds = bounds.united(obstacle.adjusted(-80, -80, 80, 80));
        xs << obstacle.left() - LanePadding << obstacle.right() + LanePadding;
        ys << obstacle.top() - LanePadding << obstacle.bottom() + LanePadding;
    }
    xs << bounds.left() << bounds.right();
    ys << bounds.top() << bounds.bottom();

    xs = uniqueSorted(xs);
    ys = uniqueSorted(ys);

    QVector<QPointF> points;
    QHash<QString, int> indexByKey;
    auto keyFor = [](const QPointF& point) {
        return QString("%1,%2").arg(qRound64(point.x() * 10.0)).arg(qRound64(point.y() * 10.0));
    };

    for (const auto x : xs) {
        for (const auto y : ys) {
            const QPointF point(x, y);
            if (pointInsideAnyObstacle(point, obstacles)) {
                continue;
            }
            indexByKey.insert(keyFor(point), points.size());
            points.append(point);
        }
    }

    const auto startKey = keyFor(startStub);
    const auto endKey = keyFor(endStub);
    if (!indexByKey.contains(startKey) || !indexByKey.contains(endKey)) {
        return {};
    }

    const auto startIndex = indexByKey.value(startKey);
    const auto endIndex = indexByKey.value(endKey);
    QVector<qreal> best(points.size(), std::numeric_limits<qreal>::infinity());
    QVector<int> previous(points.size(), -1);
    QVector<int> previousDirection(points.size(), -1);
    std::priority_queue<SearchNode> queue;
    best[startIndex] = 0.0;
    queue.push({startIndex, manhattanDistance(points.at(startIndex), points.at(endIndex))});

    while (!queue.empty()) {
        const auto current = queue.top();
        queue.pop();
        if (current.index == endIndex) {
            return reconstructPath(points, previous, endIndex);
        }

        const auto currentPoint = points.at(current.index);
        for (int next = 0; next < points.size(); ++next) {
            if (next == current.index) {
                continue;
            }
            const auto nextPoint = points.at(next);
            const auto horizontal = qAbs(currentPoint.y() - nextPoint.y()) < Epsilon;
            const auto vertical = qAbs(currentPoint.x() - nextPoint.x()) < Epsilon;
            if (!horizontal && !vertical) {
                continue;
            }
            if (segmentIntersectsAnyObstacle(currentPoint, nextPoint, obstacles)) {
                continue;
            }

            const auto direction = horizontal ? 0 : 1;
            const auto bendCost = previousDirection.at(current.index) >= 0 && previousDirection.at(current.index) != direction
                ? BendPenalty
                : 0.0;
            const auto cost = best.at(current.index) + manhattanDistance(currentPoint, nextPoint) + bendCost;
            if (cost >= best.at(next)) {
                continue;
            }

            best[next] = cost;
            previous[next] = current.index;
            previousDirection[next] = direction;
            queue.push({next, cost + manhattanDistance(nextPoint, points.at(endIndex))});
        }
    }

    return {};
}

QPointF arrowDirectionFromPoints(const QVector<QPointF>& points)
{
    if (points.size() < 2) {
        return QPointF(1, 0);
    }
    const auto direction = points.last() - points.at(points.size() - 2);
    return QLineF(QPointF(0, 0), direction).length() > 0.01 ? direction : QPointF(1, 0);
}

} // namespace

EdgeRouteResult EdgeRouter::route(const EdgeRouteRequest& request) const
{
    QList<QRectF> obstacles;
    for (const auto& rect : request.obstacleNodeRects) {
        obstacles.append(inflateRect(rect, request.obstacleMargin));
    }

    auto startStub = outFromSource(request.sourcePortScenePos);
    auto endStub = outFromTarget(request.targetPortScenePos);
    if (!qFuzzyIsNull(request.parallelOffset)) {
        startStub.ry() += request.parallelOffset;
        endStub.ry() += request.parallelOffset;
    }

    // 普通无障碍连线优先使用严格居中的正交路径，保证转折发生在输入/输出端口的水平中点。
    // 只有居中路径被其它节点挡住时，才进入 A* 候选网格避障搜索。
    auto routePoints = basicOrthogonalPath(request.sourcePortScenePos, startStub, endStub, request.targetPortScenePos, request.parallelOffset);
    if (polylineIntersectsAnyObstacle(routePoints, obstacles)) {
        routePoints = routeOnCandidateGrid(startStub, endStub, obstacles, request);
    }

    if (!routePoints.isEmpty() && !fuzzyPointEqual(routePoints.first(), request.sourcePortScenePos)) {
        routePoints.prepend(request.sourcePortScenePos);
    }
    if (!routePoints.isEmpty() && !fuzzyPointEqual(routePoints.last(), request.targetPortScenePos)) {
        routePoints.append(request.targetPortScenePos);
    }
    if (routePoints.isEmpty() || polylineIntersectsAnyObstacle(routePoints, obstacles)) {
        routePoints = fallbackBezierPolyline(request.sourcePortScenePos, request.targetPortScenePos);
    }

    routePoints = compactPolyline(routePoints);

    EdgeRouteResult result;
    result.polylinePoints = routePoints;
    result.smoothPath = EdgePathBuilder::buildRoundedPath(routePoints, request.cornerRadius);
    result.arrowTip = request.targetPortScenePos;
    result.arrowDirection = arrowDirectionFromPoints(routePoints);
    return result;
}

} // namespace vws::ui
