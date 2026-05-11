#pragma once

#include <QThreadPool>
#include <functional>

namespace vws::execution {

// WorkerPool 是执行层的有界线程池包装。
// 节点并发是“进入线程池的任务并发”，不是每条分支无限创建物理线程。
class WorkerPool {
public:
    explicit WorkerPool(int maxThreadCount = 0);

    void submit(std::function<void()> task);
    void waitForDone();
    int maxThreadCount() const;

private:
    QThreadPool m_pool;
};

} // namespace vws::execution
