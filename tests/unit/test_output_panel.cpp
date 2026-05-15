#include "domain/Artifact.h"
#include "ui/output/OutputPanel.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTextStream>

#include "execution/WorkflowExecutionResult.h"

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
    QApplication app(argc, argv);

    // Verify timeline rows, node result rows, artifacts, previews, and thread trace display.
    vws::ui::OutputPanel panel;
    panel.clearRun();
    panel.setWorkflowName("Demo Workflow");
    panel.setNodeNames({{"node-a", "Calculator"}});
    panel.recordWorkflowStatus("run-1", "Running");
    panel.recordNodeStatus("run-1", "node-a", "Queued");
    panel.recordNodeStatus("run-1", "node-a", "Running");
    panel.recordNodeOutput("run-1", "node-a", {{"result", 3}});
    panel.recordThreadTrace("run-1", "node-a", "Node worker thread started", "abc123", "worker-1");

    if (const auto check = expect(panel.timelineRowCount() >= 4, "Timeline should record workflow and node events")) {
        return check;
    }
    if (const auto check = expect(panel.nodeRunRowCount() == 1, "Node Runs should upsert one row per node")) {
        return check;
    }
    if (const auto check = expect(panel.threadTraceRowCount() == 1, "Thread Trace should record execution thread events")) {
        return check;
    }

    vws::execution::NodeExecutionResult nodeResult;
    nodeResult.runId = "run-1";
    nodeResult.nodeId = "node-a";
    nodeResult.success = true;
    nodeResult.stdoutText = "debug value from print()";
    nodeResult.outputs = QJsonObject{{"output", 3}};

    vws::execution::WorkflowExecutionResult executionResult;
    executionResult.runId = "run-1";
    executionResult.success = true;
    executionResult.status = "Succeeded";
    executionResult.nodeResults.insert("node-a", nodeResult);
    panel.showExecutionResult(executionResult);

    auto* debugOutputView = panel.findChild<QPlainTextEdit*>("debugOutputView");
    if (const auto check = expect(debugOutputView != nullptr, "OutputPanel should expose a debug output view")) {
        return check;
    }
    if (const auto check = expect(debugOutputView->toPlainText().contains("debug value from print()"),
            "Python print() output should appear in Debug Output")) {
        return check;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return fail("Could not create temporary directory");
    }

    const auto artifactPath = QDir(tempDir.path()).filePath("table.csv");
    QFile artifactFile(artifactPath);
    if (!artifactFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return fail("Could not write artifact fixture");
    }
    artifactFile.write("a,b\n1,2\n3,4\n5,6\n");
    artifactFile.close();

    vws::domain::Artifact artifact;
    artifact.artifactId = "artifact-1";
    artifact.runId = "run-1";
    artifact.nodeId = "node-a";
    artifact.type = "csv";
    artifact.path = artifactPath;
    panel.showArtifacts({artifact});

    if (const auto check = expect(panel.artifactRowCount() == 1, "Artifact table should show one artifact")) {
        return check;
    }

    const auto preview = panel.previewArtifactRows(artifactPath, 2);
    if (const auto check = expect(preview.contains("a,b") && preview.contains("1,2") && !preview.contains("3,4"),
            "Artifact preview should include only the requested first rows")) {
        return check;
    }

    QTextStream(stdout) << "output panel tests passed" << Qt::endl;
    return 0;
}
