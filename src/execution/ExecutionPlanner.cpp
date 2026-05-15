#include "execution/ExecutionPlanner.h"

#include "execution/GraphValidator.h"

namespace vws::execution {

ExecutionPlan ExecutionPlanner::plan(const domain::Workflow& workflow) const
{
    ExecutionPlan executionPlan;

    GraphValidator validator;
    const auto validation = validator.validate(workflow);
    executionPlan.valid = validation.valid;
    executionPlan.errors = validation.errors;
    executionPlan.warnings = validation.warnings;

    if (executionPlan.valid) {
        executionPlan.indexes.build(workflow);
    }

    return executionPlan;
}

} // namespace vws::execution
