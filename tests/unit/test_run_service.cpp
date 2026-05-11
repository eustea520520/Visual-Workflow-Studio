#include "application/RunService.h"
#include "execution/ExecutionEngine.h"
#include "workers/WorkerRegistry.h"

#include <QCoreApplication>
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

    vws::execution::WorkflowExecutionResult result;
    result.runId = "run-abc12345";
    result.success = true;
    result.status = "Succeeded";
    result.nodeStatuses.insert("node-a", "Succeeded");

    QString errorMessage;
    if (!runService.saveRunRecord(tempDir.path(), "workspace-1", "workflow-1", result, &errorMessage)) {
        return fail(QString("Could not save run record: %1").arg(errorMessage));
    }

    const auto runs = runService.recentRuns(tempDir.path());
    if (const auto check = expect(runs.size() == 1, "recentRuns should include saved run record")) {
        return check;
    }
    if (const auto check = expect(runs.first().contains("Succeeded"), "recentRuns should show run status")) {
        return check;
    }

    QTextStream(stdout) << "run service tests passed" << Qt::endl;
    return 0;
}
