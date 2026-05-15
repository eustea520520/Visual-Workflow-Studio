#include "application/WorkflowDocument.h"

namespace vws::application {

bool WorkflowDocument::hasWorkflow() const
{
    return !m_workflow.workflowId.trimmed().isEmpty();
}

bool WorkflowDocument::isDirty() const
{
    return m_dirty;
}

int WorkflowDocument::revision() const
{
    return m_revision;
}

const domain::Workflow& WorkflowDocument::workflow() const
{
    return m_workflow;
}

domain::Workflow WorkflowDocument::snapshot() const
{
    return m_workflow;
}

void WorkflowDocument::replace(const domain::Workflow& workflow, ChangeState state)
{
    m_workflow = workflow;
    m_dirty = state == ChangeState::Dirty;
    ++m_revision;
}

void WorkflowDocument::replaceFromView(const domain::Workflow& workflow)
{
    replace(workflow, ChangeState::Dirty);
}

void WorkflowDocument::clear()
{
    m_workflow = {};
    m_dirty = false;
    ++m_revision;
}

void WorkflowDocument::markDirty()
{
    m_dirty = true;
    ++m_revision;
}

void WorkflowDocument::markSaved()
{
    m_dirty = false;
}

domain::Workflow& WorkflowDocument::mutableWorkflow()
{
    return m_workflow;
}

} // namespace vws::application
