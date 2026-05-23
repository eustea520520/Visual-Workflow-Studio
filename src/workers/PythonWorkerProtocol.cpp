#include "workers/PythonWorkerProtocol.h"

#include "domain/NodeConfigView.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUuid>

namespace vws::workers {

namespace {

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

QJsonObject PythonWorkerProtocol::buildRequest(const execution::NodeExecutionRequest& request)
{
    const domain::NodeConfigView config(request.nodeConfig);
    auto context = request.context;
    context.insert("run_id", request.runId);
    context.insert("node_id", request.nodeId);
    context.insert("workspace_path", request.workspacePath);
    context.insert("run_path", QDir::toNativeSeparators(request.runPath));
    context.insert("artifact_path", QDir::toNativeSeparators(request.artifactPath));
    return {
        {"request_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"code", config.code()},
        {"entry", config.entry()},
        {"inputs", request.inputs},
        {"context", context},
    };
}

PythonWorkerParseResult PythonWorkerProtocol::parseResponse(
    const execution::NodeExecutionRequest& request,
    const QString& protocolStdout,
    const QString& processStderr)
{
    PythonWorkerParseResult parseResult;

    QJsonParseError parseError;
    const auto responseDocument = QJsonDocument::fromJson(protocolStdout.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
        parseResult.errorMessage = QString("Python worker returned invalid JSON: %1").arg(parseError.errorString());
        parseResult.stderrText = processStderr;
        parseResult.errorStack = protocolStdout;
        return parseResult;
    }

    const auto response = responseDocument.object();

    auto& result = parseResult.nodeResult;
    result.runId = request.runId;
    result.nodeId = request.nodeId;
    result.success = response.value("success").toBool(false);
    result.outputs = response.value("outputs").toObject();
    result.metadata = response.value("metadata").toObject();
    result.artifacts = artifactsFromJson(response.value("artifacts").toArray(), request.runId, request.nodeId);
    result.stdoutText = response.value("stdout").toString();
    result.stderrText = response.value("stderr").toString();
    if (!processStderr.trimmed().isEmpty()) {
        result.stderrText += processStderr;
    }
    result.errorMessage = response.value("error").toString();
    result.errorStack = response.value("traceback").toString();

    parseResult.valid = true;
    return parseResult;
}

} // namespace vws::workers
