#pragma once

#include <QJsonObject>
#include <QString>

namespace vws::domain {

// Slot-level connection between one output circle and one input circle.
// Current workflow JSON must contain explicit from_slot and to_slot fields.
struct Edge {
    QString edgeId;
    QString fromNode;
    QString fromPort;
    int fromSlot = 0;
    QString toNode;
    QString toPort;
    int toSlot = 0;

    QJsonObject toJson() const;
    static Edge fromJson(const QJsonObject& object);
};

} // namespace vws::domain
