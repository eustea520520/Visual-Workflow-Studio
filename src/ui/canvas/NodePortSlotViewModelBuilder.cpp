#include "ui/canvas/NodePortSlotViewModelBuilder.h"

#include "application/io/NodeIoSpecUtils.h"

namespace vws::ui {

namespace {

QList<NodePortSlotViewModel> buildSide(
    const QStringList& logicalPorts,
    const QList<domain::PortDimensionSpec>& specs)
{
    QList<NodePortSlotViewModel> portSlots;

    for (const auto& portName : logicalPorts) {
        domain::PortDimensionSpec spec;
        spec.portName = portName;
        spec.dimension = 1;
        spec.itemLabels = {QStringLiteral("1")};

        for (const auto& candidate : specs) {
            if (candidate.portName == portName) {
                spec = candidate;
                break;
            }
        }

        const auto dimension = application::NodeIoSpecUtils::normalizeDimension(spec.dimension);
        const auto labels = application::NodeIoSpecUtils::labelsForDimension(dimension, spec.itemLabels);
        for (int index = 0; index < dimension; ++index) {
            portSlots.append({portName, index, labels.value(index, QString::number(index + 1))});
        }
    }

    return portSlots;
}

} // namespace

NodePortSlots NodePortSlotViewModelBuilder::build(const domain::Node& node, const domain::NodeIoSpec& spec) const
{
    return {
        buildSide(node.inputPorts, spec.inputs),
        buildSide(node.outputPorts, spec.outputs),
    };
}

} // namespace vws::ui
