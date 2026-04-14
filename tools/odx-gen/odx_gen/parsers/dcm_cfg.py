"""Parser for `firmware/ecu/<ecu>/cfg/Dcm_Cfg_<Ecu>.c`.

Extracts:
  * the DID table rows (DID id, callback, length, comment-as-name)
  * the aggregate `Dcm_ConfigType` struct (TxPduId symbol, S3 timeout)

No ECU data is hardcoded. Every value comes from regex-matching the file
contents at parse time. Anything the regexes cannot find is left as None
and an explanatory `TODO:` note is appended to `unresolved_todos`.
"""

from __future__ import annotations

import re
from pathlib import Path

from ..model import DidEntry, EcuDiagnosticModel


# --- regex patterns ----------------------------------------------------------

# Match a DID table row of the form
#   { 0xNNNNu, SomeReadCallback, Nu },   /* optional comment */
# Trailing comment is optional. Length suffix u/U is optional.
_DID_ROW_RE = re.compile(
    r"\{\s*"
    r"(?P<did>0[xX][0-9A-Fa-f]+)[uU]?\s*,\s*"
    r"(?P<cb>[A-Za-z_][A-Za-z0-9_]*)\s*,\s*"
    r"(?P<len>\d+)[uU]?\s*"
    r"\}\s*,?"
    r"(?:\s*/\*\s*(?P<cmt>.*?)\s*\*/)?",
    re.DOTALL,
)

# Match the start of the DID table: `... <ecu>_did_table[] = {`
_DID_TABLE_HEAD_RE = re.compile(
    r"(?:static\s+)?const\s+Dcm_DidTableType\s+"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\[\s*\]\s*=\s*\{",
)

# Match the aggregate config struct. Capture the body so we can scan
# for individual `.Field = value` initializers.
_CFG_STRUCT_RE = re.compile(
    r"const\s+Dcm_ConfigType\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*\{"
    r"(?P<body>.*?)\}\s*;",
    re.DOTALL,
)

# Match `.Field = value,` initializers within a struct body.
_FIELD_INIT_RE = re.compile(
    r"\.\s*(?P<field>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*"
    r"(?P<value>[^,}]+?)\s*[,}]",
)


# --- helpers -----------------------------------------------------------------

def _line_number(text: str, offset: int) -> int:
    """Return 1-based line number for a character offset in text."""
    return text.count("\n", 0, offset) + 1


def _find_block_end(text: str, open_brace_offset: int) -> int:
    """Return the offset of the matching '}' for the '{' at open_brace_offset."""
    depth = 0
    i = open_brace_offset
    n = len(text)
    while i < n:
        c = text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def _strip_int_literal(value: str) -> int | None:
    """Parse a C integer literal (decimal or `0x..`, optional u/l suffix)."""
    s = value.strip().rstrip("uUlL")
    s = s.rstrip("uUlL")  # tolerate `ul`, `ull`
    try:
        if s.lower().startswith("0x"):
            return int(s, 16)
        return int(s, 10)
    except ValueError:
        return None


# --- public API --------------------------------------------------------------

def parse_dcm_cfg(path: str | Path, ecu_name: str) -> EcuDiagnosticModel:
    """Parse a Dcm_Cfg_<Ecu>.c file into an EcuDiagnosticModel.

    Args:
        path: Absolute path to Dcm_Cfg_<Ecu>.c.
        ecu_name: ECU short name. Used for the model field
            and for matching directory layout, but NOT for hardcoded values.

    Returns:
        EcuDiagnosticModel populated with DID entries and config-struct fields.
        Missing fields produce TODO entries in `unresolved_todos`.
    """
    p = Path(path)
    if not p.is_file():
        raise FileNotFoundError(f"Dcm_Cfg file not found: {p}")

    text = p.read_text(encoding="utf-8", errors="replace")
    src = str(p)
    model = EcuDiagnosticModel(ecu_name=ecu_name)

    # --- DID table --------------------------------------------------------
    table_match = _DID_TABLE_HEAD_RE.search(text)
    if not table_match:
        model.unresolved_todos.append(
            f"TODO: no `const Dcm_DidTableType <name>[]` array found in {src}"
        )
    else:
        # Locate the opening brace of the array initializer
        brace_open = text.find("{", table_match.end() - 1)
        brace_close = _find_block_end(text, brace_open)
        if brace_close < 0:
            model.unresolved_todos.append(
                f"TODO: unterminated DID table block in {src}"
            )
        else:
            body = text[brace_open + 1: brace_close]
            body_offset = brace_open + 1
            for row in _DID_ROW_RE.finditer(body):
                did_val = int(row.group("did"), 16)
                callback = row.group("cb")
                length = int(row.group("len"))
                comment = (row.group("cmt") or "").strip()
                line_no = _line_number(text, body_offset + row.start())
                model.dids.append(
                    DidEntry(
                        did_id=did_val,
                        name=comment,
                        callback=callback,
                        length_bytes=length,
                        source_file=src,
                        source_line=line_no,
                    )
                )

    if not model.dids:
        model.unresolved_todos.append(
            f"TODO: no DID rows extracted from {src}"
        )

    # --- aggregate Dcm_ConfigType struct ----------------------------------
    cfg = _CFG_STRUCT_RE.search(text)
    if not cfg:
        model.unresolved_todos.append(
            f"TODO: no `Dcm_ConfigType <name> = {{ ... }}` struct found in {src}"
        )
    else:
        body = cfg.group("body")
        # Append a closing brace marker so the trailing field init is matched
        scan_body = body + "}"
        fields: dict[str, str] = {}
        for fm in _FIELD_INIT_RE.finditer(scan_body):
            fields[fm.group("field")] = fm.group("value").strip()

        if "TxPduId" in fields:
            tx = fields["TxPduId"]
            int_value = _strip_int_literal(tx)
            if int_value is not None:
                model.tx_pdu_id = int_value
            else:
                # symbolic — needs the header parser to resolve
                model.tx_pdu_id_symbol = tx
        else:
            model.unresolved_todos.append(
                f"TODO: .TxPduId not found in Dcm_ConfigType struct in {src}"
            )

        if "RxPduId" in fields:
            rx = fields["RxPduId"]
            int_value = _strip_int_literal(rx)
            if int_value is not None:
                model.rx_pdu_id = int_value
            else:
                model.rx_pdu_id_symbol = rx
        else:
            model.unresolved_todos.append(
                f"TODO: .RxPduId not found in Dcm_ConfigType struct in {src}"
            )

        if "S3TimeoutMs" in fields:
            s3 = _strip_int_literal(fields["S3TimeoutMs"])
            if s3 is not None:
                model.s3_timeout_ms = s3
            else:
                model.unresolved_todos.append(
                    f"TODO: .S3TimeoutMs value `{fields['S3TimeoutMs']}` "
                    f"is not an integer literal in {src}"
                )
        else:
            model.unresolved_todos.append(
                f"TODO: .S3TimeoutMs not found in Dcm_ConfigType struct in {src}"
            )

    return model
