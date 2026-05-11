cmake_minimum_required(VERSION 3.20)

project(VisualWorkflowStudio
    VERSION 0.1.0
    DESCRIPTION "Qt desktop visual workflow studio"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(DOMAIN_SOURCES
    src/domain/Workspace.h src/domain/Workspace.cpp
    src/domain/Workflow.h src/domain/Workflow.cpp
    src/domain/Node.h src/domain/Node.cpp
    src/domain/Edge.h src/domain/Edge.cpp
    src/domain/NodeTemplate.h src/domain/NodeTemplate.cpp
    src/domain/Artifact.h src/domain/Artifact.cpp
    src/domain/RunRecord.h src/domain/RunRecord.cpp
)

set(INFRA_SOURCES
    src/infrastructure/JsonUtils.h src/infrastructure/JsonUtils.cpp
    src/infrastructure/FileSystemUtils.h src/infrastructure/FileSystemUtils.cpp
)

set(APPLICATION_SOURCES
    src/application/WorkspaceService.h src/application/WorkspaceService.cpp
    src/application/WorkflowService.h src/application/WorkflowService.cpp
    src/application/NodeTemplateService.h src/application/NodeTemplateService.cpp
    src/application/RunService.h src/application/RunService.cpp
)

set(EXECUTION_SOURCES
    src/execution/ExecutionEngine.h src/execution/ExecutionEngine.cpp
    src/execution/GraphValidator.h src/execution/GraphValidator.cpp
    src/execution/ExecutionEventBus.h src/execution/ExecutionEventBus.cpp
    src/execution/ExecutionState.h
    src/execution/DataPacket.h
    src/execution/GraphIndexes.h src/execution/GraphIndexes.cpp
    src/execution/InputMerger.h src/execution/InputMerger.cpp
    src/execution/NodeReadinessTracker.h src/execution/NodeReadinessTracker.cpp
    src/execution/NodeExecutionRequest.h
    src/execution/NodeExecutionResult.h
    src/execution/WorkerPool.h src/execution/WorkerPool.cpp
)

set(WORKER_SOURCES
    src/workers/WorkerRegistry.h src/workers/WorkerRegistry.cpp
    src/workers/INodeWorker.h
    src/workers/MockNodeWorker.h src/workers/MockNodeWorker.cpp
    src/workers/PythonNodeWorker.h src/workers/PythonNodeWorker.cpp
)

set(UI_SOURCES
    src/ui/workspace/WorkspaceExplorer.h src/ui/workspace/WorkspaceExplorer.cpp
    src/ui/canvas/WorkflowCanvas.h src/ui/canvas/WorkflowCanvas.cpp
    src/ui/canvas/NodeGraphicsItem.h src/ui/canvas/NodeGraphicsItem.cpp
    src/ui/canvas/EdgeGraphicsItem.h src/ui/canvas/EdgeGraphicsItem.cpp
    src/ui/canvas/EdgeRouteTypes.h
    src/ui/canvas/EdgeRouter.h src/ui/canvas/EdgeRouter.cpp
    src/ui/canvas/EdgePathBuilder.h src/ui/canvas/EdgePathBuilder.cpp
    src/ui/canvas/ArrowHeadBuilder.h src/ui/canvas/ArrowHeadBuilder.cpp
    src/ui/editor/LineNumberArea.h src/ui/editor/LineNumberArea.cpp
    src/ui/editor/PythonCodeTemplates.h src/ui/editor/PythonCodeTemplates.cpp
    src/ui/editor/PythonCodeEditor.h src/ui/editor/PythonCodeEditor.cpp
    src/ui/editor/PythonSyntaxHighlighter.h src/ui/editor/PythonSyntaxHighlighter.cpp
    src/ui/editor/PythonCompleter.h src/ui/editor/PythonCompleter.cpp
    src/ui/editor/PythonNodeEditorDialog.h src/ui/editor/PythonNodeEditorDialog.cpp
    src/ui/inspector/NodeInspector.h src/ui/inspector/NodeInspector.cpp
    src/ui/output/OutputPanel.h src/ui/output/OutputPanel.cpp
)

set(APP_SOURCES
    app/main.cpp
    app/AppContext.h app/AppContext.cpp
    app/MainWindow.h app/MainWindow.cpp
)

qt_add_executable(visual-workflow-studio
    ${APP_SOURCES}
    ${APPLICATION_SOURCES}
    ${DOMAIN_SOURCES}
    ${EXECUTION_SOURCES}
    ${INFRA_SOURCES}
    ${WORKER_SOURCES}
    ${UI_SOURCES}
)

target_include_directories(visual-workflow-studio PRIVATE app src)
target_link_libraries(visual-workflow-studio PRIVATE Qt6::Widgets)

enable_testing()

function(vws_add_test name)
    set(target_name test_${name})
    qt_add_executable(${target_name} ${ARGN})
    target_include_directories(${target_name} PRIVATE src)
    target_link_libraries(${target_name} PRIVATE Qt6::Core)
    add_test(NAME ${name} COMMAND ${target_name})
    set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

vws_add_test(workflow_serialization
    tests/unit/test_workflow_serialization.cpp
    src/application/WorkflowService.h src/application/WorkflowService.cpp
    src/domain/Workflow.h src/domain/Workflow.cpp
    src/domain/Node.h src/domain/Node.cpp
    src/domain/Edge.h src/domain/Edge.cpp
    ${INFRA_SOURCES}
)

vws_add_test(graph_validator
    tests/unit/test_graph_validator.cpp
    src/application/WorkflowService.h src/application/WorkflowService.cpp
    src/domain/Workflow.h src/domain/Workflow.cpp
    src/domain/Node.h src/domain/Node.cpp
    src/domain/Edge.h src/domain/Edge.cpp
    src/execution/GraphValidator.h src/execution/GraphValidator.cpp
    ${INFRA_SOURCES}
)

vws_add_test(execution_engine
    tests/unit/test_execution_engine.cpp
    src/application/WorkflowService.h src/application/WorkflowService.cpp
    ${DOMAIN_SOURCES}
    ${EXECUTION_SOURCES}
    src/workers/INodeWorker.h
    src/workers/WorkerRegistry.h src/workers/WorkerRegistry.cpp
    src/workers/MockNodeWorker.h src/workers/MockNodeWorker.cpp
    ${INFRA_SOURCES}
)

vws_add_test(python_worker
    tests/integration/test_python_worker.cpp
    src/domain/Artifact.h
    src/execution/NodeExecutionRequest.h
    src/execution/NodeExecutionResult.h
    src/workers/INodeWorker.h
    src/workers/PythonNodeWorker.h src/workers/PythonNodeWorker.cpp
    ${INFRA_SOURCES}
)

vws_add_test(workspace_templates
    tests/unit/test_workspace_templates.cpp
    src/application/WorkspaceService.h src/application/WorkspaceService.cpp
    src/application/WorkflowService.h src/application/WorkflowService.cpp
    src/application/NodeTemplateService.h src/application/NodeTemplateService.cpp
    src/domain/Workspace.h src/domain/Workspace.cpp
    src/domain/Workflow.h src/domain/Workflow.cpp
    src/domain/Node.h src/domain/Node.cpp
    src/domain/Edge.h src/domain/Edge.cpp
    src/domain/NodeTemplate.h src/domain/NodeTemplate.cpp
    ${INFRA_SOURCES}
)

qt_add_executable(test_canvas_items
    tests/unit/test_canvas_items.cpp
    src/domain/Workflow.h src/domain/Workflow.cpp
    src/domain/Node.h src/domain/Node.cpp
    src/domain/Edge.h src/domain/Edge.cpp
    src/ui/canvas/WorkflowCanvas.h src/ui/canvas/WorkflowCanvas.cpp
    src/ui/canvas/NodeGraphicsItem.h src/ui/canvas/NodeGraphicsItem.cpp
    src/ui/canvas/EdgeGraphicsItem.h src/ui/canvas/EdgeGraphicsItem.cpp
    src/ui/canvas/EdgeRouteTypes.h
    src/ui/canvas/EdgeRouter.h src/ui/canvas/EdgeRouter.cpp
    src/ui/canvas/EdgePathBuilder.h src/ui/canvas/EdgePathBuilder.cpp
    src/ui/canvas/ArrowHeadBuilder.h src/ui/canvas/ArrowHeadBuilder.cpp
    src/ui/editor/PythonCodeTemplates.h src/ui/editor/PythonCodeTemplates.cpp
)
target_include_directories(test_canvas_items PRIVATE src)
target_link_libraries(test_canvas_items PRIVATE Qt6::Widgets)
add_test(NAME canvas_items COMMAND test_canvas_items)
set_tests_properties(canvas_items PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(test_output_panel
    tests/unit/test_output_panel.cpp
    src/domain/Artifact.h
    src/ui/output/OutputPanel.h src/ui/output/OutputPanel.cpp
    src/ui/editor/LineNumberArea.h src/ui/editor/LineNumberArea.cpp
    src/ui/editor/PythonCodeEditor.h src/ui/editor/PythonCodeEditor.cpp
    src/ui/editor/PythonSyntaxHighlighter.h src/ui/editor/PythonSyntaxHighlighter.cpp
    src/ui/editor/PythonCompleter.h src/ui/editor/PythonCompleter.cpp
    src/execution/ExecutionEngine.h
    src/execution/NodeExecutionResult.h
)
target_include_directories(test_output_panel PRIVATE src)
target_link_libraries(test_output_panel PRIVATE Qt6::Widgets)
add_test(NAME output_panel COMMAND test_output_panel)
set_tests_properties(output_panel PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

vws_add_test(run_service
    tests/unit/test_run_service.cpp
    src/application/RunService.h src/application/RunService.cpp
    src/domain/RunRecord.h src/domain/RunRecord.cpp
    src/domain/Artifact.h src/domain/Artifact.cpp
    src/domain/Workflow.h src/domain/Workflow.cpp
    src/domain/Node.h src/domain/Node.cpp
    src/domain/Edge.h src/domain/Edge.cpp
    ${EXECUTION_SOURCES}
    src/workers/INodeWorker.h
    src/workers/WorkerRegistry.h src/workers/WorkerRegistry.cpp
    ${INFRA_SOURCES}
)

qt_add_executable(test_python_editor
    tests/unit/test_python_editor.cpp
    src/application/WorkflowService.h src/application/WorkflowService.cpp
    src/domain/Workflow.h src/domain/Workflow.cpp
    src/domain/Node.h src/domain/Node.cpp
    src/domain/Edge.h src/domain/Edge.cpp
    src/ui/editor/LineNumberArea.h src/ui/editor/LineNumberArea.cpp
    src/ui/editor/PythonCodeTemplates.h src/ui/editor/PythonCodeTemplates.cpp
    src/ui/editor/PythonCodeEditor.h src/ui/editor/PythonCodeEditor.cpp
    src/ui/editor/PythonSyntaxHighlighter.h src/ui/editor/PythonSyntaxHighlighter.cpp
    src/ui/editor/PythonCompleter.h src/ui/editor/PythonCompleter.cpp
    ${INFRA_SOURCES}
)
target_include_directories(test_python_editor PRIVATE src)
target_link_libraries(test_python_editor PRIVATE Qt6::Widgets)
add_test(NAME python_editor COMMAND test_python_editor)
set_tests_properties(python_editor PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
    set(VWS_WINDOWS_TEST_PATH
        "PATH=path_list_prepend:C:/Qt/Tools/mingw1120_64/bin"
        "PATH=path_list_prepend:C:/Qt/6.5.3/mingw_64/bin"
    )
    set_tests_properties(
        workflow_serialization
        graph_validator
        execution_engine
        python_worker
        workspace_templates
        canvas_items
        output_panel
        run_service
        python_editor
        PROPERTIES ENVIRONMENT_MODIFICATION "${VWS_WINDOWS_TEST_PATH}"
    )
endif()

if(WIN32)
    set_target_properties(visual-workflow-studio PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
