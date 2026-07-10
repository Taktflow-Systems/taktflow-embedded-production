"""Post-fix CLI contracts for fail-closed ARXML input handling."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

import autosar_data
import pytest


REPO_ROOT = Path(__file__).resolve().parents[3]
CONVERTER = REPO_ROOT / "tools" / "arxml" / "dbc2arxml.py"

DBC_TEXT = '''VERSION ""
NS_ :
    BA_DEF_
    BA_
BS_:
BU_: ECU_A ECU_B
BO_ 256 ValidMessage: 8 ECU_A
 SG_ ValidSignal : 0|8@1+ (1,0) [0|255] "" ECU_B
BA_DEF_ BO_ "GenMsgCycleTime" INT 0 65535;
BA_DEF_ BO_ "GenMsgSendType" STRING;
BA_ "GenMsgCycleTime" BO_ 256 10;
BA_ "GenMsgSendType" BO_ 256 "Cyclic";
'''


@pytest.fixture
def pipeline_case(tmp_path: Path) -> dict[str, Path | str]:
    """Generate a complete baseline exclusively below pytest's temp root."""
    dbc_path = tmp_path / "valid.dbc"
    converter_output = tmp_path / "converter-output"
    dbc_path.write_text(DBC_TEXT, encoding="utf-8")

    completed = subprocess.run(
        [sys.executable, str(CONVERTER), str(dbc_path), str(converter_output)],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert completed.returncode == 0, completed.stdout + completed.stderr

    baseline = converter_output / "TaktflowSystem.arxml"
    assert baseline.is_file()
    return {
        "root": tmp_path,
        "dbc": dbc_path,
        "baseline": baseline.read_text(encoding="utf-8"),
    }


def _write_config(root: Path, arxml_path: Path, dbc_path: Path) -> Path:
    config = root / "project.yaml"
    config.write_text(
        f"""project:
  name: P3PostfixProbe
input:
  arxml: [{arxml_path.name}]
  dbc: {dbc_path.name}
  e2e_source: arxml
output:
  base_dir: generated
ecus:
  ecu_a: {{prefix: ECA}}
  ecu_b: {{prefix: ECB}}
generators:
  com: {{enabled: true}}
""",
        encoding="utf-8",
    )
    return config


def _run_case(
    pipeline_case: dict[str, Path | str],
    content: str,
) -> tuple[subprocess.CompletedProcess[str], Path, Path]:
    root = pipeline_case["root"]
    dbc_path = pipeline_case["dbc"]
    assert isinstance(root, Path)
    assert isinstance(dbc_path, Path)

    arxml_path = root / "case.arxml"
    output_dir = root / "generated"
    arxml_path.write_text(content, encoding="utf-8")
    config = _write_config(root, arxml_path, dbc_path)
    completed = subprocess.run(
        [
            sys.executable,
            "-m",
            "tools.arxmlgen",
            "--config",
            str(config),
            "--output-dir",
            str(output_dir),
            "--generator",
            "com",
            "--quiet",
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    return completed, output_dir, arxml_path


def _assert_fail_closed(
    completed: subprocess.CompletedProcess[str],
    output_dir: Path,
    code: str,
) -> str:
    rendered = completed.stdout + completed.stderr
    assert completed.returncode != 0
    assert f"[{code}] error" in rendered
    assert "Traceback" not in rendered
    assert not output_dir.exists() or not any(
        item.is_file() for item in output_dir.rglob("*")
    )
    return rendered


def _edit_signal_mapping(content: str, edit) -> str:
    pattern = re.compile(
        r"(<I-SIGNAL-TO-I-PDU-MAPPING>)(.*?)(</I-SIGNAL-TO-I-PDU-MAPPING>)",
        re.DOTALL,
    )
    match = pattern.search(content)
    assert match is not None
    replacement = match.group(1) + edit(match.group(2)) + match.group(3)
    return content[: match.start()] + replacement + content[match.end() :]


def _edit_frame_mapping(content: str, edit) -> str:
    pattern = re.compile(
        r"(<PDU-TO-FRAME-MAPPING>)(.*?)(</PDU-TO-FRAME-MAPPING>)",
        re.DOTALL,
    )
    match = pattern.search(content)
    assert match is not None
    replacement = match.group(1) + edit(match.group(2)) + match.group(3)
    return content[: match.start()] + replacement + content[match.end() :]


def _replace_mapping_signal_target(content: str, target: str) -> str:
    def edit(body: str) -> str:
        changed, count = re.subn(
            r"(<I-SIGNAL-REF[^>]*>)[^<]+(</I-SIGNAL-REF>)",
            rf"\g<1>{target}\g<2>",
            body,
            count=1,
        )
        assert count == 1
        return changed

    return _edit_signal_mapping(content, edit)


def test_unresolved_signal_ref_reports_coordinates_and_no_output(pipeline_case):
    baseline = pipeline_case["baseline"]
    assert isinstance(baseline, str)
    broken = _replace_mapping_signal_target(baseline, "/Foreign/MissingSignal")

    completed, output_dir, arxml_path = _run_case(pipeline_case, broken)
    rendered = _assert_fail_closed(completed, output_dir, "ARXML102")

    coordinates = re.search(
        rf"\[ARXML102\] error {re.escape(arxml_path.name)}:(\d+):(\d+) ",
        rendered,
    )
    assert coordinates is not None, rendered
    assert int(coordinates.group(1)) > 0
    assert int(coordinates.group(2)) > 0


def test_schema_loadable_multiplexed_pdu_is_rejected_before_output(pipeline_case):
    baseline = pipeline_case["baseline"]
    root = pipeline_case["root"]
    assert isinstance(baseline, str)
    assert isinstance(root, Path)
    multiplexed = """
                <MULTIPLEXED-I-PDU>
                  <SHORT-NAME>ForeignMux</SHORT-NAME>
                  <LENGTH>8</LENGTH>
                </MULTIPLEXED-I-PDU>"""
    content = baseline.replace(
        "</I-SIGNAL-I-PDU>",
        "</I-SIGNAL-I-PDU>" + multiplexed,
        1,
    )

    load_probe = root / "schema-loadable-multiplexed.arxml"
    load_probe.write_text(content, encoding="utf-8")
    model = autosar_data.AutosarModel()
    model.load_file(str(load_probe))
    assert any(
        str(getattr(item[1] if isinstance(item, tuple) else item, "element_name", ""))
        == "MULTIPLEXED-I-PDU"
        for item in model.elements_dfs
    )

    completed, output_dir, _ = _run_case(pipeline_case, content)
    _assert_fail_closed(completed, output_dir, "ARXML111")


def test_malformed_arxml_is_typed_and_emits_no_output(pipeline_case):
    completed, output_dir, _ = _run_case(
        pipeline_case,
        '<AUTOSAR xmlns="http://autosar.org/schema/r4.0"><BROKEN>',
    )
    _assert_fail_closed(completed, output_dir, "ARXML100")


@pytest.mark.parametrize("missing", ["start-position", "signal-length"])
def test_missing_required_signal_shape_is_arxml112(pipeline_case, missing):
    baseline = pipeline_case["baseline"]
    assert isinstance(baseline, str)
    if missing == "start-position":
        content = _edit_signal_mapping(
            baseline,
            lambda body: re.sub(
                r"\s*<START-POSITION>[^<]*</START-POSITION>", "", body, count=1
            ),
        )
    else:
        pattern = re.compile(
            r"(<I-SIGNAL>\s*<SHORT-NAME>ValidSignal</SHORT-NAME>.*?)"
            r"\s*<LENGTH>[^<]*</LENGTH>",
            re.DOTALL,
        )
        content, count = pattern.subn(r"\1", baseline, count=1)
        assert count == 1

    completed, output_dir, _ = _run_case(pipeline_case, content)
    _assert_fail_closed(completed, output_dir, "ARXML112")


@pytest.mark.parametrize("reference_case", ["missing-signal", "missing-pdu", "broken-pdu"])
def test_missing_or_unresolved_references_are_arxml102(
    pipeline_case, reference_case
):
    baseline = pipeline_case["baseline"]
    assert isinstance(baseline, str)
    if reference_case == "missing-signal":
        content = _edit_signal_mapping(
            baseline,
            lambda body: re.sub(
                r"\s*<I-SIGNAL-REF[^>]*>[^<]*</I-SIGNAL-REF>", "", body, count=1
            ),
        )
    elif reference_case == "missing-pdu":
        content = _edit_frame_mapping(
            baseline,
            lambda body: re.sub(
                r"\s*<PDU-REF[^>]*>[^<]*</PDU-REF>", "", body, count=1
            ),
        )
    else:
        content = _edit_frame_mapping(
            baseline,
            lambda body: re.sub(
                r"(<PDU-REF[^>]*>)[^<]+(</PDU-REF>)",
                r"\1/Foreign/MissingPdu\2",
                body,
                count=1,
            ),
        )

    completed, output_dir, _ = _run_case(pipeline_case, content)
    _assert_fail_closed(completed, output_dir, "ARXML102")


def test_incomplete_frame_pdu_routing_is_arxml113(pipeline_case):
    baseline = pipeline_case["baseline"]
    assert isinstance(baseline, str)
    content, count = re.subn(
        r"\s*<PDU-TO-FRAME-MAPPINGS>.*?</PDU-TO-FRAME-MAPPINGS>",
        "",
        baseline,
        count=1,
        flags=re.DOTALL,
    )
    assert count == 1

    completed, output_dir, _ = _run_case(pipeline_case, content)
    _assert_fail_closed(completed, output_dir, "ARXML113")
