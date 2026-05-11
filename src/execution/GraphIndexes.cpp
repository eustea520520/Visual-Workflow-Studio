#include "execution/GraphIndexes.h"

namespace vws::execution {

void GraphIndexes::build(const domain::Workflow& workflow)
{
    nodesById.clear();
    incomingEdgesByNode.clear();
    outgoingEdgesByNode.clear();
    incomingEdgesByNodePort.clear();

    for (const auto& node : workflow.nodes) {
        nodesById.insert(node.nodeId, node);
        incomingEdgesByNode.insert(node.nodeId, {});
        outgoingEdgesByNode.insert(node.nodeId, {});
        incomingEdgesByNodePort.insert(node.nodeId, {});
    }

    for (const auto& edge : workflow.edges) {
        outgoingEdgesByNode[edge.fromNode].append(edge);
        incomingEdgesByNode[edge.toNode].append(edge);
        incomingEdgesByNodePort[edge.toNode][edge.toPort].append(edge);
    }
}

} // namespace vws::execution
