#pragma once

#include "application/PythonCodeTemplates.h"
#include "domain/Node.h"

#include <QPointF>

namespace vws::application {

// NodeFactory centralizes default node construction. The canvas asks for a node
// at a position, but no longer knows Python template keys or Agent defaults.
class NodeFactory final {
public:
    enum class StarterTemplateKind {
        EmptyOutput,
        DataOutput,
        FileOutput,
    };

    static domain::Node createStarterNode(
        const QPointF& scenePos,
        qsizetype existingNodeCount,
        StarterTemplateKind templateKind = StarterTemplateKind::DataOutput);
    static domain::Node createFunctionNode(
        const QPointF& scenePos,
        qsizetype existingNodeCount,
        DataTransferTemplate templateKind = DataTransferTemplate::DataToData);
    static domain::Node createAgentNode(
        const QPointF& scenePos,
        qsizetype existingNodeCount,
        DataTransferTemplate templateKind = DataTransferTemplate::DataToData);
};

} // namespace vws::application
