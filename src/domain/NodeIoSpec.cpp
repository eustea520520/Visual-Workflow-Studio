#include "domain/NodeIoSpec.h"

#include <QJsonArray>
#include <QtGlobal>

namespace vws::domain {

namespace {

int normalizedDimension(int dimension)
{
    return qBound(1, dimension, 32);
}

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

QJsonArray portSpecsToJson(const QList<PortDimensionSpec>& specs)
{
    QJsonArray array;
    for (const auto& spec : specs) {
        array.append(spec.toJson());
    }
    return array;
}

QList<PortDimensionSpec> portSpecsFromJson(const QJsonArray& array)
{
    QList<PortDimensionSpec> specs;
    for (const auto& value : array) {
        specs.append(PortDimensionSpec::fromJson(value.toObject()));
    }
    return specs;
}

} // namespace

QJsonObject PortDimensionSpec::toJson() const
{
    return {
        {"port_name", portName},
        {"dimension", normalizedDimension(dimension)},
        {"source", source.trimmed().isEmpty() ? QStringLiteral("default") : source},
        {"description", description},
        {"item_labels", stringListToJson(itemLabels)},
    };
}

PortDimensionSpec PortDimensionSpec::fromJson(const QJsonObject& object)
{
    PortDimensionSpec spec;
    spec.portName = object.value("port_name").toString();
    spec.dimension = normalizedDimension(object.value("dimension").toInt(1));
    spec.source = object.value("source").toString("default");
    spec.description = object.value("description").toString();
    spec.itemLabels = stringListFromJson(object.value("item_labels").toArray());
    return spec;
}

bool NodeIoSpec::isEmpty() const
{
    return inputs.isEmpty() && outputs.isEmpty();
}

QJsonObject NodeIoSpec::toJson() const
{
    return {
        {"inputs", portSpecsToJson(inputs)},
        {"outputs", portSpecsToJson(outputs)},
    };
}

NodeIoSpec NodeIoSpec::fromJson(const QJsonObject& object)
{
    NodeIoSpec spec;
    spec.inputs = portSpecsFromJson(object.value("inputs").toArray());
    spec.outputs = portSpecsFromJson(object.value("outputs").toArray());
    return spec;
}

} // namespace vws::domain
