"""Failure-contract tests for the DBC-to-ARXML pipeline."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest


TOOLS_DIR = Path(__file__).resolve().parents[2]
REPO_ROOT = TOOLS_DIR.parent
sys.path.insert(0, str(TOOLS_DIR))

from pipeline_diagnostics import (  # noqa: E402
    DiagnosticBag,
    DiagnosticSeverity,
    PipelineDiagnostic,
    PipelineDiagnosticError,
)
from arxml.dbc2arxml import Dbc2Arxml, validate_dbc_input  # noqa: E402
from arxml.swc_extractor import parse_defines  # noqa: E402
from arxmlgen.config import ProjectConfig  # noqa: E402
from arxmlgen.model import Ecu  # noqa: E402
from arxmlgen.reader import ArxmlReader, ArxmlReadError  # noqa: E402


def _database(*message_names: str, attribute_definitions=None):
    messages = [
        SimpleNamespace(name=name, signals=[], dbc=SimpleNamespace(attributes={}))
        for name in message_names
    ]
    return SimpleNamespace(
        messages=messages,
        dbc=SimpleNamespace(attribute_definitions=attribute_definitions or {}),
    )


def test_diagnostic_rendering_is_stable():
    diagnostic = PipelineDiagnostic(
        code="DBC003",
        severity=DiagnosticSeverity.ERROR,
        source="DBC message 'BrakeCmd' attribute 'GenMsgSendType'",
        message="unsupported value 'sporadic'",
    )

    assert str(diagnostic) == (
        "[DBC003] error DBC message 'BrakeCmd' attribute "
        "'GenMsgSendType': unsupported value 'sporadic'"
    )


def test_diagnostic_renders_portable_coordinates():
    diagnostic = PipelineDiagnostic(
        code="ARXML102",
        severity=DiagnosticSeverity.ERROR,
        source="ARXML reference '/Communication/Missing'",
        message="reference cannot be resolved",
        path="fixtures/broken.arxml",
        line=42,
        column=17,
    )

    assert str(diagnostic).startswith(
        "[ARXML102] error fixtures/broken.arxml:42:17 ARXML reference"
    )


def test_normalized_message_name_collision_is_an_error():
    bag = DiagnosticBag()

    with pytest.raises(PipelineDiagnosticError) as exc_info:
        validate_dbc_input(_database("Brake-Cmd", "Brake Cmd"), bag)

    assert exc_info.value.diagnostics[0].code == "DBC002"
    assert exc_info.value.diagnostics[0].source == "DBC messages 'Brake Cmd', 'Brake-Cmd'"


def test_unknown_gen_convention_is_a_typed_warning():
    bag = DiagnosticBag()
    database = _database(
        "BrakeCmd",
        attribute_definitions={"GenMsgVendorMagic": object()},
    )

    validate_dbc_input(database, bag)

    assert [(item.code, item.severity.value) for item in bag.items] == [
        ("DBC004", "warning")
    ]


def test_multiplexing_is_a_typed_unsupported_feature():
    bag = DiagnosticBag()
    signal = SimpleNamespace(
        name="Selector", is_multiplexer=True, multiplexer_ids=None
    )
    message = SimpleNamespace(
        name="MuxMessage",
        signals=[signal],
        dbc=SimpleNamespace(attributes={}),
    )
    database = SimpleNamespace(
        messages=[message],
        dbc=SimpleNamespace(attribute_definitions={}),
    )

    with pytest.raises(PipelineDiagnosticError) as exc_info:
        validate_dbc_input(database, bag)

    assert exc_info.value.diagnostics[0].code == "DBC007"


def test_unsupported_send_type_emits_no_arxml(tmp_path):
    dbc_path = tmp_path / "unsupported-send-type.dbc"
    output_dir = tmp_path / "out"
    dbc_path.write_text(
        """VERSION \"\"
NS_ :
BS_:
BU_: ECU_A ECU_B
BO_ 1 BrakeCmd: 8 ECU_A
 SG_ BrakeRequest : 0|8@1+ (1,0) [0|255] \"\" ECU_B
BA_DEF_ BO_ \"GenMsgSendType\" STRING;
BA_ \"GenMsgSendType\" BO_ 1 \"sporadic\";
""",
        encoding="utf-8",
    )

    completed = subprocess.run(
        [
            sys.executable,
            str(TOOLS_DIR / "arxml" / "dbc2arxml.py"),
            str(dbc_path),
            str(output_dir),
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert completed.returncode == 2
    assert (
        "[DBC003] error unsupported-send-type.dbc:7:13 "
        "DBC message 'BrakeCmd' attribute 'GenMsgSendType'"
    ) in (
        completed.stdout + completed.stderr
    )
    assert not (output_dir / "TaktflowSystem.arxml").exists()


def test_malformed_cycle_attribute_emits_no_arxml(tmp_path):
    dbc_path = tmp_path / "malformed-cycle.dbc"
    output_dir = tmp_path / "out"
    dbc_path.write_text(
        """VERSION \"\"
NS_ :
BS_:
BU_: ECU_A ECU_B
BO_ 1 BrakeCmd: 8 ECU_A
 SG_ BrakeRequest : 0|8@1+ (1,0) [0|255] \"\" ECU_B
