#pragma once

#include <QString>

namespace vws::domain {

struct EdgeEndpoint {
    QString nodeId;
    QString portName;
    int slotIndex = -1;
};

} // namespace vws::domain
