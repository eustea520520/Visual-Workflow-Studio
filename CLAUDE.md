# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

```powershell
# Configure (Ninja generator, repo root)
cmake -S . -B build -G "Ninja"

# Build
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Single test
ctest --test-dir build --output-on-failure -R execution_engine
```

Qt 6 Widgets is the only dependency. If Qt is not on CMake's default path: `-DCMAKE_PREFIX_PATH=C:/Qt/6.5.3/mingw_64`. Requires CMake 3.20+, C++17.

## Layers (bottom-up)

```
src/infrastructure/   File I/O, JSON read/write, directory helpers — no business logic
src/domain/           Plain structs with toJson()/fromJson() — no Qt Widgets, no execution
src/workers/          INodeWorker interface + registry + implementations (Python, Mock)
src/execution/        DAG validation, concurrent scheduling, event bus, input merging
src/application/      Use-case services (workspace, workflow, templates, run records)
src/ui/               Qt widgets: canvas, editor, inspector, output panel, theme
app/                  Composition root: main(), AppContext, MainWindow
python/               Standalone Python worker process (QProcess, stdin/stdout JSON)
```

### Domain Layer (`src/domain/`)

Plain data structs under `vws::domain`. Every struct has `toJson()` and `static fromJson()`. No dependencies on Qt Widgets or execution code.

- **Workflow** — aggregate root: nodes + edges + metadata. Serialization calls Node::toJson/Edge::toJson recursively.
- **Node** — canvas node instance: id, type, name, position, ports, config (QJsonObject holding `code`, `language`, `entry`, and type-specific fields), runtime settings (timeout, retries, memory, concurrency group).
- **Edge** — directed connection: fromNode/fromPort → toNode/toPort.
- **Workspace** — top-level organization unit: id, name, rootPath, config (contains `python_executable`).
- **NodeTemplate** — reusable node blueprint. Separate from Node (no position, no runtime). `createNodeFromTemplate()` generates a fresh Node from it.
- **Artifact** — reference to a file produced by node execution (type, path, metadata).
- **RunRecord / NodeRunRecord** — execution history. Stores paths to stdout/stderr/error/output files rather than embedding content.

### Infrastructure Layer (`src/infrastructure/`)

- **JsonUtils** — `readObjectFromFile()` / `writeObjectToFile()`. Handles QFile open errors, JSON parse errors, missing parent directories.
- **FileSystemUtils** — `ensureDirectory()`, `listFiles()` (filtered, sorted), `copyFile()` (with parent mkpath), `safeFileStem()` (sanitizes names for filesystem use).

### Worker Layer (`src/workers/`)

- **INodeWorker** — abstract interface: `type()`, `execute(NodeExecutionRequest) → NodeExecutionResult`, `cancel(runId)`.
- **WorkerRegistry** — maps node type string → `shared_ptr<INodeWorker>`. Same worker instance can serve multiple types (e.g., PythonNodeWorker serves "function", "starter", and "agent").
- **MockNodeWorker** — echoes inputs as outputs. Supports `config.mock_fail` (simulates failure) and `config.mock_delay_ms` (simulates work). Used in execution engine tests.
- **PythonNodeWorker** — spawns `python/python_worker.py` via QProcess. Sends one JSON request on stdin, reads one JSON response on stdout. Handles timeout (kills process), cancellation (tracks active processes by runId), artifact validation (checks files exist), and output persistence (writes `<nodeId>_output.json`).

### Execution Layer (`src/execution/`)

