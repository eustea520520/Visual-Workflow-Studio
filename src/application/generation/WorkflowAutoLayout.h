#pragma once

#include "domain/Workflow.h"

namespace vws::application {

class WorkflowAutoLayout final {
public:
    void applyIfNeeded(domain::Workflow& workflow) const;

private:
    bool needsLayout(const domain::Workflow& workflow) const;
    void applyLeftToRightLayout(domain::Workflow& workflow) const;
};

} // namespace vws::application
