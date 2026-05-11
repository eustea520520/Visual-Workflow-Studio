#include "workers/MockNodeWorker.h"

#include <QJsonObject>
#include <QThread>
#include <QtGlobal>
#include <utility>

namespace vws::workers {

MockNodeWorker::MockNodeWorker(QString typeName)
    : m_typeName(std::move(typeName))
{
}

QString MockNodeWorker::type() const
{
    return m_typeName;
}

execution::NodeExecutionResult MockNodeWorker::execute(const execution::NodeExecutionRequest& request)
{
    // Mock Worker 的目标不是做真实业务，而是验证：
    // 1. ExecutionEngine 能构造请求；
    // 2. WorkerRegistry 能找到 Worker；
    // 3. 结果能传给下游节点；
    // 4. 失败能沿执行链路返回。
    execution::NodeExecutionResult result;
    result.runId = request.runId;
    result.nodeId = request.nodeId;

    const auto delayMs = request.nodeConfig.value("mock_delay_ms").toInt(0);
    if (delayMs > 0) {
        QThread::msleep(static_cast<unsigned long>(delayMs));
    }

    if (request.nodeConfig.value("mock_fail").toBool(false)) {
        result.success = false;
        result.errorMessage = QString("Mock node failed: %1").arg(request.nodeId);
        result.stderrText = result.errorMessage;
        return result;
    }

    result.success = true;
    result.outputs = {
        {"node_id", request.nodeId},
        {"node_type", request.nodeType},
        {"mock", true},
        {"inputs", request.inputs},
    };
    if (request.nodeConfig.contains("mock_output")) {
        result.outputs.insert("output", request.nodeConfig.value("mock_output"));
    }
    result.stdoutText = QString("Mock node succeeded: %1").arg(request.nodeId);
    return result;
}

void MockNodeWorker::cancel(const QString& executionId)
{
    Q_UNUSED(executionId);
}

} // namespace vws::workers
