#pragma once

#include "workers/INodeWorker.h"
#include "workers/PythonNodeOutputStore.h"
#include "workers/PythonProcessRunner.h"

#include <QString>

namespace vws::workers {

// PythonNodeWorker 是 Function Node 的真实执行器。
//
// 它不在 Qt 主进程内嵌 Python，而是通过 QProcess 启动独立解释器：
//   Qt/C++ -> stdin JSON -> python/python_worker.py -> stdout JSON -> Qt/C++
//
// 这样用户代码崩溃、抛异常、写 stdout，都不会直接破坏 Qt 主进程。
class PythonNodeWorker final : public INodeWorker {
public:
    explicit PythonNodeWorker(
        QString pythonExecutable = "python",
        QString workerScriptPath = QString());

    QString type() const override;
    execution::NodeExecutionResult execute(const execution::NodeExecutionRequest& request) override;
    void cancel(const QString& executionId) override;

    QString pythonExecutable() const;
    void setPythonExecutable(const QString& pythonExecutable);

    QString workerScriptPath() const;
    void setWorkerScriptPath(const QString& workerScriptPath);

private:
    QString resolveWorkerScriptPath() const;
    execution::NodeExecutionResult errorResult(
        const execution::NodeExecutionRequest& request,
        const QString& message,
        const QString& stderrText = QString(),
        const QString& errorStack = QString()) const;
    QString m_pythonExecutable;
    QString m_workerScriptPath;
    PythonProcessRunner m_processRunner;
    PythonNodeOutputStore m_outputStore;
};

} // namespace vws::workers
