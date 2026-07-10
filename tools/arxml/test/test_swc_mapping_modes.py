"""SM5 strict explicit-mapping integration contracts."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
CONVERTER = ROOT / "tools" / "arxml" / "dbc2arxml.py"
FIXTURES = Path(__file__).with_name("fixtures") / "swc_mapping"
DBC = FIXTURES / "validation.dbc"
MODEL = FIXTURES / "validation_model.json"
MAPPING = FIXTURES / "validation_valid.yaml"


def _run(output: Path, *extra: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(CONVERTER),
            str(DBC),
            str(output),
            str(MODEL),
            *extra,
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def test_strict_emits_only_the_validated_explicit_model(tmp_path: Path):
    output = tmp_path / "strict"
    result = _run(
        output,
        "--swc-mapping",
        str(MAPPING),
        "--swc-mapping-mode",
        "strict",
    )
    assert result.returncode == 0, result.stderr
    arxml = (output / "TaktflowSystem.arxml").read_text(encoding="utf-8")
    assert "<SHORT-NAME>CVC_Swc_Pedal</SHORT-NAME>" in arxml
    assert "<SHORT-NAME>PP_PedalPosition</SHORT-NAME>" in arxml
    assert "<SHORT-NAME>RP_BrakeForceCmd</SHORT-NAME>" in arxml
    assert "<SHORT-NAME>Swc_Pedal_Init</SHORT-NAME>" in arxml
    assert "<SHORT-NAME>Swc_Pedal_MainFunction</SHORT-NAME>" in arxml
    assert "<SHORT-NAME>Swc_Pedal_Background</SHORT-NAME>" not in arxml
    assert "SWC mapping: strict explicit emission active" in result.stdout


def test_compatibility_modes_are_rejected(tmp_path: Path):
    for mode in ("shadow", "preferred"):
        result = _run(
            tmp_path / mode, "--swc-mapping", str(MAPPING),
            "--swc-mapping-mode", mode,
        )
        assert result.returncode == 2
        assert "invalid choice" in result.stderr


def test_strict_invalid_mapping_replaces_no_output_and_never_falls_back(tmp_path: Path):
    mapping = tmp_path / "invalid.yaml"
    mapping.write_text(
        MAPPING.read_text(encoding="utf-8").replace(
            "signal: PedalPosition", "signal: UnknownSignal", 1
        ),
        encoding="utf-8",
    )
    output = tmp_path / "output"
    output.mkdir()
    arxml = output / "TaktflowSystem.arxml"
    assumptions = output / "TaktflowSystem.arxml.assumptions.md"
    arxml.write_bytes(b"existing-arxml\n")
    assumptions.write_bytes(b"existing-assumptions\n")

    result = _run(
        output,
        "--swc-mapping",
        str(mapping),
        "--swc-mapping-mode",
        "strict",
    )

    assert result.returncode == 2
    assert "SWCMAP008" in result.stderr
    assert "legacy" not in result.stdout.lower()
    assert arxml.read_bytes() == b"existing-arxml\n"
    assert assumptions.read_bytes() == b"existing-assumptions\n"


def test_domain_and_catch_all_swc_tokens_confer_no_assignment(tmp_path: Path):
    model = tmp_path / "model.json"
    mapping = tmp_path / "mapping.yaml"
    model.write_text(MODEL.read_text(encoding="utf-8").replace(
        "Swc_Pedal", "Swc_BrakeComCanMonitorMain"), encoding="utf-8")
    mapping.write_text(MAPPING.read_text(encoding="utf-8")
        .replace("Swc_Pedal", "Swc_BrakeComCanMonitorMain")
        .replace("CVC_Swc_BrakeComCanMonitorMain", "ComponentWithoutEcuPrefix"),
        encoding="utf-8")
    result = subprocess.run(
        [sys.executable, str(CONVERTER), str(DBC), str(tmp_path / "out"),
         str(model), "--swc-mapping", str(mapping), "--swc-mapping-mode", "strict"],
        cwd=ROOT, text=True, capture_output=True, check=False)
    assert result.returncode == 0, result.stderr
    arxml = (tmp_path / "out" / "TaktflowSystem.arxml").read_text(encoding="utf-8")
    assert "<SHORT-NAME>ComponentWithoutEcuPrefix</SHORT-NAME>" in arxml
    assert arxml.count("<P-PORT-PROTOTYPE>") == 1
    assert arxml.count("<R-PORT-PROTOTYPE>") == 1
