#pragma once

#include <QPainterPath>
#include <QPointF>
#include <QString>

class QGraphicsPathItem;
class QGraphicsScene;

namespace vws::ui {

class NodeGraphicsItem;

class EdgeDragController final {
public:
    ~EdgeDragController();

    bool begin(QGraphicsScene* scene, NodeGraphicsItem* sourceNode, const QPointF& scenePos);
    bool isDragging() const;
    QString sourceNodeId() const;
    bool sourceIs(const QString& nodeId) const;
    void update(const QPointF& scenePos);
    void clear(QGraphicsScene* scene);
    void refreshTheme();

private:
    QGraphicsPathItem* createPreviewItem() const;
    QPainterPath previewPath(const QPointF& start, const QPointF& end) const;

    NodeGraphicsItem* m_sourceNode = nullptr;
    QGraphicsPathItem* m_previewItem = nullptr;
    QPointF m_startScenePos;
};

} // namespace vws::ui
