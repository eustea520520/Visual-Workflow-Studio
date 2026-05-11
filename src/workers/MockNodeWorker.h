#pragma once

#include "workers/INodeWorker.h"

namespace vws::workers {

// MockNodeWorker 用于在真实 Python/Agent Worker 完成前测试执行主链路。
// 它不会执行外部代码，只把输入包装成输出；也可通过 config.mock_fail 模拟失败。
class MockNodeWorker final : public INodeWorker {
public:
    explicit MockNodeWorker(QString typeName);

    QString type() const override;
    execution::NodeExecutionResult execute(const execution::NodeExecutionRequest& request) override;
    void cancel(const QString& executionId) override;

private:
    QString m_typeName;
};

} // namespace vws::workers
