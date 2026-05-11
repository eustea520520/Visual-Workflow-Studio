#pragma once

#include "workers/INodeWorker.h"

#include <QHash>
#include <QStringList>
#include <memory>

namespace vws::workers {

// WorkerRegistry 根据节点 type 找到对应 Worker。
// 这让新增节点类型时只需要注册新 Worker，不需要改 ExecutionEngine 主流程。
class WorkerRegistry {
public:
    WorkerRegistry();

    void registerWorker(std::shared_ptr<INodeWorker> worker);
    void registerWorkerForType(const QString& typeName, std::shared_ptr<INodeWorker> worker);
    bool hasWorkerType(const QString& typeName) const;
    std::shared_ptr<INodeWorker> workerForType(const QString& typeName) const;
    QStringList registeredTypes() const;

private:
    QHash<QString, std::shared_ptr<INodeWorker>> m_workers;
};

} // namespace vws::workers
