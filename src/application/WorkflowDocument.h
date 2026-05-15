#pragma once

#include "domain/Workflow.h"

namespace vws::application {

// WorkflowDocument is the in-memory owner of the currently opened workflow.
// UI widgets may render snapshots, but edits must be synchronized back through
// this object so dirty state and document lifetime stay explicit.
class WorkflowDocument {
public:
    enum class ChangeState {
        Clean,
        Dirty,
    };

    bool hasWorkflow() const;
    bool isDirty() const;
    int revision() const;

    const domain::Workflow& workflow() const;
    domain::Workflow snapshot() const;

    void replace(const domain::Workflow& workflow, ChangeState state = ChangeState::Clean);
    void replaceFromView(const domain::Workflow& workflow);
    void clear();
    void markDirty();
    void markSaved();

    // Transitional controller-only access. Long term, edit commands should
    // replace mutable access entirely.
    domain::Workflow& mutableWorkflow();

private:
    domain::Workflow m_workflow;
    bool m_dirty = false;
    int m_revision = 0;
};

} // namespace vws::application
