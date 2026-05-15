#include "workers/PythonNodeWorker.h"

#include "domain/NodeConfigView.h"
#include "domain/NodeTypes.h"
#include "workers/PythonWorkerProtocol.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>

#include <utility>

namespace vws::workers {

namespace NodeTypes = domain::NodeTypes;

namespace {

constexpr int kFallbackTimeoutMs = 300000;

} // namespace

PythonNodeWorker::PythonNodeWorker(QString pythonExecutable, QString workerScriptPath)
    : m_pythonExecutable(std::move(pythonExecutable))
    , m_workerScriptPath(std::move(workerScriptPath))
{
}

QString PythonNodeWorker::type() const
{
    return NodeTypes::Function;
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

    if (domain::NodeConfigView(request.nodeConfig).code().trimmed().isEmpty()) {
        return errorResult(request, "Function node config.code is empty.");
    }

    const QJsonDocument requestDocument(PythonWorkerProtocol::buildRequest(request));
    const auto timeoutMs = request.timeoutMs > 0 ? request.timeoutMs : kFallbackTimeoutMs;
    const auto processResult = m_processRunner.run(
        request.runId,
        m_pythonExecutable,
        {scriptPath},
        requestDocument.toJson(QJsonDocument::Compact),
        timeoutMs);

    if (!processResult.started) {
        return errorResult(request, processResult.errorMessage);
    }

    if (processResult.timedOut) {
        return errorResult(request, QString("Python node timed out after %1 ms.").arg(timeoutMs),
            processResult.stderrText);
    }

    const auto parseResult = PythonWorkerProtocol::parseResponse(
        request,
        processResult.stdoutText,
        processResult.stderrText);
    if (!parseResult.valid) {
        return errorResult(request, parseResult.errorMessage, parseResult.stderrText, parseResult.errorStack);
    }

    auto result = parseResult.nodeResult;
    if (processResult.exitStatus != QProcess::NormalExit || processResult.exitCode != 0) {
        result.success = false;
        if (result.errorMessage.isEmpty()) {
            result.errorMessage = QString("Python worker exited with code %1.").arg(processResult.exitCode);
        }
    }

    if (result.success) {
        QString artifactError;
        if (!m_outputStore.validateArtifactsExist(result, &artifactError)) {
            return errorResult(request, artifactError, result.stderrText, result.errorStack);
        }

        QString saveError;
        if (!m_outputStore.saveNodeOutput(request, result, &saveError)) {
            return errorResult(request, saveError, result.stderrText, result.errorStack);
        }
    }

    return result;
}

void PythonNodeWorker::cancel(const QString& executionId)
{
    m_processRunner.cancelRun(executionId);
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

} // namespace vws::workers
