#include "execution/ExecutionCancellationState.h"

#include <QMutexLocker>

namespace vws::execution {

void ExecutionCancellationState::beginRun(const QString& runId)
{
    QMutexLocker locker(&m_mutex);
    m_cancelRequested.store(false);
    m_currentRunId = runId;
}

void ExecutionCancellationState::finishRun(const QString& runId)
{
    QMutexLocker locker(&m_mutex);
    if (m_currentRunId == runId) {
        m_currentRunId.clear();
    }
}

QString ExecutionCancellationState::requestCancel()
{
    QMutexLocker locker(&m_mutex);
    m_cancelRequested.store(true);
    return m_currentRunId;
}

bool ExecutionCancellationState::isCancelRequested() const
{
    return m_cancelRequested.load();
}

} // namespace vws::execution
