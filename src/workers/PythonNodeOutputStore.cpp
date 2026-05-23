#include "workers/PythonNodeOutputStore.h"

#include "infrastructure/JsonUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>

namespace vws::workers {

bool PythonNodeOutputStore::saveNodeOutput(
    const execution::NodeExecutionRequest& request,
    const execution::NodeExecutionResult& result,
    QString* errorMessage) const
{
    if (request.runPath.trimmed().isEmpty()) {
        return true;
    }

    const auto outputPath = QDir(request.runPath).filePath(QString("%1_output.json").arg(request.nodeId));
    const QJsonObject outputObject = {
        {"run_id", request.runId},
        {"node_id", request.nodeId},
        {"success", result.success},
        {"outputs", result.outputs},
        {"metadata", result.metadata},
        {"stdout", result.stdoutText},
        {"stderr", result.stderrText},
        {"error", result.errorMessage},
        {"traceback", result.errorStack},
    };

    return infrastructure::JsonUtils::writeObjectToFile(outputPath, outputObject, errorMessage);
}

bool PythonNodeOutputStore::validateArtifactsExist(
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
