"""Strict ARXML validation contract shared by conversion and CI."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest


REPO_ROOT = Path(__file__).resolve().parents[3]
TOOLS_DIR = REPO_ROOT / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from arxml.dbc2arxml import Dbc2Arxml  # noqa: E402
from pipeline_diagnostics import DiagnosticBag, PipelineDiagnosticError  # noqa: E402
import arxml_validation as validation  # noqa: E402


SOURCE_ARXML = REPO_ROOT / "arxml" / "TaktflowSystem.arxml"


def test_strict_validator_accepts_inhouse_arxml():
    result = validation.validate_arxml_strict(SOURCE_ARXML)

    assert result.warning_count == 0
    assert result.reference_error_count == 0
    assert result.identifiable_count > 2000


def test_corrupted_enum_has_pointed_strict_failure(tmp_path):
    corrupted = tmp_path / "corrupted.arxml"
    content = SOURCE_ARXML.read_text(encoding="utf-8").replace(
        "<CAN-ADDRESSING-MODE>STANDARD</CAN-ADDRESSING-MODE>",
        "<CAN-ADDRESSING-MODE>BOGUS</CAN-ADDRESSING-MODE>",
        1,
    )
    corrupted.write_text(content, encoding="utf-8")

    with pytest.raises(validation.StrictArxmlValidationError) as exc_info:
        validation.validate_arxml_strict(corrupted)

    assert exc_info.value.code == "ARXML200"
    assert exc_info.value.path == "corrupted.arxml"
    assert exc_info.value.line == 1046
    assert "BOGUS" in exc_info.value.message


def test_unresolved_reference_fails_strict_gate(tmp_path):
    corrupted = tmp_path / "broken-reference.arxml"
    content = SOURCE_ARXML.read_text(encoding="utf-8")
    content = re.sub(
        r"(<I-SIGNAL-REF(?:\s[^>]*)?>)[^<]+",
        r"\1/Taktflow/Communication/Signals/DOES_NOT_EXIST",
        content,
        count=1,
    )
    corrupted.write_text(content, encoding="utf-8")

    with pytest.raises(validation.StrictArxmlValidationError) as exc_info:
        validation.validate_arxml_strict(corrupted)

    assert exc_info.value.code == "ARXML202"
    assert exc_info.value.reference_error_count == 1


def test_strict_warnings_are_rejected(monkeypatch, tmp_path):
    arxml = tmp_path / "warning.arxml"
    arxml.write_text("<AUTOSAR/>", encoding="utf-8")

    class WarningModel:
        identifiable_elements = []

        def load_file(self, _path, strict=False):
            assert strict is True
            return None, ["schema warning"]

        def check_references(self):
            return []

    monkeypatch.setattr(validation.ad, "AutosarModel", WarningModel)

    with pytest.raises(validation.StrictArxmlValidationError) as exc_info:
        validation.validate_arxml_strict(arxml)

    assert exc_info.value.code == "ARXML201"
    assert exc_info.value.warning_count == 1


def test_converter_validates_before_atomic_replace(tmp_path, monkeypatch):
    output = tmp_path / "TaktflowSystem.arxml"
    output.write_text("known-good", encoding="utf-8")
    converter = Dbc2Arxml.__new__(Dbc2Arxml)
    converter.diagnostics = DiagnosticBag()
    converter.source_path = "input.dbc"
    converter.am = SimpleNamespace(
        model=SimpleNamespace(
            serialize_files=lambda: {"generated.arxml": "candidate"}
        )
    )

    def reject_candidate(path):
        assert Path(path).name.startswith(".TaktflowSystem.arxml.")
        raise validation.StrictArxmlValidationError(
            code="ARXML200",
            path=Path(path).name,
            message="candidate failed strict load",
            line=1,
            column=1,
        )

    monkeypatch.setattr("arxml.dbc2arxml.validate_arxml_strict", reject_candidate)

    with pytest.raises(PipelineDiagnosticError) as exc_info:
        converter.write(tmp_path)

    assert exc_info.value.diagnostics[0].code == "ARXML200"
    assert output.read_text(encoding="utf-8") == "known-good"
    assert not list(tmp_path.glob(".*.tmp"))
