#include "application/WorkflowClipboard.h"

#include "application/WorkflowEditService.h"

namespace vws::application {

void WorkflowClipboard::clear()
{
    m_copiedSubgraph = {};
    m_pasteCount = 0;
}

void WorkflowClipboard::capture(const domain::Workflow& workflow, const QSet<QString>& selectedNodeIds)
{
    m_copiedSubgraph = WorkflowEditService::subgraphForNodes(workflow, selectedNodeIds);
    m_pasteCount = 0;
}

bool WorkflowClipboard::hasNodes() const
{
    return !m_copiedSubgraph.nodes.isEmpty();
}

domain::Workflow WorkflowClipboard::createPasteSubgraph(const QString& copiedNameSuffix, qreal offsetStep)
{
    if (!hasNodes()) {
        return {};
    }

    ++m_pasteCount;
    return WorkflowEditService::duplicateSubgraph(
        m_copiedSubgraph,
        offsetStep * m_pasteCount,
        copiedNameSuffix);
}

} // namespace vws::application
