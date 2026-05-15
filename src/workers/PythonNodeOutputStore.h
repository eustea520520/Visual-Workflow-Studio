#pragma once

#include "execution/NodeExecutionRequest.h"
#include "execution/NodeExecutionResult.h"

#include <QString>

namespace vws::workers {

class PythonNodeOutputStore final {
public:
    bool saveNodeOutput(
        const execution::NodeExecutionRequest& request,
        const execution::NodeExecutionResult& result,
        QString* errorMessage) const;

    bool validateArtifactsExist(
        const execution::NodeExecutionResult& result,
        QString* errorMessage) const;
};

} // namespace vws::workers
