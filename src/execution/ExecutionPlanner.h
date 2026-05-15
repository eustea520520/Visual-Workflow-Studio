#pragma once

#include "domain/Workflow.h"
#include "execution/GraphIndexes.h"

#include <QStringList>

namespace vws::execution {

struct ExecutionPlan {
    bool valid = false;
    QStringList errors;
    QStringList warnings;
    GraphIndexes indexes;
};

// Validates a workflow and prepares immutable graph indexes for one execution run.
class ExecutionPlanner final {
public:
    ExecutionPlan plan(const domain::Workflow& workflow) const;
};

} // namespace vws::execution
