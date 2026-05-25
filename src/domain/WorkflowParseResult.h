#pragma once

#include "domain/Workflow.h"

#include <QStringList>

namespace vws::domain {

struct WorkflowParseResult {
    bool success = false;
    Workflow workflow;
    QStringList errors;
};

} // namespace vws::domain
