#include "execution/WorkerPool.h"

#include <QRunnable>
#include <QThread>

namespace vws::execution {

WorkerPool::WorkerPool(int maxThreadCount)
{
    m_pool.setMaxThreadCount(maxThreadCount > 0 ? maxThreadCount : qMax(2, QThread::idealThreadCount()));
}

void WorkerPool::submit(std::function<void()> task)
{
    m_pool.start(QRunnable::create(std::move(task)));
}

void WorkerPool::waitForDone()
{
    m_pool.waitForDone();
}

int WorkerPool::maxThreadCount() const
{
    return m_pool.maxThreadCount();
}

} // namespace vws::execution
