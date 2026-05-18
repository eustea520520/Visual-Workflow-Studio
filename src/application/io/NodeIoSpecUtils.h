#pragma once

#include "domain/Node.h"
#include "domain/NodeIoSpec.h"

#include <QJsonObject>
#include <QStringList>

namespace vws::application {

class NodeIoSpecUtils final {
public:
    static constexpr int MaxVisiblePortSlots = 32;

    static int normalizeDimension(int dimension);
    static QStringList labelsForDimension(int dimension, const QStringList& labels = {});
    static domain::PortDimensionSpec makePortSpec(
        const QString& portName,
        int dimension,
        const QString& source,
        const QStringList& labels = {},
        const QString& description = {});
    static domain::NodeIoSpec defaultSpecForNode(const domain::Node& node);
    static domain::NodeIoSpec merged(const domain::NodeIoSpec& base, const domain::NodeIoSpec& patch);
};

} // namespace vws::application
