#pragma once

#include "execution/DataPacket.h"
#include "execution/GraphIndexes.h"
#include "execution/NodeExecutionRequest.h"

#include <QHash>
#include <QString>

namespace vws::execution {

class InputMerger;

class NodeExecutionRequestBuilder {
public:
    NodeExecutionRequestBuilder(
        QString runId,
        QString workspacePath,
        QString runPath,
        QString artifactPath);

    NodeExecutionRequest build(
        const QString& nodeId,
        const GraphIndexes& indexes,
        const QHash<QString, DataPacket>& completedEdgeData,
        const InputMerger& inputMerger,
        const QHash<QString, QJsonObject>& initialInputsByNodeId = {}) const;

private:
    QString m_runId;
    QString m_workspacePath;
    QString m_runPath;
    QString m_artifactPath;
};

} // namespace vws::execution
