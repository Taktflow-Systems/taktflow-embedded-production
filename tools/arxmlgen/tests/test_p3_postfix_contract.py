"""Post-fix internal contracts for atomic output and nested diagnostics."""

from __future__ import annotations

from types import SimpleNamespace

import pytest

from tools.arxml.dbc2arxml import Dbc2Arxml
from pipeline_diagnostics import (
    DiagnosticBag,
    DiagnosticSeverity,
    PipelineDiagnostic,
    PipelineDiagnosticError,
)


def test_atomic_replace_failure_preserves_bytes_and_removes_temporary_file(
    tmp_path, monkeypatch
):
    output = tmp_path / "TaktflowSystem.arxml"
    original = b"known-good\r\n\x00payload"
    output.write_bytes(original)

    converter = Dbc2Arxml.__new__(Dbc2Arxml)
    converter.diagnostics = DiagnosticBag()
    converter.source_path = "input.dbc"
    converter.am = SimpleNamespace(
        model=SimpleNamespace(
            serialize_files=lambda: {"generated.arxml": "replacement"}
        )
    )

    def fail_replace(_source, _target):
        raise OSError("simulated atomic replacement failure")

    monkeypatch.setattr(
        "tools.arxml.dbc2arxml.validate_arxml_strict", lambda _path: None
    )
    monkeypatch.setattr("tools.arxml.dbc2arxml.os.replace", fail_replace)

    with pytest.raises(PipelineDiagnosticError) as exc_info:
        converter.write(tmp_path)

    assert [item.code for item in exc_info.value.diagnostics] == ["IO001"]
    assert output.read_bytes() == original
    assert not list(tmp_path.glob(".TaktflowSystem.arxml.*.tmp"))


@pytest.mark.parametrize("nested_code", ["ARXML015", "ARXML016", "ARXML017"])
def test_nested_swc_diagnostic_code_survives_outer_handler(nested_code):
    converter = Dbc2Arxml.__new__(Dbc2Arxml)
    converter.ecu_model = {
        "ecus": {
            "brake": {
                "runnables": [],
                "swcs": [{"name": "Swc_Brake", "functions": []}],
            }
        }
    }
    converter.am = SimpleNamespace(get_or_create_package=lambda _path: object())
    converter.ecu_tx_signals = {}
    converter.ecu_rx_signals = {}
    nested = PipelineDiagnostic(
        code=nested_code,
        severity=DiagnosticSeverity.ERROR,
        source="nested SWC operation",
        message="forced failure",
    )

    def fail_with_nested_diagnostic(*_args):
        raise PipelineDiagnosticError([nested])

    converter._create_one_swc = fail_with_nested_diagnostic

    with pytest.raises(PipelineDiagnosticError) as exc_info:
        converter._create_swc_types()

    assert exc_info.value.diagnostics == (nested,)
