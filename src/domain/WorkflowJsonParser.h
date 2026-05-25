#pragma once

#include "domain/WorkflowParseResult.h"

#include <QJsonObject>
#include <QString>

namespace vws::domain {

class WorkflowJsonParser final {
public:
    static QString unreadableWorkspaceMessage();
    static WorkflowParseResult parseStrict(const QJsonObject& object);
};

} // namespace vws::domain
