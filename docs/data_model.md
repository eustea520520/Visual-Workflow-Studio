# Data Model

The domain layer contains the stable JSON model used by the current app:

- `Workspace`
- `Workflow`
- `Node`
- `Edge`
- `NodeTemplate`
- `Artifact`
- `RunRecord`

These classes live under `src/domain/`. They do not depend on Qt widgets, application services, execution code, or worker implementations. Their job is to hold workflow data and convert it to/from JSON.

## Workflow Files

`WorkflowService` owns workflow use cases:

- `createEmptyWorkflow(...)`
- `loadWorkflow(...)`
- `saveWorkflow(...)`
- `listWorkflows(...)`
- `updateNodeDetails(...)`

File IO and JSON parsing are delegated to `JsonUtils` and `FileSystemUtils`, so services do not duplicate persistence details.

## Workspace Files

A workspace is a directory:

```text
workspace/
  workspace.json
  workflows/
  node_templates/
  runs/
  artifacts/
  secrets/
  cache/
  logs/
```

`WorkspaceService` creates and opens this structure. `workspace.json` stores workspace metadata and `config.python_executable`, which is the single Python interpreter used by every Python-backed node in that workspace.

## Template Reuse

The active template flow is:

1. Convert a selected `Node` to `NodeTemplate`.
2. Save it under `node_templates/<templateId>.json`.
3. Load it later from the browser/menu.
4. Convert it back to a fresh `Node`.

The created node gets a new `nodeId`; type, ports, description, and config are copied from the template.
