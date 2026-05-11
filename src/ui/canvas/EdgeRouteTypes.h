#pragma once

#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QVector>

namespace vws::ui {

struct EdgeRouteRequest {
    QPointF sourcePortScenePos;
    QPointF targetPortScenePos;
    QRectF sourceNodeRect;
    QRectF targetNodeRect;
    QList<QRectF> obstacleNodeRects;
    qreal obstacleMargin = 20.0;
    qreal cornerRadius = 12.0;
    qreal parallelOffset = 0.0;
};

struct EdgeRouteResult {
    QVector<QPointF> polylinePoints;
    QPainterPath smoothPath;
    QPointF arrowTip;
    QPointF arrowDirection;
};

} // namespace vws::ui
