# Architecture

The application keeps a small layered shape:

```text
app
  -> src/presentation
  -> src/ui
  -> src/application
  -> src/domain
  -> src/execution
  -> src/workers
  -> src/infrastructure
```

`AppContext` is the composition root. It owns the long-lived services, the execution engine, the worker registry, the shared `PythonNodeWorker`, the presentation `AppStore`, and controllers.

## Boundaries

- `MainWindow` composes widgets, opens dialogs, and renders controller/store results.
- `src/presentation/state/AppStore` is the single owner of workspace/workflow/run UI state that is not owned by a widget.
- `src/presentation/controllers` translates UI intent into application/execution calls and keeps execution events out of widgets.
- `WorkspaceExplorer`, `WorkflowCanvas`, `NodeInspector`, and `OutputPanel` own UI state only.
- Application services own workspace, workflow, template, and run-record use cases.
- Domain objects own JSON conversion and stay independent of UI and execution.
- `ExecutionEngine` owns validation, concurrent DAG scheduling, event publication, cancellation, and result aggregation.
- Workers implement node execution behind `INodeWorker`.
- Infrastructure contains file-system and JSON helpers.

There is no database layer in the current app. Workspace data is persisted as JSON files in the workspace directory.

## Current Refactor Slice

The first architecture cleanup introduced:

- CMake sanitizer switches: `VWS_ENABLE_ASAN`, `VWS_ENABLE_TSAN`, and `VWS_ENABLE_UBSAN`.
- `AppStore` for current workspace, current workflow, selected node id, node outputs, run id mapping, cached node statuses, and running workflow ids.
- `WorkspaceController` for create/open workspace and workspace Python interpreter updates.
- `WorkflowDocument` for the current workflow's in-memory lifetime, dirty state, revision, and immutable snapshots.
- `PythonCodeTemplates` and `NodeFactory` in the application layer, so default node construction no longer lives in canvas/UI code.
- `WorkflowController` for creating, loading, saving, editing, and synchronizing the current `WorkflowDocument`.
- `RunController` for subscribing to `ExecutionEventBus`, updating run-related `AppStore` state, forwarding UI-ready events, delegating run cancellation, and saving run records.

Qt ownership rules in this slice:

- `AppContext` owns services, controllers, execution engine, worker registry, and `AppStore` with `std::unique_ptr`.
- `MainWindow` owns widgets through QObject parent-child ownership.
- `RunController` owns its event-bus connections as the QObject receiver; they disconnect automatically when the controller is destroyed.
- `MainWindow` blocks canvas `workflowChanged` synchronization while it is rendering a loaded workflow, so loading a clean document is not mistaken for a user edit.
- `WorkflowCanvas` lets `QGraphicsScene` own graphics items and clears item maps before scene rebuilds. Scene signals are blocked during rebuild so stale selection events cannot observe half-cleared item maps.
- `WorkflowCanvas` still owns interaction and scene rendering, but Starter/Function/Agent default node data now comes from `NodeFactory`.
- `WorkflowSceneController` owns the non-owning indexes from node/edge ids to `QGraphicsItem` instances. It is the only canvas component that mutates those maps, removes graphics items, and recomputes edge routing contexts.
