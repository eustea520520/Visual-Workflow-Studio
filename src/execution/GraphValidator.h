#pragma once

#include "domain/Workflow.h"

#include <QString>
#include <QStringList>

namespace vws::execution {

// 图校验结果。valid 为 false 时，errors 会说明所有发现的问题。
struct GraphValidationResult {
    bool valid = true;
    QStringList errors;
    QStringList warnings;

    void addError(const QString& error);
    void addWarning(const QString& warning);
};

// GraphValidator 只负责回答一个问题：这个工作流图能不能被执行。
// 它不计算执行顺序，也不调用 Worker。
class GraphValidator {
public:
    GraphValidationResult validate(const domain::Workflow& workflow) const;

private:
    void validateNodes(const domain::Workflow& workflow, GraphValidationResult& result) const;
    void validateEdges(const domain::Workflow& workflow, GraphValidationResult& result) const;
    void validateStarterReachability(const domain::Workflow& workflow, GraphValidationResult& result) const;
    void validateAcyclic(const domain::Workflow& workflow, GraphValidationResult& result) const;
};

} // namespace vws::execution
