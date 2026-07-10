"""SM6 strict-mapping migration closure contracts."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from tools.arxml.swc_mapping import load_swc_mapping


ROOT = Path(__file__).resolve().parents[3]
CHECKER = ROOT / "tools" / "ci" / "check_swc_mapping.py"
DBC = ROOT / "gateway" / "taktflow_vehicle.dbc"
SWC_MODEL = ROOT / "arxml_v2" / "swc_model.json"
ARXML = ROOT / "arxml" / "TaktflowSystem.arxml"
MAPPING = ROOT / "model" / "ecu_sidecar.yaml"
GOLDENS = ROOT / "tools" / "arxml" / "test" / "golden" / "swc_mapping"
FIXTURES = ROOT / "tools" / "arxml" / "test" / "fixtures" / "swc_mapping"


def test_release_ci_and_checker_accept_strict_only():
    workflow = (ROOT / ".github" / "workflows" / "ci.yml").read_text(
        encoding="utf-8"
    )
    idempotency = (ROOT / "tools" / "ci" / "check_codegen_idempotency.sh").read_text(
        encoding="utf-8"
    )
    assert "--swc-mapping-mode strict" in workflow
    assert "--mode strict --check" in workflow.replace("\\\n", " ")
    assert "--swc-mapping-mode strict" in idempotency

    missing_mode = subprocess.run(
        [
            sys.executable,
            str(CHECKER),
            str(DBC),
            str(SWC_MODEL),
            str(ARXML),
            "--output",
            "unused.json",
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    assert missing_mode.returncode == 2
    assert "--mode" in missing_mode.stderr

    for mode in ("shadow", "preferred"):
        result = subprocess.run(
            [
                sys.executable,
                str(CHECKER),
                str(DBC),
                str(SWC_MODEL),
                str(MAPPING),
                str(ARXML),
                "--mode",
                mode,
            ],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        assert result.returncode == 2
        assert "invalid choice" in result.stderr


def test_active_mapping_and_final_report_have_no_expired_compatibility_waiver():
    assert "SM3-COMPAT-001" not in MAPPING.read_text(encoding="utf-8")
    assert "SM3-COMPAT-001" not in (GOLDENS / "strict_parity.json").read_text(
        encoding="utf-8"
    )


def test_mapping_closes_exact_seven_ecu_coverage():
    mapping = load_swc_mapping(MAPPING)

    assert tuple(ecu.name for ecu in mapping.ecus) == (
        "bcm",
        "cvc",
        "fzc",
        "icu",
        "rzc",
        "sc",
        "tcu",
    )
    assert sum(len(ecu.swcs) for ecu in mapping.ecus) == 48
    assert sum(len(swc.ports) for ecu in mapping.ecus for swc in ecu.swcs) == 848
    assert sum(len(swc.runnables) for ecu in mapping.ecus for swc in ecu.swcs) == 71
    assert sum(len(ecu.unmapped_signals) for ecu in mapping.ecus) == 185
    assert sum(len(ecu.unmapped_runnables) for ecu in mapping.ecus) == 69
    assert not any(ecu.unmapped_signal_sets for ecu in mapping.ecus)


def test_only_final_strict_golden_and_negative_fixtures_are_retained():
    assert {path.name for path in GOLDENS.iterdir() if path.is_file()} == {
        "strict_parity.json"
    }
    assert {path.name for path in FIXTURES.iterdir() if path.is_file()} == {
        "duplicate_key.yaml",
        "invalid_short_name.yaml",
        "missing_required.yaml",
        "pattern_identity.yaml",
        "valid.yaml",
        "valid_reordered.yaml",
        "validation.dbc",
        "validation_model.json",
        "validation_valid.yaml",
        "wrong_type.yaml",
    }
