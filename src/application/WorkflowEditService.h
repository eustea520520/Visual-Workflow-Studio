#pragma once

#include "domain/EdgeEndpoint.h"
#include "domain/Workflow.h"

#include <QSet>
#include <QString>

namespace vws::application {

// WorkflowEditService contains pure workflow graph mutations.
// It has no QWidget/QGraphicsItem dependency, so canvas interaction code can stay focused on UI events.
class WorkflowEditService final {
public:
    static domain::Node addNode(domain::Workflow& workflow, domain::Node node);
    static bool updateNode(domain::Workflow& workflow, const domain::Node& node);
    static bool rotateNode(domain::Workflow& workflow, const QString& nodeId, int deltaDegrees);
    static bool connectNodes(
        domain::Workflow& workflow,
        const domain::Node& sourceNode,
        const domain::Node& targetNode,
        domain::Edge& createdEdge);
    static bool connectNodes(
        domain::Workflow& workflow,
        const domain::EdgeEndpoint& source,
        const domain::EdgeEndpoint& target,
        domain::Edge& createdEdge);
    static bool canConnect(
        const domain::Workflow& workflow,
        const domain::EdgeEndpoint& source,
        const domain::EdgeEndpoint& target,
        QString* errorMessage = nullptr);
    static void removeEdges(domain::Workflow& workflow, const QSet<QString>& edgeIds);
    static void removeNodes(domain::Workflow& workflow, const QSet<QString>& nodeIds);
    static domain::Workflow subgraphForNodes(
        const domain::Workflow& workflow,
        const QSet<QString>& nodeIds);
    static domain::Workflow duplicateSubgraph(
        const domain::Workflow& sourceSubgraph,
        qreal offset,
        const QString& copiedNameSuffix = QString());
    static void appendSubgraph(domain::Workflow& workflow, const domain::Workflow& subgraph);
};

} // namespace vws::application
