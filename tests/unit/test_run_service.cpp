#include "application/RunService.h"
#include "domain/Workflow.h"
#include "execution/ExecutionEngine.h"
#include "workers/WorkerRegistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
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

    vws::workers::WorkerRegistry registry;
    vws::execution::ExecutionEngine engine(registry);
    vws::application::RunService runService(engine);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return fail("Could not create temporary directory");
    }

    vws::domain::Workflow workflowSnapshot;
    workflowSnapshot.workflowId = "workflow-1";
    workflowSnapshot.name = "Test Workflow";
    workflowSnapshot.workspaceId = "workspace-1";

    vws::execution::WorkflowExecutionResult result;
    result.runId = "run-abc12345";
    result.success = true;
    result.status = "Succeeded";
    result.nodeStatuses.insert("node-a", "Succeeded");

    QString errorMessage;
    if (!runService.saveRunRecord(tempDir.path(), "workspace-1", workflowSnapshot, result, &errorMessage)) {
        return fail(QString("Could not save run record: %1").arg(errorMessage));
    }

    // Verify workflow_snapshot.json was saved
    const auto snapshotPath = QDir(tempDir.path()).filePath("runs/run-abc12345/workflow_snapshot.json");
    if (const auto check = expect(QFileInfo::exists(snapshotPath),
            "saveRunRecord should save workflow_snapshot.json")) {
        return check;
    }

    // Verify run_record.json contains workflow_snapshot_path
    vws::domain::RunRecord loadedRecord;
    if (!runService.loadRunRecord(tempDir.path(), "run-abc12345", loadedRecord, &errorMessage)) {
        return fail(QString("Could not load run record: %1").arg(errorMessage));
    }
    if (const auto check = expect(!loadedRecord.workflowSnapshotPath.isEmpty(),
            "run_record.json should contain workflow_snapshot_path")) {
        return check;
    }

    // Verify recentRunEntries
    const auto entries = runService.recentRunEntries(tempDir.path());
    if (const auto check = expect(entries.size() == 1, "recentRunEntries should include saved run")) {
        return check;
    }
    if (const auto check = expect(entries.first().runId == "run-abc12345",
            "recentRunEntries should return correct runId")) {
        return check;
    }

    // Verify recentRuns still works
    const auto runs = runService.recentRuns(tempDir.path());
    if (const auto check = expect(runs.size() == 1, "recentRuns should include saved run record")) {
        return check;
    }
    if (const auto check = expect(runs.first().contains("Succeeded"), "recentRuns should show run status")) {
        return check;
    }

    // Verify loadNodeOutputObject returns false for nonexistent file
    vws::domain::NodeRunRecord nodeRun;
    nodeRun.outputPath = tempDir.path() + "/nonexistent.json";
    QJsonObject outputObject;
    if (const auto check = expect(!runService.loadNodeOutputObject(nodeRun, outputObject, nullptr),
            "loadNodeOutputObject should return false for nonexistent file")) {
        return check;
    }

    // Verify loadNodeOutputObject returns false for empty path
    vws::domain::NodeRunRecord emptyNodeRun;
    if (const auto check = expect(!runService.loadNodeOutputObject(emptyNodeRun, outputObject, nullptr),
            "loadNodeOutputObject should return false for empty output path")) {
        return check;
    }

    QTextStream(stdout) << "run service tests passed" << Qt::endl;
    return 0;
}
