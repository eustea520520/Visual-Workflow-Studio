# Architecture

The application keeps a small layered shape:

```text
app
  -> src/ui
  -> src/application
  -> src/domain
  -> src/execution
  -> src/workers
  -> src/infrastructure
```

`AppContext` is the composition root. It owns the long-lived services, the execution engine, the worker registry, and the shared `PythonNodeWorker`.

## Boundaries

- `MainWindow` composes widgets and routes user actions.
- `WorkspaceExplorer`, `WorkflowCanvas`, `NodeInspector`, and `OutputPanel` own UI state only.
- Application services own workspace, workflow, template, and run-record use cases.
- Domain objects own JSON conversion and stay independent of UI and execution.
- `ExecutionEngine` owns validation, concurrent DAG scheduling, event publication, cancellation, and result aggregation.
- Workers implement node execution behind `INodeWorker`.
- Infrastructure contains file-system and JSON helpers.

There is no database layer in the current app. Workspace data is persisted as JSON files in the workspace directory.
