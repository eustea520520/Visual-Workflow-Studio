#pragma once

#include "domain/Workflow.h"

#include <QHash>
#include <QString>

namespace vws::presentation {

struct WorkflowDisplayModel {
    QString workflowName;
    QHash<QString, QString> nodeNamesById;
};

class WorkflowDisplayModelBuilder {
public:
    static WorkflowDisplayModel build(const domain::Workflow& workflow);
};

} // namespace vws::presentation
