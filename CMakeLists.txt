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

option(VWS_ENABLE_ASAN "Enable AddressSanitizer when supported by the compiler" OFF)
option(VWS_ENABLE_TSAN "Enable ThreadSanitizer when supported by the compiler" OFF)
option(VWS_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer when supported by the compiler" OFF)

if(VWS_ENABLE_ASAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(VWS_ENABLE_TSAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

if(VWS_ENABLE_UBSAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

set(DOMAIN_SOURCES
    src/domain/Workspace.h src/domain/Workspace.cpp
    src/domain/Workflow.h src/domain/Workflow.cpp
    src/domain/Node.h src/domain/Node.cpp
    src/domain/NodeIoSpec.h src/domain/NodeIoSpec.cpp
    src/domain/NodeConfigKeys.h
    src/domain/NodeConfigView.h
    src/domain/NodeTypes.h
    src/domain/EdgeEndpoint.h
    src/domain/Edge.h src/domain/Edge.cpp
    src/domain/NodeTemplate.h src/domain/NodeTemplate.cpp
    src/domain/Artifact.h src/domain/Artifact.cpp
    src/domain/RunRecord.h src/domain/RunRecord.cpp
)

set(INFRA_SOURCES
    src/infrastructure/JsonUtils.h src/infrastructure/JsonUtils.cpp
    src/infrastructure/FileSystemUtils.h src/infrastructure/FileSystemUtils.cpp
    src/infrastructure/llm/LlmChatTypes.h
    src/infrastructure/llm/LlmChatClient.h src/infrastructure/llm/LlmChatClient.cpp
)

set(APPLICATION_SOURCES
    src/application/WorkflowDocument.h src/application/WorkflowDocument.cpp
    src/application/WorkflowEditService.h src/application/WorkflowEditService.cpp
    src/application/WorkflowHistory.h src/application/WorkflowHistory.cpp
    src/application/WorkflowClipboard.h src/application/WorkflowClipboard.cpp
    src/application/PythonCodeTemplates.h src/application/PythonCodeTemplates.cpp
    src/application/NodeFactory.h src/application/NodeFactory.cpp
    src/application/WorkspaceService.h src/application/WorkspaceService.cpp
    src/application/WorkflowService.h src/application/WorkflowService.cpp
    src/application/NodeTemplateService.h src/application/NodeTemplateService.cpp
    src/application/RunService.h src/application/RunService.cpp
    src/application/io/NodeIoSpecUtils.h src/application/io/NodeIoSpecUtils.cpp
    src/application/io/PythonIoDimensionAnalyzer.h src/application/io/PythonIoDimensionAnalyzer.cpp
    src/application/io/NodeIoSpecValidator.h src/application/io/NodeIoSpecValidator.cpp
    src/application/subsystem/SubsystemTypes.h src/application/subsystem/SubsystemTypes.cpp
    src/application/subsystem/SubsystemBoundaryInferer.h src/application/subsystem/SubsystemBoundaryInferer.cpp
    src/application/subsystem/SubsystemService.h src/application/subsystem/SubsystemService.cpp
    src/application/subsystem/SubsystemIoMapper.h src/application/subsystem/SubsystemIoMapper.cpp
    src/application/generation/WorkflowGenerationTypes.h src/application/generation/WorkflowGenerationTypes.cpp
    src/application/generation/WorkflowGenerationTemplateCatalog.h src/application/generation/WorkflowGenerationTemplateCatalog.cpp
    src/application/generation/WorkflowGenerationPromptBuilder.h src/application/generation/WorkflowGenerationPromptBuilder.cpp
    src/application/generation/WorkflowSkeletonValidator.h src/application/generation/WorkflowSkeletonValidator.cpp
    src/application/generation/WorkflowNodeImplementationValidator.h src/application/generation/WorkflowNodeImplementationValidator.cpp
    src/application/generation/WorkflowGenerationAssembler.h src/application/generation/WorkflowGenerationAssembler.cpp
    src/application/generation/WorkflowGenerationValidator.h src/application/generation/WorkflowGenerationValidator.cpp
    src/application/generation/WorkflowGenerationNormalizer.h src/application/generation/WorkflowGenerationNormalizer.cpp
    src/application/generation/WorkflowAutoLayout.h src/application/generation/WorkflowAutoLayout.cpp
    src/application/generation/WorkflowGenerationService.h src/application/generation/WorkflowGenerationService.cpp
)

set(PRESENTATION_SOURCES
    src/presentation/state/AppState.h
    src/presentation/state/AppStore.h src/presentation/state/AppStore.cpp
    src/presentation/models/WorkflowDisplayModel.h src/presentation/models/WorkflowDisplayModel.cpp
    src/presentation/controllers/WorkspaceController.h src/presentation/controllers/WorkspaceController.cpp
    src/presentation/controllers/PythonEnvironmentController.h src/presentation/controllers/PythonEnvironmentController.cpp
    src/presentation/controllers/WorkflowController.h src/presentation/controllers/WorkflowController.cpp
    src/presentation/controllers/NodeTemplateController.h src/presentation/controllers/NodeTemplateController.cpp
    src/presentation/controllers/RunController.h src/presentation/controllers/RunController.cpp
    src/presentation/controllers/WorkspaceBrowserController.h src/presentation/controllers/WorkspaceBrowserController.cpp
    src/presentation/controllers/CanvasNavigationController.h src/presentation/controllers/CanvasNavigationController.cpp
    src/presentation/controllers/WorkflowIoController.h src/presentation/controllers/WorkflowIoController.cpp
    src/presentation/controllers/WorkflowGenerationController.h src/presentation/controllers/WorkflowGenerationController.cpp
)

set(EXECUTION_SOURCES
    src/execution/ExecutionEngine.h src/execution/ExecutionEngine.cpp
    src/execution/ExecutionCancellationState.h src/execution/ExecutionCancellationState.cpp
    src/execution/ExecutionRunState.h src/execution/ExecutionRunState.cpp
    src/execution/ExecutionPlanner.h src/execution/ExecutionPlanner.cpp
    src/execution/ExecutionSchedulingPolicy.h src/execution/ExecutionSchedulingPolicy.cpp
    src/execution/WorkflowExecutionResult.h
    src/execution/WorkflowRunOptions.h
    src/execution/GraphValidator.h src/execution/GraphValidator.cpp
    src/execution/LoopNodeExecutor.h src/execution/LoopNodeExecutor.cpp
    src/execution/ExecutionEventBus.h src/execution/ExecutionEventBus.cpp
    src/execution/ExecutionState.h
    src/execution/DataPacket.h
    src/execution/GraphIndexes.h src/execution/GraphIndexes.cpp
    src/execution/InputMerger.h src/execution/InputMerger.cpp
    src/execution/NodeReadinessTracker.h src/execution/NodeReadinessTracker.cpp
    src/execution/NodeExecutionRequest.h
    src/execution/NodeExecutionResult.h
    src/execution/NodeExecutionRequestBuilder.h src/execution/NodeExecutionRequestBuilder.cpp
    src/execution/NodeTaskRunner.h src/execution/NodeTaskRunner.cpp
    src/execution/NodeResultStatus.h src/execution/NodeResultStatus.cpp
    src/execution/ExecutionOutputRouter.h src/execution/ExecutionOutputRouter.cpp
    src/execution/ThreadTrace.h src/execution/ThreadTrace.cpp
    src/execution/WorkerPool.h src/execution/WorkerPool.cpp
)

set(WORKER_SOURCES
    src/workers/WorkerRegistry.h src/workers/WorkerRegistry.cpp
    src/workers/INodeWorker.h
    src/workers/MockNodeWorker.h src/workers/MockNodeWorker.cpp
    src/workers/PythonNodeWorker.h src/workers/PythonNodeWorker.cpp
    src/workers/PythonNodeOutputStore.h src/workers/PythonNodeOutputStore.cpp
    src/workers/PythonProcessRunner.h src/workers/PythonProcessRunner.cpp
    src/workers/PythonWorkerProtocol.h src/workers/PythonWorkerProtocol.cpp
)

set(UI_SOURCES
    src/ui/main/MainWindowLayoutBuilder.h src/ui/main/MainWindowLayoutBuilder.cpp
    src/ui/workspace/WorkspaceExplorerViewModel.h
    src/ui/workspace/WorkspaceExplorer.h src/ui/workspace/WorkspaceExplorer.cpp
    src/ui/canvas/WorkflowCanvas.h src/ui/canvas/WorkflowCanvas.cpp
    src/ui/canvas/CanvasBreadcrumbViewModel.h
    src/ui/canvas/CanvasHeader.h src/ui/canvas/CanvasHeader.cpp
    src/ui/canvas/WorkflowCanvasContextMenu.h src/ui/canvas/WorkflowCanvasContextMenu.cpp
    src/ui/canvas/WorkflowCanvasInteractionController.h src/ui/canvas/WorkflowCanvasInteractionController.cpp
    src/ui/canvas/WorkflowSceneController.h src/ui/canvas/WorkflowSceneController.cpp
    src/ui/canvas/EdgeDragController.h src/ui/canvas/EdgeDragController.cpp
    src/ui/canvas/NodePortSlotViewModel.h
    src/ui/canvas/NodePortSlotViewModelBuilder.h src/ui/canvas/NodePortSlotViewModelBuilder.cpp
    src/ui/canvas/PortSlotHit.h
    src/ui/canvas/NodeGraphicsItem.h src/ui/canvas/NodeGraphicsItem.cpp
    src/ui/canvas/EdgeGraphicsItem.h src/ui/canvas/EdgeGraphicsItem.cpp
    src/ui/canvas/CanvasZ.h
    src/ui/canvas/EdgeRouteTypes.h
    src/ui/canvas/EdgeRouter.h src/ui/canvas/EdgeRouter.cpp
    src/ui/canvas/EdgePathBuilder.h src/ui/canvas/EdgePathBuilder.cpp
    src/ui/canvas/ArrowHeadBuilder.h src/ui/canvas/ArrowHeadBuilder.cpp
    src/ui/editor/LineNumberArea.h src/ui/editor/LineNumberArea.cpp
    src/ui/editor/PythonCodeEditor.h src/ui/editor/PythonCodeEditor.cpp
    src/ui/editor/PythonNodeEditorDialog.h src/ui/editor/PythonNodeEditorDialog.cpp
    src/ui/editor/PythonSyntaxHighlighter.h src/ui/editor/PythonSyntaxHighlighter.cpp
    src/ui/editor/PythonCompleter.h src/ui/editor/PythonCompleter.cpp
    src/ui/generation/WorkflowGenerationDialogViewModel.h
    src/ui/generation/WorkflowGenerationDialog.h src/ui/generation/WorkflowGenerationDialog.cpp
    src/ui/inspector/NodeInspectorViewModel.h
    src/ui/inspector/NodeInspector.h src/ui/inspector/NodeInspector.cpp
    src/ui/output/OutputPanelViewModel.h
    src/ui/output/OutputPanel.h src/ui/output/OutputPanel.cpp
    src/ui/theme/ThemeManager.h src/ui/theme/ThemeManager.cpp
    src/ui/theme/UiMetrics.h
    src/ui/theme/StyleReloader.h src/ui/theme/StyleReloader.cpp
    src/ui/widgets/IconSquareButton.h src/ui/widgets/IconSquareButton.cpp
    src/ui/widgets/CommandBar.h src/ui/widgets/CommandBar.cpp
    src/ui/widgets/EmptyStateOverlay.h src/ui/widgets/EmptyStateOverlay.cpp
)

set(APP_SOURCES
    app/main.cpp
    app/AppContext.h app/AppContext.cpp
    app/MainWindow.h app/MainWindow.cpp
)

add_library(vws_domain STATIC ${DOMAIN_SOURCES})
target_include_directories(vws_domain PUBLIC src)
target_link_libraries(vws_domain PUBLIC Qt6::Core)

add_library(vws_infrastructure STATIC ${INFRA_SOURCES})
target_include_directories(vws_infrastructure PUBLIC src)
target_link_libraries(vws_infrastructure PUBLIC Qt6::Core)
target_link_libraries(vws_infrastructure PUBLIC Qt6::Network)

add_library(vws_application STATIC ${APPLICATION_SOURCES})
target_include_directories(vws_application PUBLIC src)
target_link_libraries(vws_application PUBLIC vws_domain vws_infrastructure Qt6::Core)

add_library(vws_workers STATIC ${WORKER_SOURCES})
target_include_directories(vws_workers PUBLIC src)
target_link_libraries(vws_workers PUBLIC vws_domain vws_infrastructure Qt6::Core)

add_library(vws_execution STATIC ${EXECUTION_SOURCES})
target_include_directories(vws_execution PUBLIC src)
target_link_libraries(vws_execution PUBLIC vws_domain vws_workers Qt6::Core)

add_library(vws_presentation STATIC ${PRESENTATION_SOURCES})
target_include_directories(vws_presentation PUBLIC src)
target_link_libraries(vws_presentation PUBLIC vws_application vws_execution Qt6::Core)

add_library(vws_ui STATIC ${UI_SOURCES})
target_include_directories(vws_ui PUBLIC src)
target_link_libraries(vws_ui PUBLIC vws_domain vws_application vws_presentation vws_execution Qt6::Widgets)

qt_add_executable(visual-workflow-studio
    ${APP_SOURCES}
    resources/resources.qrc
)

if(WIN32)
    target_sources(visual-workflow-studio PRIVATE resources/app.rc)
endif()

target_include_directories(visual-workflow-studio PRIVATE app src)
target_link_libraries(visual-workflow-studio PRIVATE vws_ui vws_presentation vws_execution vws_workers vws_application vws_infrastructure vws_domain Qt6::Widgets)

enable_testing()

function(vws_add_test name)
    set(target_name test_${name})
    qt_add_executable(${target_name} ${ARGN})
    target_include_directories(${target_name} PRIVATE src)
    target_link_libraries(${target_name} PRIVATE vws_presentation vws_execution vws_workers vws_application vws_infrastructure vws_domain Qt6::Core)
    add_test(NAME ${name} COMMAND ${target_name})
    set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

vws_add_test(workflow_serialization
    tests/unit/test_workflow_serialization.cpp
)

vws_add_test(graph_validator
    tests/unit/test_graph_validator.cpp
)

vws_add_test(execution_engine
    tests/unit/test_execution_engine.cpp
)

vws_add_test(python_worker
    tests/integration/test_python_worker.cpp
)

vws_add_test(workspace_templates
    tests/unit/test_workspace_templates.cpp
)

vws_add_test(workflow_document
    tests/unit/test_workflow_document.cpp
)

vws_add_test(subsystem_service
    tests/unit/test_subsystem_service.cpp
)

vws_add_test(node_factory
    tests/unit/test_node_factory.cpp
)

vws_add_test(node_io_spec
    tests/unit/test_node_io_spec.cpp
)

vws_add_test(python_io_dimension_analyzer
    tests/unit/test_python_io_dimension_analyzer.cpp
)

vws_add_test(workflow_generation_prompt
    tests/unit/test_workflow_generation_prompt.cpp
)

vws_add_test(workflow_generation_validator
    tests/unit/test_workflow_generation_validator.cpp
)

vws_add_test(workflow_generation_multistage
    tests/unit/test_workflow_generation_multistage.cpp
)

vws_add_test(presentation_state
    tests/unit/test_presentation_state.cpp
)

qt_add_executable(test_canvas_items
    tests/unit/test_canvas_items.cpp
)
target_include_directories(test_canvas_items PRIVATE src)
target_link_libraries(test_canvas_items PRIVATE vws_ui Qt6::Widgets)
add_test(NAME canvas_items COMMAND test_canvas_items)
set_tests_properties(canvas_items PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(test_output_panel
    tests/unit/test_output_panel.cpp
)
target_include_directories(test_output_panel PRIVATE src)
target_link_libraries(test_output_panel PRIVATE vws_ui Qt6::Widgets)
add_test(NAME output_panel COMMAND test_output_panel)
set_tests_properties(output_panel PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(test_empty_state_overlay
    tests/unit/test_empty_state_overlay.cpp
)
target_include_directories(test_empty_state_overlay PRIVATE src)
target_link_libraries(test_empty_state_overlay PRIVATE vws_ui Qt6::Widgets)
add_test(NAME empty_state_overlay COMMAND test_empty_state_overlay)
set_tests_properties(empty_state_overlay PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(test_theme_manager
    tests/unit/test_theme_manager.cpp
    resources/resources.qrc
)
target_include_directories(test_theme_manager PRIVATE src)
target_link_libraries(test_theme_manager PRIVATE vws_ui Qt6::Widgets)
add_test(NAME theme_manager COMMAND test_theme_manager)
set_tests_properties(theme_manager PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(test_main_window_layout_builder
    tests/unit/test_main_window_layout_builder.cpp
)
target_include_directories(test_main_window_layout_builder PRIVATE src)
target_link_libraries(test_main_window_layout_builder PRIVATE vws_ui Qt6::Widgets)
add_test(NAME main_window_layout_builder COMMAND test_main_window_layout_builder)
set_tests_properties(main_window_layout_builder PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

vws_add_test(run_service
    tests/unit/test_run_service.cpp
)

qt_add_executable(test_python_editor
    tests/unit/test_python_editor.cpp
)
target_include_directories(test_python_editor PRIVATE src)
target_link_libraries(test_python_editor PRIVATE vws_ui Qt6::Widgets)
add_test(NAME python_editor COMMAND test_python_editor)
set_tests_properties(python_editor PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(test_workflow_generation_dialog
    tests/unit/test_workflow_generation_dialog.cpp
)
target_include_directories(test_workflow_generation_dialog PRIVATE src)
target_link_libraries(test_workflow_generation_dialog PRIVATE vws_ui Qt6::Widgets)
add_test(NAME workflow_generation_dialog COMMAND test_workflow_generation_dialog)
set_tests_properties(workflow_generation_dialog PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

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
        workflow_document
        node_factory
        presentation_state
            canvas_items
            output_panel
            empty_state_overlay
            theme_manager
            main_window_layout_builder
            run_service
            subsystem_service
            python_editor
            node_io_spec
            python_io_dimension_analyzer
            workflow_generation_prompt
            workflow_generation_validator
            workflow_generation_multistage
            workflow_generation_dialog
        PROPERTIES ENVIRONMENT_MODIFICATION "${VWS_WINDOWS_TEST_PATH}"
    )
endif()

if(WIN32)
    set_target_properties(visual-workflow-studio PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
