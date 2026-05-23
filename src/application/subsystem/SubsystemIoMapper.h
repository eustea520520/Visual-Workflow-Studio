#pragma once

#include "application/subsystem/SubsystemTypes.h"
#include "execution/WorkflowExecutionResult.h"

#include <QHash>
#include <QJsonObject>
#include <QString>

namespace vws::application {

class SubsystemIoMapper final {
public:
    QHash<QString, QJsonObject> mapExternalInputsToInternalNodes(
        const SubsystemBoundary& boundary,
        const QJsonObject& externalInputs) const;

    QJsonObject mapInternalOutputsToExternalPorts(
        const SubsystemBoundary& boundary,
        const execution::WorkflowExecutionResult& internalResult) const;
};

} // namespace vws::application
