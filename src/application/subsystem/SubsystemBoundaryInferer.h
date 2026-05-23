#pragma once

#include "application/subsystem/SubsystemTypes.h"

#include <QStringList>

namespace vws::application {

class SubsystemBoundaryInferer final {
public:
    SubsystemBoundary infer(const domain::Workflow& subWorkflow, QStringList* warnings = nullptr) const;
};

} // namespace vws::application
