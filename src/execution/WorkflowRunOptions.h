#pragma once

#include "execution/GraphValidator.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <functional>

namespace vws::execution {

struct WorkflowRunOptions {
    QHash<QString, QJsonObject> initialInputsByNodeId;
    GraphValidationMode validationMode = GraphValidationMode::TopLevelWorkflow;
    bool allowImplicitEntryNodes = false;
    QString runIdOverride;
    int nodeDispatchDelayMs = 0;
    std::function<bool()> cancelPredicate;
};

} // namespace vws::execution
