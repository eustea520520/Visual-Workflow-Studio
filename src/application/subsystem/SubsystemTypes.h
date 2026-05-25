#pragma once

#include "domain/Workflow.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace vws::application {

struct SubsystemBoundaryPort {
    // Stable connection key used by parent workflow edges.
    QString externalPort;
    // Human-readable label derived from the current internal node name and port.
    QString displayName;
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
