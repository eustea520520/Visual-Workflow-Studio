#pragma once

#include "domain/EdgeEndpoint.h"

#include <QPainterPath>
#include <QPointF>
#include <QString>

class QGraphicsPathItem;
class QGraphicsScene;

namespace vws::ui {

class EdgeDragController final {
public:
    ~EdgeDragController();

    bool begin(QGraphicsScene* scene, const domain::EdgeEndpoint& source, const QPointF& startScenePos);
    bool isDragging() const;
    QString sourceNodeId() const;
    domain::EdgeEndpoint sourceEndpoint() const;
    bool sourceIs(const QString& nodeId) const;
    void update(const QPointF& scenePos);
    void clear(QGraphicsScene* scene);
    void refreshTheme();

private:
    QGraphicsPathItem* createPreviewItem() const;
    QPainterPath previewPath(const QPointF& start, const QPointF& end) const;

    domain::EdgeEndpoint m_source;
    bool m_dragging = false;
    QGraphicsPathItem* m_previewItem = nullptr;
    QPointF m_startScenePos;
};

} // namespace vws::ui