BA_DEF_ BO_ \"GenMsgCycleTime\" STRING;
BA_ \"GenMsgCycleTime\" BO_ 1 \"fast\";
""",
        encoding="utf-8",
    )

    completed = subprocess.run(
        [
            sys.executable,
            str(TOOLS_DIR / "arxml" / "dbc2arxml.py"),
            str(dbc_path),
            str(output_dir),
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert completed.returncode == 2
    assert (
        "[DBC001] error malformed-cycle.dbc:7:14 "
        "DBC message 'BrakeCmd' attribute 'GenMsgCycleTime'"
    ) in (
        completed.stdout + completed.stderr
    )
    assert not (output_dir / "TaktflowSystem.arxml").exists()


def test_reader_rejects_invalid_signal_start_position():
    reader = ArxmlReader(ProjectConfig())
    mapping = SimpleNamespace(
        sub_elements=[
            SimpleNamespace(element_name="SHORT-NAME", character_data="BrakeRequest"),
            SimpleNamespace(element_name="START-POSITION", character_data="first"),
        ]
    )

    with pytest.raises(ArxmlReadError) as exc_info:
        reader._parse_signal_mapping(mapping)

    assert exc_info.value.diagnostics[0].code == "ARXML105"


def test_reader_rejects_foreign_swc_prefix():
    reader = ArxmlReader(ProjectConfig())
    foreign_swc = SimpleNamespace(
        element_name="APPLICATION-SW-COMPONENT-TYPE",
        item_name="FOREIGN_Swc_Brake",
        sub_elements=[],
    )
    reader.model = SimpleNamespace(identifiable_elements=[("/Foreign", foreign_swc)])

    with pytest.raises(ArxmlReadError) as exc_info:
        reader._extract_swcs({"brake": Ecu(name="brake", prefix="BRK")})

    assert exc_info.value.diagnostics[0].code == "ARXML106"


def test_reader_rejects_broken_signal_reference():
    class BrokenReference:
        element_name = "I-SIGNAL-REF"
        is_reference = True

        @property
        def reference_target(self):
            raise RuntimeError("unresolved")

    reader = ArxmlReader(ProjectConfig())
    mapping = SimpleNamespace(
        sub_elements=[
            SimpleNamespace(element_name="SHORT-NAME", character_data="BrakeRequest"),
            BrokenReference(),
        ]
    )

    with pytest.raises(ArxmlReadError) as exc_info:
        reader._parse_signal_mapping(mapping)

    assert exc_info.value.diagnostics[0].code == "ARXML102"


def test_reader_rejects_missing_signal_reference():
    reader = ArxmlReader(ProjectConfig())
    mapping = SimpleNamespace(
        sub_elements=[
            SimpleNamespace(element_name="SHORT-NAME", character_data="BrakeRequest"),
            SimpleNamespace(element_name="START-POSITION", character_data="0"),
        ]
    )

    with pytest.raises(ArxmlReadError) as exc_info:
        reader._parse_signal_mapping(mapping)

    assert exc_info.value.diagnostics[0].code == "ARXML102"


def test_reader_rejects_unsupported_pdu_construct():
    reader = ArxmlReader(ProjectConfig())
    multiplexed = SimpleNamespace(
        element_name="MULTIPLEXED-I-PDU",
        path="/Communication/MuxPdu",
    )
    reader.model = SimpleNamespace(elements_dfs=[multiplexed])

    with pytest.raises(ArxmlReadError) as exc_info:
        reader._reject_unsupported_pdu_constructs()

    assert exc_info.value.diagnostics[0].code == "ARXML111"


def test_atomic_write_failure_preserves_existing_output(tmp_path, monkeypatch):
    output = tmp_path / "TaktflowSystem.arxml"
    output.write_text("known-good", encoding="utf-8")
    converter = Dbc2Arxml.__new__(Dbc2Arxml)
    converter.diagnostics = DiagnosticBag()
    converter.source_path = "input.dbc"
    converter.am = SimpleNamespace(
        model=SimpleNamespace(
            serialize_files=lambda: {"generated.arxml": "replacement"}
        )
    )

    def fail_replace(_source, _target):
        raise OSError("simulated replace failure")

    monkeypatch.setattr(
        "arxml.dbc2arxml.validate_arxml_strict", lambda _path: None
    )
    monkeypatch.setattr("arxml.dbc2arxml.os.replace", fail_replace)

    with pytest.raises(PipelineDiagnosticError) as exc_info:
        converter.write(tmp_path)

    assert exc_info.value.diagnostics[0].code == "IO001"
    assert output.read_text(encoding="utf-8") == "known-good"
    assert not list(tmp_path.glob(".*.tmp"))


def test_extractor_rejects_malformed_numeric_macro(tmp_path):
    header = tmp_path / "Malformed_Cfg.h"
    header.write_text("#define ECU_LIMIT 0xNOPE\n", encoding="utf-8")

    with pytest.raises(PipelineDiagnosticError) as exc_info:
        parse_defines(header)

    assert exc_info.value.diagnostics[0].code == "MODEL002"
