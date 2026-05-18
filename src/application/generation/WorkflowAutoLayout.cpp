#include "application/generation/WorkflowAutoLayout.h"

#include <QHash>
#include <QQueue>

namespace vws::application {

namespace {
constexpr double HorizontalGap = 320.0;
constexpr double VerticalGap = 150.0;
constexpr double StartX = 80.0;
constexpr double StartY = 100.0;
}

void WorkflowAutoLayout::applyIfNeeded(domain::Workflow& workflow) const
{
    if (needsLayout(workflow)) {
        applyLeftToRightLayout(workflow);
    }
}

bool WorkflowAutoLayout::needsLayout(const domain::Workflow& workflow) const
{
    for (const auto& node : workflow.nodes) {
        if (node.position.x == 0.0 && node.position.y == 0.0) {
            return true;
        }
    }

    for (int i = 0; i < workflow.nodes.size(); ++i) {
        for (int j = i + 1; j < workflow.nodes.size(); ++j) {
            const auto& a = workflow.nodes.at(i);
            const auto& b = workflow.nodes.at(j);
            if (qAbs(a.position.x - b.position.x) < 220.0 && qAbs(a.position.y - b.position.y) < 130.0) {
                return true;
            }
        }
    }

    return false;
}

void WorkflowAutoLayout::applyLeftToRightLayout(domain::Workflow& workflow) const
{
    QHash<QString, int> indegree;
    QHash<QString, QStringList> children;
    QHash<QString, int> depthByNodeId;
    for (const auto& node : workflow.nodes) {
        indegree.insert(node.nodeId, 0);
        depthByNodeId.insert(node.nodeId, 0);
    }
    for (const auto& edge : workflow.edges) {
        children[edge.fromNode].append(edge.toNode);
        indegree[edge.toNode] = indegree.value(edge.toNode) + 1;
    }

    QQueue<QString> queue;
    for (const auto& node : workflow.nodes) {
        if (indegree.value(node.nodeId) == 0) {
            queue.enqueue(node.nodeId);
        }
    }

    while (!queue.isEmpty()) {
        const auto nodeId = queue.dequeue();
        for (const auto& childId : children.value(nodeId)) {
            depthByNodeId[childId] = qMax(depthByNodeId.value(childId), depthByNodeId.value(nodeId) + 1);
            indegree[childId] = indegree.value(childId) - 1;
            if (indegree.value(childId) == 0) {
                queue.enqueue(childId);
            }
        }
    }

    QHash<int, int> rowByDepth;
    for (auto& node : workflow.nodes) {
        const auto depth = depthByNodeId.value(node.nodeId);
        const auto row = rowByDepth.value(depth, 0);
        rowByDepth[depth] = row + 1;
        node.position.x = StartX + depth * HorizontalGap;
        node.position.y = StartY + row * VerticalGap;
    }
}

} // namespace vws::application
