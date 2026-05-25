#include "AppContext.h"

#include "application/NodeTemplateService.h"
#include "application/RunService.h"
#include "application/subsystem/SubsystemService.h"
#include "application/WorkflowService.h"
#include "application/WorkspaceService.h"
#include "application/generation/WorkflowAutoLayout.h"
#include "application/generation/WorkflowGenerationAssembler.h"
#include "application/generation/WorkflowGenerationNormalizer.h"
#include "application/generation/WorkflowGenerationPromptBuilder.h"
#include "application/generation/WorkflowGenerationService.h"
#include "application/generation/WorkflowGenerationTemplateCatalog.h"
#include "application/generation/WorkflowGenerationValidator.h"
#include "application/generation/WorkflowNodeImplementationValidator.h"
#include "application/generation/WorkflowSkeletonValidator.h"
#include "domain/NodeTypes.h"
#include "execution/ExecutionEngine.h"
#include "infrastructure/llm/LlmChatClient.h"
#include "presentation/controllers/NodeTemplateController.h"
#include "presentation/controllers/PythonEnvironmentController.h"
#include "presentation/controllers/RunController.h"
#include "presentation/controllers/WorkspaceBrowserController.h"
#include "presentation/controllers/CanvasNavigationController.h"
#include "presentation/controllers/CanvasSessionController.h"
#include "presentation/controllers/WorkflowGenerationController.h"
#include "presentation/controllers/WorkflowIoController.h"
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
    m_workerRegistry->registerWorkerForType(NodeTypes::Loop, m_pythonWorker);

    m_executionEngine = std::make_unique<execution::ExecutionEngine>(*m_workerRegistry);
    m_workspaceService = std::make_unique<application::WorkspaceService>();
    m_workflowService = std::make_unique<application::WorkflowService>();
    m_nodeTemplateService = std::make_unique<application::NodeTemplateService>();
    m_runService = std::make_unique<application::RunService>();
    m_subsystemService = std::make_unique<application::SubsystemService>();
    m_workflowAutoLayout = std::make_unique<application::WorkflowAutoLayout>();
    m_workflowGenerationTemplateCatalog = std::make_unique<application::WorkflowGenerationTemplateCatalog>();
    m_workflowGenerationPromptBuilder = std::make_unique<application::WorkflowGenerationPromptBuilder>();
    m_workflowGenerationValidator = std::make_unique<application::WorkflowGenerationValidator>();
    m_workflowSkeletonValidator = std::make_unique<application::WorkflowSkeletonValidator>();
    m_workflowNodeImplementationValidator = std::make_unique<application::WorkflowNodeImplementationValidator>();
    m_workflowGenerationAssembler = std::make_unique<application::WorkflowGenerationAssembler>();
    m_workflowGenerationNormalizer = std::make_unique<application::WorkflowGenerationNormalizer>(*m_workflowAutoLayout);
    m_workflowGenerationService = std::make_unique<application::WorkflowGenerationService>(
        *m_workflowService,
        *m_workflowGenerationValidator,
        *m_workflowGenerationNormalizer,
        *m_workflowGenerationTemplateCatalog,
        *m_workflowSkeletonValidator,
        *m_workflowNodeImplementationValidator,
        *m_workflowGenerationAssembler);
    m_llmChatClient = std::make_unique<infrastructure::LlmChatClient>();

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
    m_canvasNavigationController = std::make_unique<presentation::CanvasNavigationController>(
        *m_subsystemService);
    m_canvasSessionController = std::make_unique<presentation::CanvasSessionController>(
        *m_appStore,
        *m_workflowController,
        *m_canvasNavigationController);
    m_workflowIoController = std::make_unique<presentation::WorkflowIoController>();
    m_workflowGenerationController = std::make_unique<presentation::WorkflowGenerationController>(
        *m_workflowGenerationPromptBuilder,
        *m_workflowGenerationService,
        *m_llmChatClient,
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

presentation::CanvasNavigationController& AppContext::canvasNavigationController()
{
    return *m_canvasNavigationController;
}

presentation::CanvasSessionController& AppContext::canvasSessionController()
{
    return *m_canvasSessionController;
}

presentation::WorkflowIoController& AppContext::workflowIoController()
{
    return *m_workflowIoController;
}

presentation::WorkflowGenerationController& AppContext::workflowGenerationController()
{
    return *m_workflowGenerationController;
}

application::SubsystemService& AppContext::subsystemService()
{
    return *m_subsystemService;
}

void AppContext::setPythonExecutable(const QString& pythonExecutable)
{
    if (m_pythonWorker != nullptr) {
        m_pythonWorker->setPythonExecutable(pythonExecutable.trimmed());
    }
}

} // namespace vws
