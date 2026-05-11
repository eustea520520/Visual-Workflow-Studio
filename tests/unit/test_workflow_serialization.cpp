#include "application/WorkflowService.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
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

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Verify fixture JSON -> Workflow object -> temporary JSON file -> Workflow object.
    vws::application::WorkflowService service;

    QString errorMessage;
    vws::domain::Workflow simpleWorkflow;
    if (!service.loadWorkflow("tests/fixtures/simple_workflow.json", simpleWorkflow, &errorMessage)) {
        return fail(QString("Failed to load simple workflow: %1").arg(errorMessage));
    }

    if (const auto result = expect(simpleWorkflow.nodes.size() == 2, "Simple workflow should contain 2 nodes")) {
        return result;
    }
    if (const auto result = expect(simpleWorkflow.edges.size() == 1, "Simple workflow should contain 1 edge")) {
        return result;
    }

    vws::domain::Workflow branchingWorkflow;
    if (!service.loadWorkflow("tests/fixtures/branching_workflow.json", branchingWorkflow, &errorMessage)) {
        return fail(QString("Failed to load branching workflow: %1").arg(errorMessage));
    }

    if (const auto result = expect(branchingWorkflow.nodes.size() == 4, "Branching workflow should contain 4 nodes")) {
        return result;
    }
    if (const auto result = expect(branchingWorkflow.edges.size() == 4, "Branching workflow should contain 4 edges")) {
        return result;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return fail("Could not create temporary directory");
    }

    const auto roundTripPath = QDir(tempDir.path()).filePath("roundtrip_workflow.json");
    if (!service.saveWorkflow(roundTripPath, branchingWorkflow, &errorMessage)) {
        return fail(QString("Failed to save workflow: %1").arg(errorMessage));
    }

    vws::domain::Workflow roundTripWorkflow;
    if (!service.loadWorkflow(roundTripPath, roundTripWorkflow, &errorMessage)) {
        return fail(QString("Failed to load roundtrip workflow: %1").arg(errorMessage));
    }

    if (const auto result = expect(roundTripWorkflow.workflowId == branchingWorkflow.workflowId,
            "Roundtrip workflow id should be preserved")) {
        return result;
    }
    if (const auto result = expect(roundTripWorkflow.nodes.size() == branchingWorkflow.nodes.size(),
            "Roundtrip node count should be preserved")) {
        return result;
    }
    if (const auto result = expect(roundTripWorkflow.edges.size() == branchingWorkflow.edges.size(),
            "Roundtrip edge count should be preserved")) {
        return result;
    }

    QTextStream(stdout) << "workflow serialization tests passed" << Qt::endl;
    return 0;
}
