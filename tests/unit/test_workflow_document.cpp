#include "application/WorkflowDocument.h"
#include "application/WorkflowClipboard.h"
#include "application/WorkflowEditService.h"
#include "application/WorkflowHistory.h"

#include <QSet>
#include <QTextStream>

namespace {

int fail(const QString& message)
{
    QTextStream(stderr) << message << Qt::endl;
    return 1;
}

int expect(bool condition, const QString& message)
{
    return condition ? 0 : fail(message);
}

} // namespace

int main()
{
    vws::application::WorkflowDocument document;

    if (const auto check = expect(!document.hasWorkflow() && !document.isDirty(),
            "New WorkflowDocument should be empty and clean")) {
        return check;
    }

    vws::domain::Workflow workflow;
    workflow.workflowId = "workflow-a";
    workflow.name = "Workflow A";

    document.replace(workflow);
    if (const auto check = expect(document.hasWorkflow() && !document.isDirty(),
            "Replacing from persistence should create a clean document")) {
        return check;
    }

    auto snapshot = document.snapshot();
    snapshot.name = "Mutated Snapshot";
    if (const auto check = expect(document.workflow().name == "Workflow A",
            "WorkflowDocument snapshots should not let callers mutate the owned workflow")) {
        return check;
    }

    snapshot.workflowId = "workflow-a";
    document.replaceFromView(snapshot);
    if (const auto check = expect(document.isDirty() && document.workflow().name == "Mutated Snapshot",
            "Replacing from a view snapshot should mark the document dirty")) {
        return check;
    }

    document.markSaved();
    if (const auto check = expect(!document.isDirty(), "markSaved should clear dirty state")) {
        return check;
    }

    document.clear();
    if (const auto check = expect(!document.hasWorkflow() && !document.isDirty(),
            "clear should reset the document to an empty clean state")) {
        return check;
    }

    vws::domain::Workflow editableWorkflow;
    editableWorkflow.workflowId = "editable-workflow";

    vws::domain::Node sourceNode;
    sourceNode.nodeId = "source";
    sourceNode.name = "Source";
    sourceNode.outputPorts = {"output"};

    vws::domain::Node targetNode;
    targetNode.nodeId = "target";
    targetNode.name = "Target";
    targetNode.inputPorts = {"input"};

    vws::application::WorkflowEditService::addNode(editableWorkflow, sourceNode);
    vws::application::WorkflowEditService::addNode(editableWorkflow, targetNode);

    vws::domain::Edge createdEdge;
    if (const auto check = expect(vws::application::WorkflowEditService::connectNodes(
            editableWorkflow,
            editableWorkflow.nodes.first(),
            editableWorkflow.nodes.last(),
            createdEdge),
            "WorkflowEditService should connect compatible nodes")) {
        return check;
    }
    if (const auto check = expect(editableWorkflow.edges.size() == 1
            && editableWorkflow.edges.first().fromNode == "source"
            && editableWorkflow.edges.first().toNode == "target"
            && editableWorkflow.edges.first().fromSlot == 0
            && editableWorkflow.edges.first().toSlot == 0,
            "WorkflowEditService should append a slot-0 edge to the workflow")) {
        return check;
    }
    QString connectError;
    if (const auto check = expect(!vws::application::WorkflowEditService::canConnect(
                editableWorkflow,
                vws::domain::EdgeEndpoint{"source", "output", -1},
                vws::domain::EdgeEndpoint{"target", "input", 0},
                &connectError)
            && !connectError.isEmpty(),
            "WorkflowEditService should reject negative source slots with a clear error")) {
        return check;
    }
    connectError.clear();
    if (const auto check = expect(!vws::application::WorkflowEditService::canConnect(
                editableWorkflow,
                vws::domain::EdgeEndpoint{"source", "output", 1},
                vws::domain::EdgeEndpoint{"target", "input", 0},
                &connectError)
            && !connectError.isEmpty(),
            "WorkflowEditService should reject source slots outside the output dimension")) {
        return check;
    }

    const auto subgraph = vws::application::WorkflowEditService::subgraphForNodes(
        editableWorkflow,
        QSet<QString>{"source", "target"});
    const auto duplicate = vws::application::WorkflowEditService::duplicateSubgraph(subgraph, 32.0);
    if (const auto check = expect(duplicate.nodes.size() == 2
            && duplicate.edges.size() == 1
            && duplicate.edges.first().fromNode != "source"
            && duplicate.edges.first().toNode != "target",
            "WorkflowEditService should duplicate selected nodes and remap internal edges")) {
        return check;
    }

    vws::application::WorkflowEditService::appendSubgraph(editableWorkflow, duplicate);
    if (const auto check = expect(editableWorkflow.nodes.size() == 4 && editableWorkflow.edges.size() == 2,
            "WorkflowEditService should append duplicated subgraphs")) {
        return check;
    }

    vws::application::WorkflowEditService::removeNodes(editableWorkflow, QSet<QString>{"source"});
    if (const auto check = expect(editableWorkflow.nodes.size() == 3 && editableWorkflow.edges.size() == 1,
            "Removing a node should also remove its connected edges")) {
        return check;
    }

    vws::application::WorkflowHistory history;
    history.push(editableWorkflow);
    editableWorkflow.name = "Edited";
    history.push(editableWorkflow);
    if (const auto check = expect(history.size() == 2,
            "WorkflowHistory should store undo snapshots")) {
        return check;
    }
    history.setRestoring(true);
    history.push(editableWorkflow);
    history.setRestoring(false);
    if (const auto check = expect(history.size() == 2,
            "WorkflowHistory should ignore pushes while restoring")) {
        return check;
    }
    const auto undoSnapshot = history.takeUndoSnapshot();
    if (const auto check = expect(undoSnapshot.has_value() && undoSnapshot->name == "Edited",
            "WorkflowHistory should return the most recent undo snapshot")) {
        return check;
    }

    vws::application::WorkflowClipboard clipboard;
    clipboard.capture(editableWorkflow, QSet<QString>{editableWorkflow.nodes.first().nodeId});
    if (const auto check = expect(clipboard.hasNodes(),
            "WorkflowClipboard should capture selected nodes")) {
        return check;
    }
    const auto pasted = clipboard.createPasteSubgraph();
    if (const auto check = expect(pasted.nodes.size() == 1
            && pasted.nodes.first().nodeId != editableWorkflow.nodes.first().nodeId,
            "WorkflowClipboard should create a pasted subgraph with fresh node ids")) {
        return check;
    }
    if (const auto check = expect(pasted.nodes.first().name == editableWorkflow.nodes.first().name,
            "WorkflowClipboard should not append a copy suffix to pasted node titles by default")) {
        return check;
    }

    QTextStream(stdout) << "workflow document tests passed" << Qt::endl;
    return 0;
}
