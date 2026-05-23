#pragma once

#include "execution/NodeExecutionResult.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace vws::execution {

struct NodeDebugOutput {
    QString nodeId;
    QString text;
};

// 一次 workflow 运行结束后的轻量结果快照。
// debugOutputs 额外保存按发生顺序排列的 print 输出，避免 QHash 节点结果打乱展示顺序。
struct WorkflowExecutionResult {
    QString runId;
    bool success = false;
    QString status;
    QStringList errors;
    QHash<QString, QString> nodeStatuses;
    QHash<QString, NodeExecutionResult> nodeResults;
    QList<NodeDebugOutput> debugOutputs;
};

} // namespace vws::execution
