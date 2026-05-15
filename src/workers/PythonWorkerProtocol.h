#pragma once

#include "execution/NodeExecutionRequest.h"
#include "execution/NodeExecutionResult.h"

#include <QJsonObject>
#include <QString>

namespace vws::workers {

struct PythonWorkerParseResult {
    bool valid = false;
    execution::NodeExecutionResult nodeResult;
    QString errorMessage;
    QString stderrText;
    QString errorStack;
};

// PythonWorkerProtocol 只负责 C++ 与 python_worker.py 的 JSON 协议转换。
// 它不启动进程、不处理超时、不保存文件，因此可以独立测试并保持无 UI 依赖。
class PythonWorkerProtocol final {
public:
    static QJsonObject buildRequest(const execution::NodeExecutionRequest& request);
    static PythonWorkerParseResult parseResponse(
        const execution::NodeExecutionRequest& request,
        const QString& protocolStdout,
        const QString& processStderr);
};

} // namespace vws::workers
