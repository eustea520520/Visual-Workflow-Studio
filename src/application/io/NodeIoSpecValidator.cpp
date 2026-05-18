#include "application/io/NodeIoSpecValidator.h"

#include "application/io/NodeIoSpecUtils.h"

namespace vws::application {

bool NodeIoSpecValidator::validate(const domain::NodeIoSpec& spec, QStringList& errors) const
{
    auto validateSide = [&errors](const QList<domain::PortDimensionSpec>& ports, const QString& side) {
        for (const auto& port : ports) {
            if (port.portName.trimmed().isEmpty()) {
                errors.append(QStringLiteral("%1 port name must not be empty.").arg(side));
            }
            if (port.dimension < 1 || port.dimension > NodeIoSpecUtils::MaxVisiblePortSlots) {
                errors.append(QStringLiteral("%1 port %2 dimension must be between 1 and %3.")
                    .arg(side, port.portName)
                    .arg(NodeIoSpecUtils::MaxVisiblePortSlots));
            }
            if (!port.itemLabels.isEmpty() && port.itemLabels.size() != port.dimension) {
                errors.append(QStringLiteral("%1 port %2 item_labels size must match dimension.")
                    .arg(side, port.portName));
            }
        }
    };

    validateSide(spec.inputs, QStringLiteral("input"));
    validateSide(spec.outputs, QStringLiteral("output"));
    return errors.isEmpty();
}

} // namespace vws::application
