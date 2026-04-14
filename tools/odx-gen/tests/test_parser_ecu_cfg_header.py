"""Tests for the ECU header symbolic-PDU-ID resolver.

Re-derives the expected integer from the source file at test time —
no values are hardcoded.
"""

from __future__ import annotations

import re
from pathlib import Path

from odx_gen.parsers.dcm_cfg import parse_dcm_cfg
from odx_gen.parsers.ecu_cfg_header import resolve_symbol


REPO_ROOT = Path(__file__).resolve().parents[3]
CVC_DCM_CFG = REPO_ROOT / "firmware" / "ecu" / "cvc" / "cfg" / "Dcm_Cfg_Cvc.c"


def _final_int_for_symbol(repo_root: Path, ecu: str, symbol: str) -> int | None:
    """Independently follow #define chains in *.h files under the ECU."""
    base = repo_root / "firmware" / "ecu" / ecu
    headers: list[Path] = []
    for d in (base / "include", base / "cfg"):
        if d.is_dir():
            headers.extend(d.glob("*.h"))

    current = symbol
    for _ in range(4):
        found = None
        for h in headers:
            text = h.read_text(encoding="utf-8", errors="replace")
            m = re.search(
                rf"^[ \t]*#[ \t]*define[ \t]+{re.escape(current)}[ \t]+([A-Za-z_0-9xX]+)",
                text,
                re.MULTILINE,
            )
            if m:
                found = m.group(1)
                break
        if found is None:
            return None
        s = found.rstrip("uUlL")
        try:
            if s.lower().startswith("0x"):
                return int(s, 16)
            return int(s, 10)
        except ValueError:
            current = found
    return None


def test_resolves_cvc_tx_pdu_symbol_chain() -> None:
    model = parse_dcm_cfg(CVC_DCM_CFG, ecu_name="cvc")
    symbol = model.tx_pdu_id_symbol
    assert symbol, "Dcm_Cfg parser did not extract a TxPduId symbol"

    value, src, todos = resolve_symbol(REPO_ROOT, "cvc", symbol)
    expected = _final_int_for_symbol(REPO_ROOT, "cvc", symbol)
    assert expected is not None, "test setup: header chain produced no int"
    assert value == expected, f"resolver got {value}, file says {expected}"
    assert src is not None
    assert todos == []


def test_unresolved_symbol_emits_todo() -> None:
    value, src, todos = resolve_symbol(
        REPO_ROOT, "cvc", "DOES_NOT_EXIST_ANYWHERE_XYZ"
    )
    assert value is None
    assert any("not defined" in t for t in todos)
