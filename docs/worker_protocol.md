# Worker Protocol

Python-backed nodes run outside the Qt process through `python/python_worker.py`. C++ sends one JSON request through stdin and receives one JSON response through stdout.

This keeps user code exceptions, stdout noise, blocking imports, and Python crashes out of the desktop process.

## Request

```json
{
  "request_id": "uuid",
  "code": "def run(inputs, context): ...",
  "entry": "run",
  "inputs": {},
  "context": {
    "run_id": "run id",
    "node_id": "node id",
    "workspace_path": "...",
    "run_path": "...",
    "artifact_path": "..."
  }
}
```

## Response

```json
{
  "request_id": "uuid",
  "success": true,
  "outputs": {},
  "artifacts": [],
  "stdout": "",
  "stderr": "",
  "error": null,
  "traceback": ""
}
```

User `print()` output is captured into `stdout`. It must not be written directly to process stdout because stdout is the protocol channel.

## Node Types

Starter, Function, and Agent nodes all use this worker protocol. The differences are expressed in node type, ports, editor behavior, and generated Python templates. There is no separate C++ Agent HTTP worker in the current architecture.
