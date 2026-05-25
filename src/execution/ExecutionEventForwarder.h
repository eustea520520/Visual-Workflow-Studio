#pragma once

#include <QString>

namespace vws::execution {

class ExecutionEventBus;

class ExecutionEventForwarder final {
public:
    static void connectNestedRun(
        ExecutionEventBus& nestedBus,
        ExecutionEventBus& parentBus,
        const QString& outerRunId);
};

} // namespace vws::execution
