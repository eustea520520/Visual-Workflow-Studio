#pragma once

#include "domain/Artifact.h"

#include <QJsonObject>
#include <QList>
#include <QString>

namespace vws::execution {

// Worker 返回给 ExecutionEngine 的标准结果。
// success 控制节点状态；outputs 给下游节点使用；artifacts 记录落盘产物。
struct NodeExecutionResult {
    QString runId;
    QString nodeId;
    bool success = false;
    QJsonObject outputs;
    QList<domain::Artifact> artifacts;
    QString stdoutText;
    QString stderrText;
    QString errorMessage;
    QString errorStack;
};

} // namespace vws::execution
