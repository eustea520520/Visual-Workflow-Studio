#pragma once

#include <QHash>
#include <QMutex>
#include <QProcess>
#include <QSet>
#include <QString>

namespace vws::workers {

struct PythonProcessResult {
    bool started = false;
    bool cancelled = false;
    bool timedOut = false;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
    int exitCode = -1;
    QString stdoutText;
    QString stderrText;
    QString errorMessage;
};

// Owns QProcess startup, timeout handling, and run-level cancellation tracking.
class PythonProcessRunner final {
public:
    PythonProcessResult run(
        const QString& runId,
        const QString& program,
        const QStringList& arguments,
        const QByteArray& stdinBytes,
        int timeoutMs);
    void cancelRun(const QString& runId);

private:
    void registerProcess(const QString& runId, QProcess* process);
    void unregisterProcess(const QString& runId, QProcess* process);
    bool isRunCancelled(const QString& runId) const;

    mutable QMutex m_processMutex;
    // Values point to heap-owned QProcess instances whose lifetime is controlled by run().
    // cancelRun() holds m_processMutex while killing them, so unregister/delete cannot race it.
    QHash<QString, QSet<QProcess*>> m_runningProcessesByRun;
    QSet<QString> m_cancelledRuns;
};

} // namespace vws::workers
