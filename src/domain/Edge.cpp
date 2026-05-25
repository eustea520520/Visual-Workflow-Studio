#include "domain/Edge.h"

namespace vws::domain {

QJsonObject Edge::toJson() const
{
    return {
        {"edge_id", edgeId},
        {"from_node", fromNode},
        {"from_port", fromPort},
        {"from_slot", fromSlot},
        {"to_node", toNode},
        {"to_port", toPort},
        {"to_slot", toSlot},
    };
}

Edge Edge::fromJson(const QJsonObject& object)
{
    Edge edge;
    edge.edgeId = object.value("edge_id").toString();
    edge.fromNode = object.value("from_node").toString();
    edge.fromPort = object.value("from_port").toString();
    edge.fromSlot = object.value("from_slot").toInt(-1);
    edge.toNode = object.value("to_node").toString();
    edge.toPort = object.value("to_port").toString();
    edge.toSlot = object.value("to_slot").toInt(-1);
    return edge;
}

} // namespace vws::domain
