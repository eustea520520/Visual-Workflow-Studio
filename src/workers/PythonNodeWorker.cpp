#include "workers/PythonNodeWorker.h"

#include "infrastructure/JsonUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QProcess>
#include <QUuid>

#include <utility>

namespace vws::workers {

namespace {

constexpr int kFallbackTimeoutMs = 300000;

QJsonObject buildWorkerRequest(const execution::NodeExecutionRequest& request)
{
    // 这是 C++ -> Python 的协议对象。
    // code/entry 来自 Function Node 配置；inputs/context 来自执行器。
    return {
        {"request_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"code", request.nodeConfig.value("code").toString()},
        {"entry", request.nodeConfig.value("entry").toString("run")},
        {"inputs", request.inputs},
        {"context", QJsonObject{
            {"run_id", request.runId},
            {"node_id", request.nodeId},
            {"workspace_path", request.workspacePath},
            {"run_path", QDir::toNativeSeparators(request.runPath)},
            {"artifact_path", QDir::toNativeSeparators(request.artifactPath)},
        }},
    };
}

QList<domain::Artifact> artifactsFromJson(
    const QJsonArray& artifactArray,
    const QString& runId,
    const QString& nodeId)
{
    QList<domain::Artifact> artifacts;
    for (const auto& artifactValue : artifactArray) {
        const auto object = artifactValue.toObject();

        domain::Artifact artifact;
        artifact.artifactId = object.value("artifact_id").toString(QUuid::createUuid().toString(QUuid::WithoutBraces));
        artifact.runId = object.value("run_id").toString(runId);
        artifact.nodeId = object.value("node_id").toString(nodeId);
        artifact.type = object.value("type").toString("json");
        artifact.path = QDir::toNativeSeparators(QDir::cleanPath(object.value("path").toString()));
        artifact.metadata = object.value("metadata").toObject();
        artifacts.append(artifact);
    }
    return artifacts;
}

} // namespace

PythonNodeWorker::PythonNodeWorker(QString pythonExecutable, QString workerScriptPath)
    : m_pythonExecutable(std::move(pythonExecutable))
    , m_workerScriptPath(std::move(workerScriptPath))
{
}

QString PythonNodeWorker::type() const
{
    return "function";
}

execution::NodeExecutionResult PythonNodeWorker::execute(const execution::NodeExecutionRequest& request)
{
    if (m_pythonExecutable.trimmed().isEmpty()) {
        return errorResult(request, "Python interpreter path is empty.");
    }

    const auto scriptPath = resolveWorkerScriptPath();
    if (scriptPath.isEmpty()) {
        return errorResult(request, "Could not find python/python_worker.py.");
    }

    if (request.nodeConfig.value("code").toString().trimmed().isEmpty()) {
        return errorResult(request, "Function node config.code is empty.");
    }

    QProcess process;
    process.setProgram(m_pythonExecutable);
    process.setArguments({scriptPath});
    process.setProcessChannelMode(QProcess::SeparateChannels);

    auto unregisterProcess = [&]() {
        QMutexLocker locker(&m_processMutex);
        auto it = m_runningProcessesByRun.find(request.runId);
        if (it == m_runningProcessesByRun.end()) {
            return;
        }
        it.value().remove(&process);
        if (it.value().isEmpty()) {
            m_runningProcessesByRun.erase(it);
        }
    };

    {
        QMutexLocker locker(&m_processMutex);
        m_runningProcessesByRun[request.runId].insert(&process);
    }

    // 启动独立 Python 进程。这里失败通常是解释器路径错误、权限问题或脚本路径错误。
    process.start();
    if (!process.waitForStarted(10000)) {
        unregisterProcess();
        return errorResult(request, QString("Could not start Python interpreter: %1").arg(process.errorString()));
    }

    const auto workerRequest = buildWorkerRequest(request);
    const QJsonDocument requestDocument(workerRequest);
    process.write(requestDocument.toJson(QJsonDocument::Compact));
    process.closeWriteChannel();

    const auto timeoutMs = request.timeoutMs > 0 ? request.timeoutMs : kFallbackTimeoutMs;
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        unregisterProcess();
        return errorResult(request, QString("Python node timed out after %1 ms.").arg(timeoutMs),
            QString::fromUtf8(process.readAllStandardError()));
    }

    const auto protocolStdout = QString::fromUtf8(process.readAllStandardOutput());
    const auto processStderr = QString::fromUtf8(process.readAllStandardError());
    unregisterProcess();

    QJsonParseError parseError;
    const auto responseDocument = QJsonDocument::fromJson(protocolStdout.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
        return errorResult(request,
            QString("Python worker returned invalid JSON: %1").arg(parseError.errorString()),
            processStderr,
            protocolStdout);
    }

