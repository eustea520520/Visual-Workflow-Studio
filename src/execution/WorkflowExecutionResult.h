#pragma once

#include "execution/NodeExecutionResult.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace vws::execution {

// 一次工作流运行完成后的结果快照。
// 这个结构只保存可跨层传递的数据，不持有线程、Worker 或 Qt 对象生命周期。
struct WorkflowExecutionResult {
    QString runId;
    bool success = false;
    QString status;
    QStringList errors;
    QHash<QString, QString> nodeStatuses;
    QHash<QString, NodeExecutionResult> nodeResults;
};

} // namespace vws::execution
