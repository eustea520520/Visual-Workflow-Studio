#pragma once

#include <QString>

namespace vws::execution {

struct ThreadTraceInfo {
    QString threadId;
    QString threadName;
};

ThreadTraceInfo currentThreadTraceInfo();

} // namespace vws::execution
