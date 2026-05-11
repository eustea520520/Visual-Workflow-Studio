#include "AppContext.h"

#include "application/NodeTemplateService.h"
#include "application/RunService.h"
#include "application/WorkflowService.h"
#include "application/WorkspaceService.h"
#include "execution/ExecutionEngine.h"
#include "workers/PythonNodeWorker.h"
#include "workers/WorkerRegistry.h"

#include <memory>

namespace vws {

AppContext::AppContext() = default;

AppContext::~AppContext() = default;

void AppContext::initialize()
{
    m_workerRegistry = std::make_unique<workers::WorkerRegistry>();
    m_pythonWorker = std::make_shared<workers::PythonNodeWorker>(QString());
    m_workerRegistry->registerWorker(m_pythonWorker);
    m_workerRegistry->registerWorkerForType("starter", m_pythonWorker);
    m_workerRegistry->registerWorkerForType("agent", m_pythonWorker);

    m_executionEngine = std::make_unique<execution::ExecutionEngine>(*m_workerRegistry);
    m_workspaceService = std::make_unique<application::WorkspaceService>();
    m_workflowService = std::make_unique<application::WorkflowService>();
    m_nodeTemplateService = std::make_unique<application::NodeTemplateService>();
    m_runService = std::make_unique<application::RunService>(*m_executionEngine);
}

application::WorkspaceService& AppContext::workspaceService()
{
    return *m_workspaceService;
}

application::WorkflowService& AppContext::workflowService()
{
    return *m_workflowService;
}

application::NodeTemplateService& AppContext::nodeTemplateService()
{
    return *m_nodeTemplateService;
}

application::RunService& AppContext::runService()
{
    return *m_runService;
}

execution::ExecutionEngine& AppContext::executionEngine()
{
    return *m_executionEngine;
}

workers::WorkerRegistry& AppContext::workerRegistry()
{
    return *m_workerRegistry;
}

void AppContext::setPythonExecutable(const QString& pythonExecutable)
{
    if (m_pythonWorker != nullptr) {
        m_pythonWorker->setPythonExecutable(pythonExecutable.trimmed());
    }
}

} // namespace vws
