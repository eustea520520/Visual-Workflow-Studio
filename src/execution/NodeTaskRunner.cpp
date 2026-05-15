#include "execution/NodeTaskRunner.h"

#include "workers/WorkerRegistry.h"

namespace vws::execution {

NodeTaskRunner::NodeTaskRunner(workers::WorkerRegistry& workerRegistry)
    : m_workerRegistry(workerRegistry)
{
}

NodeExecutionResult NodeTaskRunner::execute(const NodeExecutionRequest& request) const
{
    const auto worker = m_workerRegistry.workerForType(request.nodeType);
    if (worker != nullptr) {
        return worker->execute(request);
    }

    NodeExecutionResult result;
    result.runId = request.runId;
    result.nodeId = request.nodeId;
    result.success = false;
    result.errorMessage = QString("No worker registered for node type: %1").arg(request.nodeType);
    return result;
}

} // namespace vws::execution
