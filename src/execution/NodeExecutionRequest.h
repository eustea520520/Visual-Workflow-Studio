#pragma once

#include <QJsonObject>
#include <QString>

namespace vws::execution {

// ExecutionEngine 传给 Worker 的标准请求。
// 无论将来是 Python、Agent、Docker 还是远程 Worker，都尽量走这个结构。
struct NodeExecutionRequest {
    QString runId;
    QString nodeId;
    QString nodeType;
    QJsonObject nodeConfig;
    QJsonObject inputs;
    QString workspacePath;
    QString runPath;
    QString artifactPath;
    QJsonObject context;
    int timeoutMs = 300000;
};

} // namespace vws::execution
