#include "execution/ThreadTrace.h"

#include <QThread>

namespace vws::execution {

ThreadTraceInfo currentThreadTraceInfo()
{
    const auto name = QThread::currentThread() != nullptr
        ? QThread::currentThread()->objectName()
        : QString();

    return {
        QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16),
        name.trimmed().isEmpty() ? QStringLiteral("unnamed") : name,
    };
}

} // namespace vws::execution
