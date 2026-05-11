"""Entry point for Python-backed workflow nodes.

C++ starts this script through QProcess, writes one JSON request to stdin, and
expects one JSON response on stdout. User ``print()`` calls are captured by the
runtime and returned in the response stdout field, so protocol stdout stays
machine-readable.
"""

from __future__ import annotations

import sys
import traceback

from sandbox_runtime import execute_user_code
from worker_protocol import read_request, write_response


def main() -> int:
    try:
        request = read_request(sys.stdin)
        response = execute_user_code(request)
    except Exception as exc:  # noqa: BLE001 - protocol errors must still return JSON
        response = {
            "request_id": "",
            "success": False,
            "outputs": {},
            "artifacts": [],
            "stdout": "",
            "stderr": "",
            "error": str(exc),
            "traceback": traceback.format_exc(),
        }

    write_response(response)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
