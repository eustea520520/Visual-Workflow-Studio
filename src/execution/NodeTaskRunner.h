#pragma once

#include "execution/NodeExecutionRequest.h"
#include "execution/NodeExecutionResult.h"

namespace vws::workers {
class WorkerRegistry;
}

namespace vws::execution {

// Executes one node request through the registered worker for its node type.
class NodeTaskRunner final {
public:
    explicit NodeTaskRunner(workers::WorkerRegistry& workerRegistry);

    NodeExecutionResult execute(const NodeExecutionRequest& request) const;

private:
    workers::WorkerRegistry& m_workerRegistry;
};

} // namespace vws::execution
