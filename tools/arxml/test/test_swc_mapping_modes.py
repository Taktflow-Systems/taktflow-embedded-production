"""SM3 shadow and SM4 preferred-mode integration contracts."""

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


def test_shadow_validates_explicit_mapping_but_emits_legacy_bytes(tmp_path: Path):
    legacy = tmp_path / "legacy"
    shadow = tmp_path / "shadow"

    legacy_result = _run(legacy)
    shadow_result = _run(
        shadow,
        "--swc-mapping",
        str(MAPPING),
        "--swc-mapping-mode",
        "shadow",
    )

    assert legacy_result.returncode == 0, legacy_result.stderr
    assert shadow_result.returncode == 0, shadow_result.stderr
    assert (legacy / "TaktflowSystem.arxml").read_bytes() == (
        shadow / "TaktflowSystem.arxml"
    ).read_bytes()
    assert (legacy / "TaktflowSystem.arxml.assumptions.md").read_bytes() == (
        shadow / "TaktflowSystem.arxml.assumptions.md"
    ).read_bytes()
    assert "SWC mapping: shadow validation passed; legacy emission remains active" in (
        shadow_result.stdout
    )


def test_shadow_validation_failure_replaces_no_output(tmp_path: Path):
    mapping = tmp_path / "invalid.yaml"
    text = MAPPING.read_text(encoding="utf-8")
    mapping.write_text(
        text.replace("signal: PedalPosition", "signal: UnknownSignal", 1),
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
        "shadow",
    )

    assert result.returncode == 2
    assert "SWCMAP008" in result.stderr
    assert arxml.read_bytes() == b"existing-arxml\n"
    assert assumptions.read_bytes() == b"existing-assumptions\n"


def test_mapping_path_and_shadow_mode_must_be_supplied_together(tmp_path: Path):
    missing_mode = _run(tmp_path / "missing-mode", "--swc-mapping", str(MAPPING))
    missing_mapping = _run(
        tmp_path / "missing-mapping", "--swc-mapping-mode", "shadow"
    )

    assert missing_mode.returncode == 2
    assert missing_mapping.returncode == 2
    assert "must be supplied together" in missing_mode.stderr
    assert "must be supplied together" in missing_mapping.stderr


def test_preferred_emits_only_the_validated_explicit_model(tmp_path: Path):
    output = tmp_path / "preferred"

    result = _run(
        output,
        "--swc-mapping",
        str(MAPPING),
        "--swc-mapping-mode",
        "preferred",
    )

    assert result.returncode == 0, result.stderr
    arxml = (output / "TaktflowSystem.arxml").read_text(encoding="utf-8")
    assert "<SHORT-NAME>CVC_Swc_Pedal</SHORT-NAME>" in arxml
    assert "<SHORT-NAME>PP_PedalPosition</SHORT-NAME>" in arxml
    assert "<SHORT-NAME>RP_BrakeForceCmd</SHORT-NAME>" in arxml
    assert "<SHORT-NAME>Swc_Pedal_Init</SHORT-NAME>" in arxml
    assert "<SHORT-NAME>Swc_Pedal_MainFunction</SHORT-NAME>" in arxml
    assert "<SHORT-NAME>Swc_Pedal_Background</SHORT-NAME>" not in arxml
    assert "SWC mapping: preferred explicit emission active" in result.stdout


def test_preferred_invalid_mapping_replaces_no_output_and_never_falls_back(tmp_path: Path):
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
        "preferred",
    )

    assert result.returncode == 2
    assert "SWCMAP008" in result.stderr
    assert "legacy" not in result.stdout.lower()
    assert arxml.read_bytes() == b"existing-arxml\n"
    assert assumptions.read_bytes() == b"existing-assumptions\n"
