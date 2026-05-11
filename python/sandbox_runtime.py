"""Minimal runtime for executing user Python node code.

The desktop app isolates user code in a separate Python process. This protects
the Qt process from Python exceptions, crashes, and stdout writes, but it is not
a security sandbox for hostile code.
"""

from __future__ import annotations

import contextlib
import io
import traceback
from typing import Any


def execute_user_code(request: dict[str, Any]) -> dict[str, Any]:
    """Run user code and normalize the result into the worker response schema."""

    request_id = str(request.get("request_id", ""))
    code = str(request.get("code", ""))
    entry = str(request.get("entry", "run"))
    inputs = request.get("inputs", {})
    context = request.get("context", {})

    stdout_buffer = io.StringIO()
    stderr_buffer = io.StringIO()

    try:
        # Keep helper functions and the entry function in the same namespace.
        namespace: dict[str, Any] = {
            "__name__": "__visual_workflow_user_node__",
        }

        with contextlib.redirect_stdout(stdout_buffer), contextlib.redirect_stderr(stderr_buffer):
            compiled = compile(code, "<visual_workflow_user_node>", "exec")
            exec(compiled, namespace)

            entry_func = namespace.get(entry)
            if not callable(entry_func):
                raise TypeError(f"entry function '{entry}' is not callable or not defined")

            user_result = entry_func(inputs if isinstance(inputs, dict) else {}, context if isinstance(context, dict) else {})

        if user_result is None:
            user_result = {}
        if not isinstance(user_result, dict):
            raise TypeError("entry function must return a dict")

        outputs = user_result.get("outputs", {})
        artifacts = user_result.get("artifacts", [])
        if not isinstance(outputs, dict):
            raise TypeError("result['outputs'] must be a dict")
        if not isinstance(artifacts, list):
            raise TypeError("result['artifacts'] must be a list")

        return {
            "request_id": request_id,
            "success": True,
            "outputs": outputs,
            "artifacts": artifacts,
            "stdout": stdout_buffer.getvalue(),
            "stderr": stderr_buffer.getvalue(),
            "error": None,
            "traceback": "",
        }
    except Exception as exc:  # noqa: BLE001 - every user-code failure becomes a protocol error
        return {
            "request_id": request_id,
            "success": False,
            "outputs": {},
            "artifacts": [],
            "stdout": stdout_buffer.getvalue(),
            "stderr": stderr_buffer.getvalue(),
            "error": str(exc),
            "traceback": traceback.format_exc(),
        }
