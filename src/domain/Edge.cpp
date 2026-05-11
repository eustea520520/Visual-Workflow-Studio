#include "domain/Edge.h"

namespace vws::domain {

QJsonObject Edge::toJson() const
{
    return {
        {"edge_id", edgeId},
        {"from_node", fromNode},
        {"from_port", fromPort},
        {"to_node", toNode},
        {"to_port", toPort},
    };
}

Edge Edge::fromJson(const QJsonObject& object)
{
    Edge edge;
    edge.edgeId = object.value("edge_id").toString();
    edge.fromNode = object.value("from_node").toString();
    edge.fromPort = object.value("from_port").toString();
    edge.toNode = object.value("to_node").toString();
    edge.toPort = object.value("to_port").toString();
    return edge;
}

} // namespace vws::domain
