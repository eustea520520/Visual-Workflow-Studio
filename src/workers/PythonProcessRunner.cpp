#include "workers/PythonProcessRunner.h"

#include <QMutexLocker>
#include <QProcess>

#include <memory>

namespace vws::workers {

PythonProcessResult PythonProcessRunner::run(
    const QString& runId,
    const QString& program,
    const QStringList& arguments,
    const QByteArray& stdinBytes,
    int timeoutMs)
{
    auto process = std::make_unique<QProcess>();
    process->setProgram(program);
    process->setArguments(arguments);
    process->setProcessChannelMode(QProcess::SeparateChannels);

    registerProcess(runId, process.get());

    process->start();
    if (!process->waitForStarted(10000)) {
        const auto error = process->errorString();
        unregisterProcess(runId, process.get());
        PythonProcessResult result;
        result.errorMessage = QString("Could not start Python interpreter: %1").arg(error);
        return result;
    }

    if (isRunCancelled(runId)) {
        process->kill();
        process->waitForFinished(3000);
        PythonProcessResult result;
        result.started = true;
        result.cancelled = true;
        result.stderrText = QString::fromUtf8(process->readAllStandardError());
        result.exitStatus = process->exitStatus();
        result.exitCode = process->exitCode();
        unregisterProcess(runId, process.get());
        return result;
    }

    process->write(stdinBytes);
    process->closeWriteChannel();

    if (!process->waitForFinished(timeoutMs)) {
        process->kill();
        process->waitForFinished(3000);

        PythonProcessResult result;
        result.started = true;
        result.timedOut = true;
        result.stderrText = QString::fromUtf8(process->readAllStandardError());
        result.exitStatus = process->exitStatus();
        result.exitCode = process->exitCode();
        unregisterProcess(runId, process.get());
        return result;
    }

    PythonProcessResult result;
    result.started = true;
    result.cancelled = isRunCancelled(runId);
    result.exitStatus = process->exitStatus();
    result.exitCode = process->exitCode();
    result.stdoutText = QString::fromUtf8(process->readAllStandardOutput());
    result.stderrText = QString::fromUtf8(process->readAllStandardError());
    unregisterProcess(runId, process.get());
    return result;
}

void PythonProcessRunner::cancelRun(const QString& runId)
{
    QMutexLocker locker(&m_processMutex);
    m_cancelledRuns.insert(runId);
    for (auto* process : m_runningProcessesByRun.value(runId)) {
        if (process != nullptr && process->state() != QProcess::NotRunning) {
            process->kill();
        }
    }
}

void PythonProcessRunner::registerProcess(const QString& runId, QProcess* process)
{
    QMutexLocker locker(&m_processMutex);
    m_runningProcessesByRun[runId].insert(process);
}

void PythonProcessRunner::unregisterProcess(const QString& runId, QProcess* process)
{
    QMutexLocker locker(&m_processMutex);
    auto it = m_runningProcessesByRun.find(runId);
    if (it == m_runningProcessesByRun.end()) {
        return;
    }
    it.value().remove(process);
    if (it.value().isEmpty()) {
        m_runningProcessesByRun.erase(it);
        m_cancelledRuns.remove(runId);
    }
}

bool PythonProcessRunner::isRunCancelled(const QString& runId) const
{
    QMutexLocker locker(&m_processMutex);
    return m_cancelledRuns.contains(runId);
}

} // namespace vws::workers
