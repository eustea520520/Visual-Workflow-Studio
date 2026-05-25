#include "execution/ExecutionEventForwarder.h"

#include "execution/ExecutionEventBus.h"

#include <QObject>

namespace vws::execution {

void ExecutionEventForwarder::connectNestedRun(
    ExecutionEventBus& nestedBus,
    ExecutionEventBus& parentBus,
    const QString& outerRunId)
{
    QObject::connect(&nestedBus,
        &ExecutionEventBus::nodeStatusChanged,
        &parentBus,
        [&parentBus, outerRunId](const QString&, const QString& nodeId, const QString& status) {
            parentBus.publishNodeStatusText(outerRunId, nodeId, status);
        },
        Qt::DirectConnection);

    QObject::connect(&nestedBus,
        &ExecutionEventBus::nodeError,
        &parentBus,
        [&parentBus, outerRunId](const QString&, const QString& nodeId, const QString& message) {
            parentBus.publishNodeError(outerRunId, nodeId, message);
        },
        Qt::DirectConnection);

    QObject::connect(&nestedBus,
        &ExecutionEventBus::threadTrace,
        &parentBus,
        [&parentBus, outerRunId](
            const QString&,
            const QString& nodeId,
            const QString& phase,
            const QString& threadId,
            const QString& threadName) {
            parentBus.publishThreadTrace(outerRunId, nodeId, phase, threadId, threadName);
        },
        Qt::DirectConnection);
}

} // namespace vws::execution
