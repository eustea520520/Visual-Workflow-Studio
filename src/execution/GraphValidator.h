#pragma once

#include "domain/Workflow.h"

#include <QString>
#include <QStringList>

namespace vws::execution {

struct GraphValidationResult {
    bool valid = true;
    QStringList errors;
    QStringList warnings;

    void addError(const QString& error);
    void addWarning(const QString& warning);
};

enum class GraphValidationMode {
    TopLevelWorkflow,
    SubsystemWorkflow,
};

class GraphValidator {
public:
    GraphValidationResult validate(
        const domain::Workflow& workflow,
        GraphValidationMode mode = GraphValidationMode::TopLevelWorkflow) const;

private:
    void validateNodes(const domain::Workflow& workflow, GraphValidationMode mode, GraphValidationResult& result) const;
    void validateEdges(const domain::Workflow& workflow, GraphValidationResult& result) const;
    void validateLoopNodes(const domain::Workflow& workflow, GraphValidationResult& result) const;
    void validateStarterReachability(const domain::Workflow& workflow, GraphValidationMode mode, GraphValidationResult& result) const;
    void validateAcyclic(const domain::Workflow& workflow, GraphValidationResult& result) const;
};

} // namespace vws::execution
