from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
CHECKER = ROOT / "tools" / "ci" / "roundtrip_check.py"
DBC = ROOT / "gateway" / "taktflow_vehicle.dbc"
ARXML = ROOT / "arxml" / "TaktflowSystem.arxml"


def run_checker(arxml: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(CHECKER), str(DBC), str(arxml)],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )


def test_in_house_models_are_consistent():
    result = run_checker(ARXML)

    assert result.returncode == 0, result.stderr
    assert result.stdout == "Round-trip consistency check passed: 45 frames, 182 signals.\n"


def test_start_bit_mutation_reports_object_and_values(tmp_path):
    mutated = tmp_path / "mutated.arxml"
    text = ARXML.read_text(encoding="utf-8")
    mutated.write_text(
        text.replace("<START-POSITION>0</START-POSITION>",
                     "<START-POSITION>1</START-POSITION>", 1),
        encoding="utf-8",
    )

    result = run_checker(mutated)

    assert result.returncode == 1
    assert result.stdout == ""
    assert "message 'EStop_Broadcast' signal 'EStop_Broadcast_E2E_DataID'" in result.stderr
    assert "start bit: expected 0, actual 1" in result.stderr
