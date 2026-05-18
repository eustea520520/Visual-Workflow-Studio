#pragma once

#include "domain/EdgeEndpoint.h"

#include <QString>

namespace vws::ui {

enum class PortDirection {
    Input,
    Output,
};

struct PortSlotHit {
    QString nodeId;
    QString portName;
    int slotIndex = -1;
    PortDirection direction = PortDirection::Input;

    domain::EdgeEndpoint toEndpoint() const
    {
        return {nodeId, portName, slotIndex};
    }
};

} // namespace vws::ui
