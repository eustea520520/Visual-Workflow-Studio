#include "application/subsystem/SubsystemTypes.h"

#include <QJsonArray>
#include <QtGlobal>

namespace vws::application {

namespace {

QJsonArray stringListToJson(const QStringList& values)
{
    QJsonArray array;
    for (const auto& value : values) {
        array.append(value);
    }
    return array;
}

QStringList stringListFromJson(const QJsonArray& array)
{
    QStringList values;
    for (const auto& value : array) {
        values.append(value.toString());
    }
    return values;
}

QJsonArray portsToJson(const QList<SubsystemBoundaryPort>& ports)
{
    QJsonArray array;
    for (const auto& port : ports) {
        array.append(port.toJson());
    }
    return array;
}

QList<SubsystemBoundaryPort> portsFromJson(const QJsonArray& array)
{
    QList<SubsystemBoundaryPort> ports;
    for (const auto& value : array) {
        ports.append(SubsystemBoundaryPort::fromJson(value.toObject()));
    }
    return ports;
}

} // namespace

QJsonObject SubsystemBoundaryPort::toJson() const
{
    return {
        {"external_port", externalPort},
        {"internal_node_id", internalNodeId},
        {"internal_node_name", internalNodeName},
        {"internal_port", internalPort},
        {"dimension", qBound(1, dimension, 32)},
        {"item_labels", stringListToJson(itemLabels)},
    };
}

SubsystemBoundaryPort SubsystemBoundaryPort::fromJson(const QJsonObject& object)
{
    SubsystemBoundaryPort port;
    port.externalPort = object.value("external_port").toString();
    port.internalNodeId = object.value("internal_node_id").toString();
    port.internalNodeName = object.value("internal_node_name").toString();
    port.internalPort = object.value("internal_port").toString();
    port.dimension = qBound(1, object.value("dimension").toInt(1), 32);
    port.itemLabels = stringListFromJson(object.value("item_labels").toArray());
    return port;
}

QJsonObject SubsystemBoundary::toJson() const
{
    return {
        {"inputs", portsToJson(inputs)},
        {"outputs", portsToJson(outputs)},
    };
}

SubsystemBoundary SubsystemBoundary::fromJson(const QJsonObject& object)
{
    SubsystemBoundary boundary;
    boundary.inputs = portsFromJson(object.value("inputs").toArray());
    boundary.outputs = portsFromJson(object.value("outputs").toArray());
    return boundary;
}

} // namespace vws::application
