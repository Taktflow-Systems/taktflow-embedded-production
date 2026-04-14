"""Resolve a symbolic PDU ID (e.g. `CVC_COM_TX_UDS_RSP`) to an integer.

The Dcm_Cfg.c file references PDU IDs by macro name. The actual numeric
value is defined in one of the ECU-specific header files under
`firmware/ecu/<ecu>/include/*.h` or `firmware/ecu/<ecu>/cfg/*.h`. The
chain may go through one alias macro (e.g.
`#define CVC_COM_TX_UDS_RSP CVC_COM_TX_UDS_RESP_CVC`).

This resolver walks at most one level of indirection. If the second hop
also resolves to a symbol rather than an integer, it gives up and emits
a TODO note. No PDU values are hardcoded.
"""

from __future__ import annotations

import re
from pathlib import Path
from typing import Optional


# Match a #define producing either an integer literal or another identifier.
#   #define NAME   42u
#   #define NAME   ANOTHER_NAME
_DEFINE_RE_TEMPLATE = (
    r"^[ \t]*#[ \t]*define[ \t]+{name}[ \t]+"
    r"(?P<value>[A-Za-z_][A-Za-z0-9_]*|0[xX][0-9A-Fa-f]+[uUlL]*|\d+[uUlL]*)"
    r"[ \t]*(?:/\*.*?\*/|//.*)?[ \t]*$"
)


def _strip_int_literal(value: str) -> Optional[int]:
    s = value.strip().rstrip("uUlL").rstrip("uUlL")
    try:
        if s.lower().startswith("0x"):
            return int(s, 16)
        return int(s, 10)
    except ValueError:
        return None


def _candidate_header_dirs(repo_root: Path, ecu_name: str) -> list[Path]:
    base = repo_root / "firmware" / "ecu" / ecu_name
    return [base / "include", base / "cfg"]


def _scan_for_define(headers: list[Path], symbol: str) -> tuple[Optional[str], Optional[Path]]:
    """Return (raw value string, header path) for the first matching #define."""
    pattern = re.compile(
        _DEFINE_RE_TEMPLATE.format(name=re.escape(symbol)),
        re.MULTILINE,
    )
    for h in headers:
        try:
            text = h.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        m = pattern.search(text)
        if m:
            return m.group("value"), h
    return None, None


def resolve_symbol(
    repo_root: Path | str,
    ecu_name: str,
    symbol: str,
    *,
    max_indirection: int = 1,
) -> tuple[Optional[int], Optional[str], list[str]]:
    """Resolve a PDU ID symbol to an integer value.

    Args:
        repo_root: absolute path to the firmware repository root.
        ecu_name: ECU short name (used to locate header directories).
        symbol: macro name to resolve.
        max_indirection: maximum number of alias hops to follow after the
            initial lookup. Default 1 (so total chain length up to 2).

    Returns:
        (value, source_file, todos).
        - `value` is an int if fully resolved, else None.
        - `source_file` is the path of the header where the integer was
          ultimately found, or where the chain stopped.
        - `todos` lists any unresolved-symbol notes.
    """
    repo_root = Path(repo_root)
    todos: list[str] = []

    headers = []
    for d in _candidate_header_dirs(repo_root, ecu_name):
        if d.is_dir():
            headers.extend(sorted(d.glob("*.h")))

    if not headers:
        todos.append(
            f"TODO: no header files found under firmware/ecu/{ecu_name}/include "
            f"or firmware/ecu/{ecu_name}/cfg to resolve `{symbol}`"
        )
        return None, None, todos

    current_symbol = symbol
    visited: set[str] = set()
    last_source: Optional[Path] = None

    for hop in range(max_indirection + 1):
        if current_symbol in visited:
            todos.append(
                f"TODO: cycle detected resolving `{symbol}` at `{current_symbol}`"
            )
            return None, str(last_source) if last_source else None, todos
        visited.add(current_symbol)

        raw, src = _scan_for_define(headers, current_symbol)
        if raw is None:
            todos.append(
                f"TODO: macro `{current_symbol}` not defined in any header "
                f"under firmware/ecu/{ecu_name}/{{include,cfg}}"
            )
            return None, str(last_source) if last_source else None, todos

        last_source = src
        int_value = _strip_int_literal(raw)
        if int_value is not None:
            return int_value, str(src) if src else None, todos

        # raw is another identifier — follow it (unless we're out of hops)
        if hop == max_indirection:
            todos.append(
                f"TODO: `{symbol}` chain not fully resolved after "
                f"{max_indirection + 1} hops; stopped at `{current_symbol}` "
                f"-> `{raw}` in {src}"
            )
            return None, str(src) if src else None, todos
        current_symbol = raw

    todos.append(f"TODO: unexpected fall-through resolving `{symbol}`")
    return None, str(last_source) if last_source else None, todos
