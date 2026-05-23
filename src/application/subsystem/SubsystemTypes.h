#pragma once

#include "domain/Workflow.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace vws::application {

struct SubsystemBoundaryPort {
    QString externalPort;
    QString internalNodeId;
    QString internalNodeName;
    QString internalPort;
    int dimension = 1;
    QStringList itemLabels;

    QJsonObject toJson() const;
    static SubsystemBoundaryPort fromJson(const QJsonObject& object);
};

struct SubsystemBoundary {
    QList<SubsystemBoundaryPort> inputs;
    QList<SubsystemBoundaryPort> outputs;

    QJsonObject toJson() const;
    static SubsystemBoundary fromJson(const QJsonObject& object);
};

struct SubsystemDocument {
    domain::Workflow workflow;
    SubsystemBoundary boundary;
};

} // namespace vws::application
