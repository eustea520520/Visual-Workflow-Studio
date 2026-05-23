#pragma once

#include "domain/Workflow.h"

#include <QSet>
#include <QString>

namespace vws::application {

// Stores a copied workflow subgraph and creates fresh duplicated subgraphs for paste.
class WorkflowClipboard final {
public:
    void clear();
    void capture(const domain::Workflow& workflow, const QSet<QString>& selectedNodeIds);
    bool hasNodes() const;
    domain::Workflow createPasteSubgraph(
        const QString& copiedNameSuffix = QString(),
        qreal offsetStep = 32.0);

private:
    domain::Workflow m_copiedSubgraph;
    int m_pasteCount = 0;
};

} // namespace vws::application
