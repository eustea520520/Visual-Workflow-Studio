# Visual Workflow Studio

Visual Workflow Studio is a C++/Qt desktop application for building and running visual DAG workflows with Python-backed nodes.

## Current Features

- Workspace creation/opening with JSON-backed settings.
- Workflow creation, loading, saving, and canvas editing.
- Starter, Function, and Agent nodes backed by Python execution.
- Workspace-level Python interpreter selection persisted in `workspace.json`.
- Reusable node templates saved from selected canvas nodes.
- Concurrent DAG execution with join waiting, branch failure isolation, cancellation, and per-node status events.
- Output panel with timeline, node results, debug `print()` output, stderr, traceback, output JSON, artifacts, and thread trace.
- Edge drawing with obstacle-aware routing and centered turns.

## Architecture

The application is split by responsibility:

- `app/`: application bootstrap, dependency composition, and the main window.
- `src/ui/`: Qt widgets, canvas items, editor widgets, inspector, workspace browser, and output panel.
- `src/application/`: use-case services for workspaces, workflows, node templates, and run records.
- `src/domain/`: workflow data models and JSON conversion.
- `src/execution/`: graph validation, concurrent DAG execution, input merging, worker pool, and event bus.
- `src/workers/`: node worker interface, registry, mock worker, and Python worker.
- `src/infrastructure/`: JSON and file-system utilities.
- `python/`: isolated Python worker process and protocol helpers.
- `tests/`: unit and integration regression tests.

There is no database layer. The current application state is stored as JSON files inside the selected workspace directory.

## Workspace Layout

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

`workspace.json` stores workspace-level configuration, including `config.python_executable`. Workflows cannot run until a Python interpreter is selected for the workspace.

## Node Types

- `starter`: no input ports; creates the initial workflow output.
- `function`: accepts workflow inputs and runs user Python code.
- `agent`: runs editable Python code, with optional structured settings for URL, model name, API key, background prompt, and task prompt.

All three node types execute through `PythonNodeWorker`; the node type is kept so UI behavior, validation, and templates stay distinct. In Agent nodes, `Load Agent Template` can regenerate code from the structured Agent fields, but Save preserves the current editor contents.

## Data Transfer

Every Python node returns:

```python
return {
    "outputs": {
        "output": {}
    },
    "artifacts": []
}
```

Small business data should be passed through `outputs["output"]`. Large files should be written under `context["artifact_path"]`, passed downstream through a `file_path` value, and registered in `artifacts` for display in the output panel.

When multiple upstream nodes connect to the same input port, `InputMerger` puts their extracted `output` values into a stable list.

## Build

Requirements:

- CMake 3.20 or newer.
- C++17 compiler.
- Qt 6 with the Widgets module.

Configure and build:

```powershell
cmake -S . -B build -G "Ninja"
cmake --build build
```

Run tests:

```powershell
ctest --test-dir build --output-on-failure
```

If Qt is installed outside CMake's default search path, pass `CMAKE_PREFIX_PATH` when configuring.