- **ExecutionState.h** — enums: `NodeStatus` (Idle→Pending→Waiting→Queued→Running→Succeeded/Failed/Skipped/Cancelled/Timeout) and `WorkflowStatus` (Created→Validating→Running→Succeeded/Failed/Cancelled/PartiallySucceeded/Timeout). Includes `nodeStatusToString()` / `workflowStatusToString()` helpers.
- **NodeExecutionRequest** — what ExecutionEngine passes to workers: runId, nodeId, nodeType, nodeConfig, inputs, paths, timeoutMs.
- **NodeExecutionResult** — what workers return: success, outputs, artifacts, stdoutText, stderrText, errorMessage, errorStack.
- **DataPacket** — represents data flowing across an edge. Holds the upstream value (extracted from outputs by port name), artifacts, and edge metadata.
- **GraphValidator** — checks: nodes not empty, unique node/edge IDs, has at least one Starter, Starter has no inputs, Starter has "output" port, edges reference valid nodes and ports, no cycles (DFS), all non-Starter nodes have incoming edges, all nodes reachable from a Starter (BFS from starters following adjacency).
- **GraphIndexes** — pre-built lookup tables: nodesById, incomingEdgesByNode, outgoingEdgesByNode, incomingEdgesByNodePort. Built once before scheduling to avoid repeated scans.
- **NodeReadinessTracker** — `isReady()` returns true when all of a node's incoming edges have completed data in `completedEdgeData`.
- **InputMerger** — builds a downstream node's `inputs` QJsonObject by port. Single edge: passes value directly. Multiple edges to same port: builds a QJsonArray (stable edge order).
- **WorkerPool** — wrapper around QThreadPool. Default thread count: `max(2, idealThreadCount())`.
- **ExecutionEventBus** — Qt signal bridge from execution layer to UI. Publishes workflow/node status changes, output ready, errors, and thread trace events. Status enums are converted to strings so UI/QML/logging can consume them directly.
- **ExecutionEngine** — main DAG scheduler. `runWorkflow()`: validates → builds indexes → marks all nodes Pending → schedules all Starter nodes → processes completions in worker threads protected by a QMutex + QWaitCondition → marks join nodes Waiting until all incoming edges complete → merges inputs → skips downstream of failed nodes → returns Succeeded/PartiallySucceeded/Failed/Cancelled. `runWorkflowAsync()` wraps this in a single background QThreadPool thread and invokes callback on the UI thread via QMetaObject::invokeMethod. `requestCancelCurrentRun()` sets an atomic flag and calls `worker->cancel(runId)` on every unique worker.

### Application Layer (`src/application/`)

Use-case services that sit between UI and domain/infrastructure.

- **WorkspaceService** — create/open/save workspace. Manages directory structure (workflows/, node_templates/, runs/, etc.). Reads/writes `workspace.json`. Tracks the active workspace in memory.
- **WorkflowService** — createEmptyWorkflow (generates UUID + timestamps), load/save (delegates to JsonUtils), listWorkflows (scans workflows/ folder), updateNodeCode/updateNodeDetails (finds node by id and updates config fields). File path: `workflows/<workflowId>.json`.
- **NodeTemplateService** — createTemplateFromNode (copies ports/config, assigns new UUID), createNodeFromTemplate (creates fresh Node from template), save/load/list templates, importTemplateFile (validates template structure, reassigns workspace ID, saves to target workspace).
- **RunService** — `saveRunRecord()` builds a RunRecord from `WorkflowExecutionResult` and writes it to `runs/<runId>/run_record.json`. `recentRuns()` lists run directories sorted by modification time.

### UI Layer (`src/ui/`)

#### Canvas (`src/ui/canvas/`)

- **WorkflowCanvas** — QGraphicsView managing the scene. Handles: right-click context menu for node creation (Starter with 3 templates × Function × Agent each with 4 data transfer modes), drag-to-connect edges (mousedown on output port → drag → release on input port → creates edge), manual connection via selection, Delete/Backspace removal, Ctrl+S save, Ctrl+Z undo (50-state stack), Ctrl+wheel zoom (0.25x–3x), Shift+wheel horizontal pan, right-button drag pan, empty-state overlay. Paints a grid background. All workflow changes emit `workflowChanged()`.
- **NodeGraphicsItem** — QGraphicsObject drawing a rounded rectangle with: title text, type label, truncated nodeId, left/right port circles (only if ports exist), colored left border strip indicating execution state. Six visual states: Idle (gray), Pending (light gray), Queued (purple), Running (blue + glow halo), Succeeded (green), Failed (red). Colors come from ThemeManager tokens. Pos changes emit `nodeMoved()`; selection emits `nodeSelected()`; double-click emits `nodeDoubleClicked()`.
- **EdgeGraphicsItem** — QGraphicsPathItem drawing a smooth cubic-bezier path between two NodeGraphicsItems. Path is recalculated when nodes move. Supports parallel offset for multiple edges between same nodes. Hover and selection state change pen width and color. Arrow head drawn as filled triangle polygon.
- **EdgeRouter** — obstacle-aware orthogonal edge routing. First tries a simple 3-segment orthogonal path (source→stub→center→stub→target). If it intersects any obstacle node rect, falls back to A* search on a candidate grid built from obstacle boundaries + lane offsets. Final fallback: direct bezier curve.
- **EdgePathBuilder** — converts polyline waypoints to a smooth QPainterPath with rounded corners (quadTo arcs at each turn, clamped to half the adjacent segment length).
- **ArrowHeadBuilder** — builds a triangle polygon at the target port tip, oriented along the last path segment direction.

