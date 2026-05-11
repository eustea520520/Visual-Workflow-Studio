# UI Design

The UI is a Qt Widgets desktop shell:

- Left: `WorkspaceExplorer`.
- Center: `WorkflowCanvas`.
- Right: `NodeInspector`.
- Bottom: `OutputPanel`.
- Status bar: selected node timeout and workspace Python interpreter.

## Main Window

`MainWindow` composes widgets and routes user actions to application services. It does not own workflow persistence logic directly. It keeps the current workspace/workflow selection, updates the canvas, refreshes the browser, and bridges execution events into the UI.

## Canvas

`WorkflowCanvas` owns the graphics scene representation of the current workflow:

- right-click node creation;
- Starter/Function/Agent template menus;
- manual edge dragging;
- selected-node connection action;
- node dragging and position persistence in memory;
- Delete/Backspace removal;
- Ctrl+S workflow save request;
- Ctrl+Z undo;
- Ctrl+wheel zoom;
- Shift+wheel horizontal pan;
- right-button drag pan;
- workspace/workflow empty-state overlays.

The canvas updates the in-memory `Workflow`; `WorkflowService` remains responsible for writing JSON files.

## Node Editing

Double-clicking a Python-backed node opens `PythonNodeEditorDialog`.

Function and Starter nodes allow editing title, one-line description, and Python code. Agent nodes expose structured Agent settings and show generated Python code in read-only mode. Saving updates the in-memory node through `WorkflowService::updateNodeDetails(...)`.

## Inspector

`NodeInspector` previews the selected node. Starter and Function nodes cannot select the Agent page. Agent nodes show title, description, timeout, URL, model name, API key, background prompt, and task prompt.

## Output Panel

`OutputPanel` displays:

- workflow and node timeline events;
- node run table;
- application logs;
- Python debug output from `print()`;
- stderr;
- traceback;
- output JSON;
- artifacts and text/table previews;
- thread trace events.

It consumes execution events and final execution results. It does not run workflows.
