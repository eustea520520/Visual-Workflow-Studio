#include "execution/NodeExecutionRequest.h"
#include "workers/PythonNodeWorker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

const QString kPythonExecutable = "C:/Users/19272/anaconda3/python.exe";
const QString kWorkerScript = "python/python_worker.py";

int fail(const QString& message)
{
    QTextStream(stderr) << message << Qt::endl;
    return 1;
}

int expect(bool condition, const QString& message)
{
    return condition ? 0 : fail(message);
}

vws::execution::NodeExecutionRequest makeRequest(const QString& runPath, const QString& code)
{
    vws::execution::NodeExecutionRequest request;
    request.runId = "test-run";
    request.nodeId = "python-node";
    request.nodeType = "function";
    request.nodeConfig = {
        {"language", "python"},
        {"entry", "run"},
        {"code", code},
    };
    request.inputs = {
        {"value", 41},
    };
    request.runPath = runPath;
    request.artifactPath = QDir(runPath).filePath("artifacts");
    request.timeoutMs = 30000;
    return request;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Verify the C++ QProcess -> python_worker.py -> user run() -> JSON response path.
    if (const auto result = expect(QFileInfo::exists(kPythonExecutable),
            QString("Python interpreter does not exist: %1").arg(kPythonExecutable))) {
        return result;
    }
    if (const auto result = expect(QFileInfo::exists(kWorkerScript),
            QString("Python worker script does not exist: %1").arg(kWorkerScript))) {
        return result;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return fail("Could not create temporary directory");
    }

    vws::workers::PythonNodeWorker worker(kPythonExecutable, kWorkerScript);

    const auto successCode =
        "def run(inputs, context):\n"
        "    print('hello from python node')\n"
        "    return {'outputs': {'answer': inputs['value'] + 1}, 'metadata': {'debug': {'source': 'test'}}, 'artifacts': []}\n";

    const auto successRequest = makeRequest(tempDir.path(), successCode);
    const auto successResult = worker.execute(successRequest);

    if (const auto result = expect(successResult.success, successResult.errorMessage)) {
        return result;
    }
    if (const auto result = expect(successResult.outputs.value("answer").toInt() == 42,
            "Python output should contain answer=42")) {
        return result;
    }
    if (const auto result = expect(successResult.metadata.value("debug").toObject().value("source").toString() == "test",
            "Python metadata should be returned to C++ outside outputs")) {
        return result;
    }
    if (const auto result = expect(successResult.stdoutText.contains("hello from python node"),
            "Python stdout should be captured")) {
        return result;
    }
    if (const auto result = expect(QFileInfo::exists(QDir(tempDir.path()).filePath("python-node_output.json")),
            "Python node output JSON should be saved")) {
        return result;
    }

    const auto artifactCode =
        "import json\n"
        "import os\n"
        "\n"
        "def run(inputs, context):\n"
        "    artifact_dir = context['artifact_path']\n"
        "    os.makedirs(artifact_dir, exist_ok=True)\n"
        "    file_path = os.path.join(artifact_dir, 'large_data.jsonl')\n"
        "    with open(file_path, 'w', encoding='utf-8') as f:\n"
        "        f.write(json.dumps({'value': 42}, ensure_ascii=False) + '\\n')\n"
        "    return {\n"
        "        'outputs': {'output': {'file_path': file_path}},\n"
        "        'artifacts': [{'type': 'jsonl', 'path': file_path, 'metadata': {'rows': 1}}],\n"
        "    }\n";

    const auto artifactRequest = makeRequest(tempDir.path(), artifactCode);
    const auto artifactResult = worker.execute(artifactRequest);
    if (const auto result = expect(artifactResult.success, artifactResult.errorMessage)) {
        return result;
    }
    if (const auto result = expect(artifactResult.artifacts.size() == 1,
            "Python artifacts should be returned to C++")) {
        return result;
    }
    if (const auto result = expect(QFileInfo::exists(artifactResult.artifacts.first().path),
            QString("Returned artifact path should exist: %1").arg(artifactResult.artifacts.first().path))) {
        return result;
    }

    auto unicodeArtifactRequest = makeRequest(tempDir.path(), artifactCode);
    unicodeArtifactRequest.artifactPath = QDir(tempDir.path()).filePath("测试 artifacts");
    const auto unicodeArtifactResult = worker.execute(unicodeArtifactRequest);
    if (const auto result = expect(unicodeArtifactResult.success, unicodeArtifactResult.errorMessage)) {
        return result;
    }
    if (const auto result = expect(QFileInfo::exists(unicodeArtifactResult.artifacts.first().path),
            QString("Returned artifact path with Chinese characters should exist: %1")
                .arg(unicodeArtifactResult.artifacts.first().path))) {
        return result;
    }

    const auto failureCode =
        "def run(inputs, context):\n"
        "    print('before failure')\n"
        "    raise ValueError('expected failure')\n";

    const auto failureRequest = makeRequest(tempDir.path(), failureCode);
    const auto failureResult = worker.execute(failureRequest);

    if (const auto result = expect(!failureResult.success, "Python failure should return success=false")) {
        return result;
    }
    if (const auto result = expect(failureResult.errorMessage.contains("expected failure"),
            "Python error message should be captured")) {
        return result;
    }
    if (const auto result = expect(failureResult.errorStack.contains("ValueError"),
            "Python traceback should be captured")) {
        return result;
    }

    QTextStream(stdout) << "python worker tests passed" << Qt::endl;
    return 0;
}