#### Editor (`src/ui/editor/`)

- **PythonCodeEditor** — QPlainTextEdit subclass with: Consolas 11pt monospace font, line number gutter, Python syntax highlighting, auto-indent (detects trailing colon), Tab/Shift+Tab indent/unindent, Ctrl+Space completion popup. Completion scans the document for word tokens and merges with built-in Python keywords + workflow API names (`inputs`, `context`, `outputs`, `artifacts`).
- **PythonSyntaxHighlighter** — QSyntaxHighlighter using regex rules for keywords, numbers, def/class names, single/double-quoted strings, comments. Handles multiline triple-quoted strings via block state (prevents comment/keyword highlighting inside multi-line strings).
- **PythonCompleter** — QCompleter with a QStringListModel. Base words: Python builtins + workflow context names. Refreshed on text change by scanning the document for identifier tokens.
- **LineNumberArea** — thin QWidget to the left of the editor. Painting is delegated to PythonCodeEditor for accurate line-height matching.
- **PythonCodeTemplates** — static methods returning Python code strings for 7 data transfer modes (starter: EmptyOutput/DataOutput/FileOutput; function: DataToData/DataToFile/FileToData/FileToFile). Also generates Agent node code with OpenAI-compatible API call using urllib. Templates include `def run(inputs, context) → dict` function signatures returning `{"outputs": {...}, "artifacts": [...]}`.
- **PythonNodeEditorDialog** — modal/modeless dialog for editing node name, description, and code. For Agent nodes, adds structured fields: URL, Model name, API key (password field), Background prompt, Task prompt. The "Load Agent Template" button regenerates Python code from these structured fields. Ctrl+S shortcut. Unsaved changes prompt on close.

#### Inspector (`src/ui/inspector/`)

- **NodeInspector** — right-side panel with two tabs: "Python" (read-only code preview) and "Agent" (read-only structured fields: title, description, timeout, template, URL, model, API key, prompts). Agent tab is disabled for non-agent nodes. Updates when canvas selection changes.

#### Output Panel (`src/ui/output/`)

- **OutputPanel** — bottom panel with 9 tabs: Run Timeline (timestamped event log), Node Runs (per-node status/debug/output/error table), Thread Trace (thread-level execution phases), Logs (application messages), Debug Output (captured Python `print()`), stderr, Traceback, Output JSON (pretty-printed), Artifacts (file path, type, size, text preview of first 8 lines). Data flows in via `OutputPanel::record*()` methods called from MainWindow's event bus connections.

#### Theme (`src/ui/theme/`)

- **ThemeManager** — singleton managing Light/Dark themes. `applyTheme()` loads a QSS stylesheet from `:/styles/light.qss` or `:/styles/dark.qss` and applies it via `qApp->setStyleSheet()`. `color(token)` returns a QColor by name from the current theme's color map (hardcoded, ~100 tokens per theme). Both themes define colors for: surfaces, text, borders, accent/semantic colors, node states (fill/border/strip × 6 states), edges, editor, syntax highlighting, canvas grid, overlays. Missing tokens fall back to the other theme, then magenta for visibility.

#### Widgets (`src/ui/widgets/`)

- **CommandBar** — horizontal bar above the canvas with icon buttons (workspace, python, workflow, save, template, link, import, run, stop, theme) and breadcrumb label (workspace / workflow).
- **IconSquareButton** — 38×38px QPushButton rendering tinted SVG icons. Four roles: Primary (blue bg, white icon), Secondary (white bg, dark icon), Danger (red bg, white icon), Ghost (transparent bg). Re-tints on theme change.

#### Workspace Explorer (`src/ui/workspace/`)

