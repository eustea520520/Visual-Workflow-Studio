#include "application/WorkflowHistory.h"

namespace vws::application {

void WorkflowHistory::clear()
{
    m_undoStack.clear();
    m_restoring = false;
}

void WorkflowHistory::push(const domain::Workflow& workflow)
{
    if (m_restoring) {
        return;
    }

    m_undoStack.append(workflow);
    constexpr int MaxUndoStates = 50;
    while (m_undoStack.size() > MaxUndoStates) {
        m_undoStack.removeFirst();
    }
}

std::optional<domain::Workflow> WorkflowHistory::takeUndoSnapshot()
{
    if (m_undoStack.isEmpty()) {
        return std::nullopt;
    }

    return m_undoStack.takeLast();
}

void WorkflowHistory::setRestoring(bool restoring)
{
    m_restoring = restoring;
}

bool WorkflowHistory::isRestoring() const
{
    return m_restoring;
}

int WorkflowHistory::size() const
{
    return m_undoStack.size();
}

} // namespace vws::application
