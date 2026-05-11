"""Python Worker 的 JSON 协议工具。

这个文件只处理协议输入/输出，不执行用户代码。

协议约定：
- Qt/C++ 主程序通过 stdin 发送一个 JSON 对象。
- Python Worker 通过 stdout 返回一个 JSON 对象。
- 用户代码自己的 print/stdout 会被 sandbox_runtime 捕获，不能直接污染协议 stdout。
"""

from __future__ import annotations

import json
import sys
from typing import Any, TextIO


def _ensure_utf8_stdio() -> None:
    """Force UTF-8 for the JSON protocol on Windows.

    Qt writes UTF-8 bytes to QProcess stdin and reads UTF-8 bytes from stdout.
    Python's default text encoding on Windows can follow the system code page,
    which corrupts non-ASCII workspace paths unless we pin the protocol streams.
    """

    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8")
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8")


def read_request(stream: TextIO) -> dict[str, Any]:
    """从 stdin 读取完整 JSON 请求。"""

    _ensure_utf8_stdio()
    raw_text = stream.read()
    if not raw_text.strip():
        raise ValueError("empty worker request")
    request = json.loads(raw_text)
    if not isinstance(request, dict):
        raise TypeError("worker request must be a JSON object")
    return request


def write_response(response: dict[str, Any], stream: TextIO | None = None) -> None:
    """把响应写回 stdout。

    ensure_ascii=False 是为了后续错误信息、stdout 中出现中文时保持可读。
    """

    target = stream if stream is not None else sys.stdout
    target.write(json.dumps(response, ensure_ascii=False))
    target.write("\n")
    target.flush()