- **WorkspaceExplorer** — left panel with QTreeWidget showing three categories: Workflows (with workflowIds stored as UserRole data for activation), Node Templates, Runs. Double-clicking a workflow emits `workflowActivated(workflowId)`. Empty states show placeholder text.

### Application Bootstrap (`app/`)

- **main.cpp** — creates QApplication, AppContext, MainWindow (1440×900), shows it.
- **AppContext** — composition root. In `initialize()`: creates WorkerRegistry, PythonNodeWorker (shared ptr), registers PythonNodeWorker for types "function", "starter", "agent". Then creates ExecutionEngine, WorkspaceService, WorkflowService, NodeTemplateService, RunService. Owns all through unique_ptr. `setPythonExecutable()` delegates to PythonNodeWorker.
- **MainWindow** — QMainWindow composing all UI panels. Builds menu bar (File/Workflow/View). Builds layout: horizontal splitter (workspace explorer | canvas | inspector) → vertical splitter (+ output panel). Wires up ~70 signal/slot connections: canvas events → app services, event bus → canvas status + output panel, theme changes → canvas refresh. Manages workspace/workflow lifecycle with empty-state overlay. Orchestrates workflow execution: checks Python interpreter, clears output, calls `executionEngine.runWorkflowAsync()`, then saves run record on completion.

### Python Worker Process (`python/`)

Three standalone scripts, invoked via QProcess:

- **python_worker.py** — entry point. Calls `read_request(stdin)` → `execute_user_code(request)` → `write_response(response)`. Catches protocol-level errors and returns valid JSON even on failure.
- **worker_protocol.py** — `read_request()`: forces UTF-8 stdio encoding, reads all stdin, parses JSON. `write_response()`: writes JSON with `ensure_ascii=False` (supports non-ASCII paths/errors).
- **sandbox_runtime.py** — `execute_user_code()`: compiles user code with `compile()` + `exec()` in an isolated namespace dict, captures stdout/stderr via `contextlib.redirect_stdout/redirect_stderr`, calls the entry function `run(inputs, context)`, validates the return type (must be dict with `outputs` dict and `artifacts` list). Returns standardized response JSON. Any exception is caught and returned as `success: false`.

### Python Protocol (stdin/stdout JSON)

Request (C++ → Python):
```json
{"request_id": "uuid", "code": "def run(...):...", "entry": "run", "inputs": {...}, "context": {"run_id": "...", "node_id": "...", "workspace_path": "...", "run_path": "...", "artifact_path": "..."}}
```

Response (Python → C++):
```json
{"request_id": "uuid", "success": true, "outputs": {...}, "artifacts": [...], "stdout": "...", "stderr": "...", "error": null, "traceback": ""}
```

User `print()` output is captured into `stdout` — it must NOT go to process stdout directly because stdout is the protocol channel.

## Namespaces

All C++ code under `vws` with sub-namespaces: `vws::domain`, `vws::infrastructure`, `vws::workers`, `vws::execution`, `vws::application`, `vws::ui`.

## Test Structure

- `tests/unit/` — unit tests (no Widgets needed)
- `tests/integration/` — integration tests (`test_python_worker` spawns a real QProcess)
- `tests/fixtures/` — JSON workflow files for serialization/scheduler tests
- CMake uses `vws_add_test(name sources...)` for Core-only tests; widget-dependent tests use `qt_add_executable` directly

## Theme System

Two built-in themes (Light/Dark) managed by `ThemeManager` singleton. QSS stylesheets loaded from Qt resources (`:/styles/light.qss`, `:/styles/dark.qss`). Color tokens are hardcoded in `ThemeManager::buildColorMaps()` (~100 tokens per theme). All visual elements reference theme tokens: `ThemeManager::instance()->color("token-name")`. Custom widgets (NodeGraphicsItem, EdgeGraphicsItem, PythonCodeEditor, IconSquareButton) react to `ThemeManager::themeChanged` signal to re-paint without a full stylesheet reload.

## Agent Nodes

Agent nodes are structurally identical to Function nodes — both execute through PythonNodeWorker. The difference is in the UI: Agent nodes expose structured fields (URL, model name, API key, background prompt, task prompt) in the editor dialog and inspector. The "Load Agent Template" button in the editor dialog regenerates Python code from these structured fields using `PythonCodeTemplates::agentCode()`, which generates an OpenAI-compatible HTTP call via `urllib.request`.
