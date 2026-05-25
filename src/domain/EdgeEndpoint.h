#pragma once

#include <QString>

namespace vws::domain {

struct EdgeEndpoint {
    QString nodeId;
    QString portName;
    int slotIndex = 0;

    bool isValid() const
    {
        return !nodeId.trimmed().isEmpty()
            && !portName.trimmed().isEmpty()
            && slotIndex >= 0;
    }
};

} // namespace vws::domain
