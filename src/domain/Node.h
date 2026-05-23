#pragma once

#include "domain/NodeIoSpec.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace vws::domain {

struct NodePosition {
    double x = 0.0;
    double y = 0.0;

    QJsonObject toJson() const;
    static NodePosition fromJson(const QJsonObject& object);
};

// Optional canvas size. A non-positive width/height means "use the UI default".
struct NodeSize {
    double width = 0.0;
    double height = 0.0;

    bool isValid() const;
    QJsonObject toJson() const;
    static NodeSize fromJson(const QJsonObject& object);
};

struct NodeRuntime {
    int timeoutMs = 300000;
    int retryCount = 0;
    int maxMemoryMb = 1024;
    QString concurrencyGroup = "default";

    QJsonObject toJson() const;
    static NodeRuntime fromJson(const QJsonObject& object);
};

// A concrete workflow step on the canvas.
struct Node {
    QString nodeId;
    QString templateId;
    QString type;
    QString name;
    QString description;
    NodePosition position;
    NodeSize size;
    int rotationDegrees = 0;
    QStringList inputPorts;
    QStringList outputPorts;
    NodeIoSpec ioSpec;
    QJsonObject config;
    NodeRuntime runtime;

    QJsonObject toJson() const;
    static Node fromJson(const QJsonObject& object);
};

} // namespace vws::domain
