#include "application/io/NodeIoSpecUtils.h"

#include <QtGlobal>

namespace vws::application {

namespace {

bool isOrdinalLabels(const QStringList& labels)
{
    for (int index = 0; index < labels.size(); ++index) {
        if (labels.at(index) != QString::number(index + 1)) {
            return false;
        }
    }
    return true;
}

bool shouldPreserveExistingLabels(
    const domain::PortDimensionSpec& existing,
    const domain::PortDimensionSpec& update)
{
    return update.source.startsWith(QStringLiteral("runtime"))
        && !existing.itemLabels.isEmpty()
        && !isOrdinalLabels(existing.itemLabels);
}

} // namespace

int NodeIoSpecUtils::normalizeDimension(int dimension)
{
    return qBound(1, dimension, MaxVisiblePortSlots);
}

QStringList NodeIoSpecUtils::labelsForDimension(int dimension, const QStringList& labels)
{
    const auto normalized = normalizeDimension(dimension);
    QStringList result = labels.mid(0, normalized);
    while (result.size() < normalized) {
        result.append(QString::number(result.size() + 1));
    }
    return result;
}

domain::PortDimensionSpec NodeIoSpecUtils::makePortSpec(
    const QString& portName,
    int dimension,
    const QString& source,
    const QStringList& labels,
    const QString& description)
{
    domain::PortDimensionSpec spec;
    spec.portName = portName;
    spec.dimension = normalizeDimension(dimension);
    spec.source = source.trimmed().isEmpty() ? QStringLiteral("default") : source;
    spec.itemLabels = labelsForDimension(spec.dimension, labels);
    spec.description = description;
    return spec;
}

domain::NodeIoSpec NodeIoSpecUtils::defaultSpecForNode(const domain::Node& node)
{
    domain::NodeIoSpec spec;
    for (const auto& port : node.inputPorts) {
        spec.inputs.append(makePortSpec(port, 1, QStringLiteral("default")));
    }
    for (const auto& port : node.outputPorts) {
        spec.outputs.append(makePortSpec(port, 1, QStringLiteral("default")));
    }
    return spec;
}

domain::NodeIoSpec NodeIoSpecUtils::merged(const domain::NodeIoSpec& base, const domain::NodeIoSpec& patch)
{
    auto mergeSide = [](QList<domain::PortDimensionSpec> result, const QList<domain::PortDimensionSpec>& updates) {
        for (const auto& update : updates) {
            bool replaced = false;
            for (auto& existing : result) {
                if (existing.portName == update.portName) {
                    const auto preserveLabels = shouldPreserveExistingLabels(existing, update);
                    auto merged = update;
                    merged.dimension = NodeIoSpecUtils::normalizeDimension(merged.dimension);
                    merged.itemLabels = preserveLabels
                        ? NodeIoSpecUtils::labelsForDimension(merged.dimension, existing.itemLabels)
                        : NodeIoSpecUtils::labelsForDimension(merged.dimension, merged.itemLabels);
                    existing = merged;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                result.append(NodeIoSpecUtils::makePortSpec(
                    update.portName,
                    update.dimension,
                    update.source,
                    update.itemLabels,
                    update.description));
            }
        }
        return result;
    };

    domain::NodeIoSpec result = base;
    result.inputs = mergeSide(result.inputs, patch.inputs);
    result.outputs = mergeSide(result.outputs, patch.outputs);
    return result;
}

} // namespace vws::application
