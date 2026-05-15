#pragma once

#include <QMutex>
#include <QString>
#include <atomic>

namespace vws::execution {

// Tracks cancellation for the single active workflow run owned by ExecutionEngine.
// Worker cancellation is still performed by the engine because it owns the registry.
class ExecutionCancellationState final {
public:
    void beginRun(const QString& runId);
    void finishRun(const QString& runId);
    QString requestCancel();
    bool isCancelRequested() const;

private:
    mutable QMutex m_mutex;
    QString m_currentRunId;
    std::atomic_bool m_cancelRequested = false;
};

} // namespace vws::execution
