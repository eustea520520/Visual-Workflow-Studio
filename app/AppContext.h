#pragma once

#include <memory>
#include <QString>

namespace vws::application {
class NodeTemplateService;
class RunService;
class WorkflowService;
class WorkspaceService;
}

namespace vws::execution {
class ExecutionEngine;
}

namespace vws::workers {
class PythonNodeWorker;
class WorkerRegistry;
}

namespace vws::presentation {
class AppStore;
class NodeTemplateController;
class PythonEnvironmentController;
class RunController;
class WorkspaceBrowserController;
class WorkflowController;
class WorkspaceController;
}

namespace vws {

// AppContext 是应用的“组合根”（composition root）。
//
// 作用：
// 1. 统一创建数据库、应用服务、执行器、Worker 注册表等长期对象。
// 2. 明确对象之间的依赖关系，避免在 MainWindow 或各个 UI 控件里到处 new。
// 3. 给 UI 提供访问服务的入口，但不把业务逻辑写进 UI。
class AppContext {
public:
    AppContext();
    ~AppContext();

    AppContext(const AppContext&) = delete;
    AppContext& operator=(const AppContext&) = delete;

    // 初始化依赖图。Starter、Function、Agent 当前都通过 PythonNodeWorker 执行。
    void initialize();

    presentation::AppStore& appStore();
    presentation::WorkspaceController& workspaceController();
    presentation::PythonEnvironmentController& pythonEnvironmentController();
    presentation::WorkflowController& workflowController();
    presentation::NodeTemplateController& nodeTemplateController();
    presentation::RunController& runController();
    presentation::WorkspaceBrowserController& workspaceBrowserController();

private:
    void setPythonExecutable(const QString& pythonExecutable);

    // 这些对象生命周期跟随整个应用。
    // 使用 unique_ptr 是为了表达“AppContext 独占拥有这些对象”。
    std::unique_ptr<workers::WorkerRegistry> m_workerRegistry;
    std::unique_ptr<execution::ExecutionEngine> m_executionEngine;
    std::unique_ptr<application::WorkspaceService> m_workspaceService;
    std::unique_ptr<application::WorkflowService> m_workflowService;
    std::unique_ptr<application::NodeTemplateService> m_nodeTemplateService;
    std::unique_ptr<application::RunService> m_runService;
    std::shared_ptr<workers::PythonNodeWorker> m_pythonWorker;
    std::unique_ptr<presentation::AppStore> m_appStore;
    std::unique_ptr<presentation::WorkspaceController> m_workspaceController;
    std::unique_ptr<presentation::PythonEnvironmentController> m_pythonEnvironmentController;
    std::unique_ptr<presentation::WorkflowController> m_workflowController;
    std::unique_ptr<presentation::NodeTemplateController> m_nodeTemplateController;
    std::unique_ptr<presentation::RunController> m_runController;
    std::unique_ptr<presentation::WorkspaceBrowserController> m_workspaceBrowserController;
};

} // namespace vws