    const auto response = responseDocument.object();

    execution::NodeExecutionResult result;
    result.runId = request.runId;
    result.nodeId = request.nodeId;
    result.success = response.value("success").toBool(false);
    result.outputs = response.value("outputs").toObject();
    result.artifacts = artifactsFromJson(response.value("artifacts").toArray(), request.runId, request.nodeId);
    result.stdoutText = response.value("stdout").toString();
    result.stderrText = response.value("stderr").toString();
    if (!processStderr.trimmed().isEmpty()) {
        result.stderrText += processStderr;
    }
    result.errorMessage = response.value("error").toString();
    result.errorStack = response.value("traceback").toString();

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        result.success = false;
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = QString("Python worker exited with code %1.").arg(process.exitCode());
        }
    }

    if (result.success) {
        QString artifactError;
        if (!validateArtifactsExist(result, &artifactError)) {
            return errorResult(request, artifactError, result.stderrText, result.errorStack);
        }

        QString saveError;
        if (!saveNodeOutput(request, result, &saveError)) {
            return errorResult(request, saveError, result.stderrText, result.errorStack);
        }
    }

    return result;
}

void PythonNodeWorker::cancel(const QString& executionId)
{
    QSet<QProcess*> processes;
    {
        QMutexLocker locker(&m_processMutex);
        processes = m_runningProcessesByRun.value(executionId);
    }

    for (auto* process : processes) {
        if (process != nullptr && process->state() != QProcess::NotRunning) {
            process->kill();
        }
    }
}

QString PythonNodeWorker::pythonExecutable() const
{
    return m_pythonExecutable;
}

void PythonNodeWorker::setPythonExecutable(const QString& pythonExecutable)
{
    m_pythonExecutable = pythonExecutable;
}

QString PythonNodeWorker::workerScriptPath() const
{
    return m_workerScriptPath;
}

void PythonNodeWorker::setWorkerScriptPath(const QString& workerScriptPath)
{
    m_workerScriptPath = workerScriptPath;
}

QString PythonNodeWorker::resolveWorkerScriptPath() const
{
    if (!m_workerScriptPath.isEmpty() && QFileInfo::exists(m_workerScriptPath)) {
        return QFileInfo(m_workerScriptPath).absoluteFilePath();
    }

    // python_worker.py is source-controlled under python/. Search the source
    // root and nearby executable directories used by local builds.
    const QStringList candidates = {
        QDir::current().filePath("python/python_worker.py"),
        QDir(QCoreApplication::applicationDirPath()).filePath("python/python_worker.py"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../python/python_worker.py"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../../python/python_worker.py"),
    };

    for (const auto& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }

    return {};
}

execution::NodeExecutionResult PythonNodeWorker::errorResult(
    const execution::NodeExecutionRequest& request,
    const QString& message,
    const QString& stderrText,
    const QString& errorStack) const
{
    execution::NodeExecutionResult result;
    result.runId = request.runId;
    result.nodeId = request.nodeId;
    result.success = false;
    result.errorMessage = message;
    result.stderrText = stderrText;
    result.errorStack = errorStack;
    return result;
}

bool PythonNodeWorker::saveNodeOutput(
    const execution::NodeExecutionRequest& request,
    const execution::NodeExecutionResult& result,
    QString* errorMessage) const
{
    // runPath 为空时说明调用方当前不要求落盘，例如某些极简单元测试。
    if (request.runPath.trimmed().isEmpty()) {
        return true;
    }

    const auto outputPath = QDir(request.runPath).filePath(QString("%1_output.json").arg(request.nodeId));
    const QJsonObject outputObject = {
        {"run_id", request.runId},
        {"node_id", request.nodeId},
        {"success", result.success},
        {"outputs", result.outputs},
        {"stdout", result.stdoutText},
        {"stderr", result.stderrText},
        {"error", result.errorMessage},
        {"traceback", result.errorStack},
    };

    return infrastructure::JsonUtils::writeObjectToFile(outputPath, outputObject, errorMessage);
}

bool PythonNodeWorker::validateArtifactsExist(
    const execution::NodeExecutionResult& result,
    QString* errorMessage) const
{
    for (const auto& artifact : result.artifacts) {
        if (artifact.path.trimmed().isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QString("Artifact from node %1 has an empty path.").arg(result.nodeId);
            }
            return false;
        }

        if (!QFileInfo::exists(artifact.path)) {
            if (errorMessage != nullptr) {
                *errorMessage = QString("Artifact file does not exist after Python node completed: %1").arg(artifact.path);
            }
            return false;
        }
    }

    return true;
}

} // namespace vws::workers
