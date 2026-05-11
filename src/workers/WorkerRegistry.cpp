#include "workers/WorkerRegistry.h"

#include <utility>

namespace vws::workers {

WorkerRegistry::WorkerRegistry() = default;

void WorkerRegistry::registerWorker(std::shared_ptr<INodeWorker> worker)
{
    if (worker == nullptr || worker->type().isEmpty()) {
        return;
    }

    m_workers.insert(worker->type(), std::move(worker));
}

void WorkerRegistry::registerWorkerForType(const QString& typeName, std::shared_ptr<INodeWorker> worker)
{
    if (worker == nullptr || typeName.trimmed().isEmpty()) {
        return;
    }

    // 同一个 Worker 可以服务多个节点类型。
    // 例如 function、starter、agent 当前都使用 Python 代码协议执行，
    // 但它们在 UI 上仍然保留不同节点类型和不同默认模板。
    m_workers.insert(typeName.trimmed(), std::move(worker));
}

bool WorkerRegistry::hasWorkerType(const QString& typeName) const
{
    return m_workers.contains(typeName);
}

std::shared_ptr<INodeWorker> WorkerRegistry::workerForType(const QString& typeName) const
{
    return m_workers.value(typeName);
}

QStringList WorkerRegistry::registeredTypes() const
{
    auto types = m_workers.keys();
    types.sort();
    return types;
}

} // namespace vws::workers
