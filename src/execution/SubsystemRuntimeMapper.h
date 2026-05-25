#pragma once

#include "execution/WorkflowExecutionResult.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace vws::execution {

struct SubsystemBoundaryPortRuntime {
    QString externalPort;
    QString displayName;
    QString internalNodeId;
    QString internalPort;
};

class SubsystemRuntimeMapper final {
public:
    QHash<QString, QJsonObject> mapExternalInputsToInternalNodes(
        const QJsonObject& boundary,
        const QJsonObject& externalInputs) const;

    QJsonObject mapInternalOutputsToExternalPorts(
        const QJsonObject& boundary,
        const WorkflowExecutionResult& internalResult) const;

private:
    static QList<SubsystemBoundaryPortRuntime> boundaryPorts(
        const QJsonObject& boundary,
        const QString& key);
};

} // namespace vws::execution
