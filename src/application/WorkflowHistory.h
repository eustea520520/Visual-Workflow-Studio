#pragma once

#include "domain/Workflow.h"

#include <QList>
#include <optional>

namespace vws::application {

// Stores in-memory undo snapshots for workflow editing.
// UI code decides when an edit begins; this class only owns the bounded history stack.
class WorkflowHistory final {
public:
    void clear();
    void push(const domain::Workflow& workflow);
    std::optional<domain::Workflow> takeUndoSnapshot();
    void setRestoring(bool restoring);
    bool isRestoring() const;
    int size() const;

private:
    QList<domain::Workflow> m_undoStack;
    bool m_restoring = false;
};

} // namespace vws::application
