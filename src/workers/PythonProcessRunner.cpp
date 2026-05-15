#include "workers/PythonProcessRunner.h"

#include <QMutexLocker>
#include <QProcess>

namespace vws::workers {

PythonProcessResult PythonProcessRunner::run(
    const QString& runId,
    const QString& program,
    const QStringList& arguments,
    const QByteArray& stdinBytes,
    int timeoutMs)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);

    registerProcess(runId, &process);

    process.start();
    if (!process.waitForStarted(10000)) {
        const auto error = process.errorString();
        unregisterProcess(runId, &process);
        PythonProcessResult result;
        result.errorMessage = QString("Could not start Python interpreter: %1").arg(error);
        return result;
    }

    process.write(stdinBytes);
    process.closeWriteChannel();

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(3000);

        PythonProcessResult result;
        result.started = true;
        result.timedOut = true;
        result.stderrText = QString::fromUtf8(process.readAllStandardError());
        result.exitStatus = process.exitStatus();
        result.exitCode = process.exitCode();
        unregisterProcess(runId, &process);
        return result;
    }

    PythonProcessResult result;
    result.started = true;
    result.exitStatus = process.exitStatus();
    result.exitCode = process.exitCode();
    result.stdoutText = QString::fromUtf8(process.readAllStandardOutput());
    result.stderrText = QString::fromUtf8(process.readAllStandardError());
    unregisterProcess(runId, &process);
    return result;
}

void PythonProcessRunner::cancelRun(const QString& runId)
{
    QSet<QProcess*> processes;
    {
        QMutexLocker locker(&m_processMutex);
        processes = m_runningProcessesByRun.value(runId);
    }

    for (auto* process : processes) {
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
    }
}

} // namespace vws::workers
