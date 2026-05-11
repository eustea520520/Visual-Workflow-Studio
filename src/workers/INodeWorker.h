#pragma once

#include "execution/NodeExecutionRequest.h"
#include "execution/NodeExecutionResult.h"

#include <QString>

namespace vws::workers {

// 所有节点执行器的统一接口。
// ExecutionEngine 只认识 INodeWorker，不关心底层是 Python、Agent、Docker 还是远程服务。
class INodeWorker {
public:
    virtual ~INodeWorker() = default;

    virtual QString type() const = 0;
    virtual execution::NodeExecutionResult execute(const execution::NodeExecutionRequest& request) = 0;
    virtual void cancel(const QString& executionId) = 0;
};

} // namespace vws::workers
