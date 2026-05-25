#pragma once

#include "application/subsystem/SubsystemTypes.h"

#include <QStringList>

namespace vws::application {

class SubsystemBoundaryInferer final {
public:
    SubsystemBoundary infer(
        const domain::Workflow& subWorkflow,
        const SubsystemBoundary& previousBoundary = {},
        QStringList* warnings = nullptr) const;
};

} // namespace vws::application
