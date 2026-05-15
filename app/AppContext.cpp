#include "AppContext.h"

#include "application/NodeTemplateService.h"
#include "application/RunService.h"
#include "application/WorkflowService.h"
#include "application/WorkspaceService.h"
#include "domain/NodeTypes.h"
#include "execution/ExecutionEngine.h"
#include "presentation/controllers/NodeTemplateController.h"
#include "presentation/controllers/PythonEnvironmentController.h"
#include "presentation/controllers/RunController.h"
#include "presentation/controllers/WorkspaceBrowserController.h"
#include "presentation/controllers/WorkflowController.h"
#include "presentation/controllers/WorkspaceController.h"
#include "presentation/state/AppStore.h"
#include "workers/PythonNodeWorker.h"
#include "workers/WorkerRegistry.h"

#include <memory>

namespace vws {

namespace NodeTypes = domain::NodeTypes;

AppContext::AppContext() = default;

AppContext::~AppContext() = default;

void AppContext::initialize()
{
    m_appStore = std::make_unique<presentation::AppStore>();
    m_workerRegistry = std::make_unique<workers::WorkerRegistry>();
    m_pythonWorker = std::make_shared<workers::PythonNodeWorker>(QString());
    m_workerRegistry->registerWorker(m_pythonWorker);
    m_workerRegistry->registerWorkerForType(NodeTypes::Starter, m_pythonWorker);
    m_workerRegistry->registerWorkerForType(NodeTypes::Agent, m_pythonWorker);

    m_executionEngine = std::make_unique<execution::ExecutionEngine>(*m_workerRegistry);
    m_workspaceService = std::make_unique<application::WorkspaceService>();
    m_workflowService = std::make_unique<application::WorkflowService>();
    m_nodeTemplateService = std::make_unique<application::NodeTemplateService>();
    m_runService = std::make_unique<application::RunService>();

    m_workspaceController = std::make_unique<presentation::WorkspaceController>(
        *m_workspaceService,
        *m_appStore);
    m_pythonEnvironmentController = std::make_unique<presentation::PythonEnvironmentController>(
        *m_workspaceService,
        *m_appStore,
        [this](const QString& pythonExecutable) {
            setPythonExecutable(pythonExecutable);
        });
    m_workflowController = std::make_unique<presentation::WorkflowController>(
        *m_workflowService,
        *m_appStore);
    m_nodeTemplateController = std::make_unique<presentation::NodeTemplateController>(
        *m_nodeTemplateService,
        *m_appStore);
    m_runController = std::make_unique<presentation::RunController>(
        *m_executionEngine,
        *m_runService,
        *m_appStore);
    m_workspaceBrowserController = std::make_unique<presentation::WorkspaceBrowserController>(
        *m_workflowService,
        *m_nodeTemplateService,
        *m_runService,
        *m_appStore);
}

presentation::AppStore& AppContext::appStore()
{
    return *m_appStore;
}

presentation::WorkspaceController& AppContext::workspaceController()
{
    return *m_workspaceController;
}

presentation::PythonEnvironmentController& AppContext::pythonEnvironmentController()
{
    return *m_pythonEnvironmentController;
}

presentation::WorkflowController& AppContext::workflowController()
{
    return *m_workflowController;
}

presentation::NodeTemplateController& AppContext::nodeTemplateController()
{
    return *m_nodeTemplateController;
}

presentation::RunController& AppContext::runController()
{
    return *m_runController;
}

presentation::WorkspaceBrowserController& AppContext::workspaceBrowserController()
{
    return *m_workspaceBrowserController;
}

void AppContext::setPythonExecutable(const QString& pythonExecutable)
{
    if (m_pythonWorker != nullptr) {
        m_pythonWorker->setPythonExecutable(pythonExecutable.trimmed());
    }
}

} // namespace vws
