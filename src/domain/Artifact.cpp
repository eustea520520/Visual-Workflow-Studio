#include "domain/Artifact.h"

namespace vws::domain {

QJsonObject Artifact::toJson() const
{
    return {
        {"artifact_id", artifactId},
        {"run_id", runId},
        {"node_id", nodeId},
        {"type", type},
        {"path", path},
        {"metadata", metadata},
    };
}

Artifact Artifact::fromJson(const QJsonObject& object)
{
    Artifact artifact;
    artifact.artifactId = object.value("artifact_id").toString();
    artifact.runId = object.value("run_id").toString();
    artifact.nodeId = object.value("node_id").toString();
    artifact.type = object.value("type").toString();
    artifact.path = object.value("path").toString();
    artifact.metadata = object.value("metadata").toObject();
    return artifact;
}

} // namespace vws::domain
