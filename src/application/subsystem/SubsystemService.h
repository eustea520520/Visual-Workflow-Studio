#pragma once

#include "application/subsystem/SubsystemBoundaryInferer.h"
#include "domain/Node.h"
#include "domain/Workflow.h"

#include <QPointF>
#include <QStringList>

namespace vws::application {

class SubsystemService final {
public:
    bool isSubsystemNode(const domain::Node& node) const;

    domain::Node createSubsystemNode(
        const QString& workspaceId,
        const QString& name,
        const domain::NodePosition& position) const;

    bool loadSubsystemWorkflow(
        const domain::Node& subsystemNode,
        domain::Workflow& subWorkflow,
        QString* errorMessage = nullptr) const;

    bool saveSubsystemWorkflow(
        domain::Node& subsystemNode,
        const domain::Workflow& subWorkflow,
        QString* errorMessage = nullptr) const;

    bool refreshSubsystemBoundary(
        domain::Node& subsystemNode,
        QStringList* warnings = nullptr,
        QString* errorMessage = nullptr) const;

    QString breadcrumbLabel(const domain::Node& subsystemNode) const;

private:
    static domain::Workflow createEmbeddedWorkflow(
        const QString& workspaceId,
        const QString& nodeId,
        const QString& name);
    static void applyBoundary(domain::Node& subsystemNode, const SubsystemBoundary& boundary);

    SubsystemBoundaryInferer m_boundaryInferer;
};

} // namespace vws::application
