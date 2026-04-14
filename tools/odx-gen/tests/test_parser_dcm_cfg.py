"""Tests for the Dcm_Cfg parser.

These tests run against the REAL firmware file and derive their
expectations from the file contents at test time. There are no
hardcoded DID values, callback names, or counts.
"""

from __future__ import annotations

import re
from pathlib import Path

import pytest

from odx_gen.model import EcuDiagnosticModel
from odx_gen.parsers.dcm_cfg import parse_dcm_cfg


REPO_ROOT = Path(__file__).resolve().parents[3]
CVC_DCM_CFG = REPO_ROOT / "firmware" / "ecu" / "cvc" / "cfg" / "Dcm_Cfg_Cvc.c"


def _count_did_rows_in_file(path: Path) -> int:
    """Independently count `{ 0xXXXXu, ... },` rows for cross-checking."""
    text = path.read_text(encoding="utf-8")
    rows = re.findall(r"\{\s*0[xX][0-9A-Fa-f]+[uU]?\s*,\s*[A-Za-z_]", text)
    return len(rows)


def _read_field_from_file(path: Path, field: str) -> str | None:
    """Read a `.Field = value,` initializer directly from the file."""
    text = path.read_text(encoding="utf-8")
    m = re.search(
        rf"\.\s*{re.escape(field)}\s*=\s*([^,}}]+?)\s*[,}}]", text
    )
    return m.group(1).strip() if m else None


@pytest.fixture
def model() -> EcuDiagnosticModel:
    assert CVC_DCM_CFG.is_file(), f"missing source file: {CVC_DCM_CFG}"
    return parse_dcm_cfg(CVC_DCM_CFG, ecu_name="cvc")


def test_returns_model_for_correct_ecu(model: EcuDiagnosticModel) -> None:
    assert model.ecu_name == "cvc"


def test_did_count_matches_file_contents(model: EcuDiagnosticModel) -> None:
    expected = _count_did_rows_in_file(CVC_DCM_CFG)
    assert expected > 0, "test setup: file appears to have no DID rows"
    assert len(model.dids) == expected


def test_dids_have_required_fields(model: EcuDiagnosticModel) -> None:
    assert len(model.dids) >= 1
    for did in model.dids:
        assert did.did_id > 0
        assert did.callback != ""
        assert did.length_bytes > 0
        assert did.source_file.endswith("Dcm_Cfg_Cvc.c")
        assert did.source_line > 0


def test_tx_pdu_id_symbol_extracted(model: EcuDiagnosticModel) -> None:
    file_value = _read_field_from_file(CVC_DCM_CFG, "TxPduId")
    assert file_value, "TxPduId not present in file — test setup invalid"
    # The parser will either resolve it to an integer or capture the symbol
    if model.tx_pdu_id is not None:
        # integer literal in source — unlikely for CVC right now
        assert isinstance(model.tx_pdu_id, int)
    else:
        assert model.tx_pdu_id_symbol == file_value


def test_s3_timeout_is_positive_integer(model: EcuDiagnosticModel) -> None:
    file_value = _read_field_from_file(CVC_DCM_CFG, "S3TimeoutMs")
    assert file_value, "S3TimeoutMs not present in file — test setup invalid"
    assert model.s3_timeout_ms is not None
    assert model.s3_timeout_ms > 0
    # Cross-check parser against an independent re-read of the file
    expected_int = int(file_value.rstrip("uUlL"))
    assert model.s3_timeout_ms == expected_int


def test_no_hardcoded_data_leaks_through_parser() -> None:
    """Sanity: the parser module text does not contain literal CVC values."""
    parser_src = (
        Path(__file__).resolve().parents[1]
        / "odx_gen" / "parsers" / "dcm_cfg.py"
    ).read_text(encoding="utf-8")
    # The parser must not embed any specific DID, callback name, or PDU symbol.
    forbidden_substrings = [
        "0xF190", "0xF191", "0xF195", "0xF010",
        "Dcm_ReadDid_EcuId", "CVC_COM_TX_UDS_RSP", "5000",
    ]
    for needle in forbidden_substrings:
        assert needle not in parser_src, (
            f"hardcoded ECU data leaked into parser: {needle}"
        )
