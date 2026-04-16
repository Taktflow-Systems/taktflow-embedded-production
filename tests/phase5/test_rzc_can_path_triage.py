# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B follow-up - RZC CAN-path triage helper.
"""
from __future__ import annotations

import json
import subprocess
import sys
import tomllib
import textwrap
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = REPO_ROOT / "tools" / "bench" / "rzc_can_path_triage.py"


def _write_hardware_map(path: Path) -> Path:
    path.write_text(
        textwrap.dedent(
            """
            [[stlink]]
            logical_ecu = "rzc"
            stlink_serial = "TEST-RZC-SN"
            com_port = "COM33"
            flashed_image = "build/rzc-arm/rzc_firmware.elf"
            target_mcu = "STM32G47x/G48x"
            notes = "local test map"
            """
        ).strip()
        + "\n",
        encoding="utf-8",
    )
    return path


def _run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), *args],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
    )


def _load_rzc_row(hardware_map: Path) -> dict[str, str]:
    data = tomllib.loads(hardware_map.read_text(encoding="utf-8"))
    for row in data.get("stlink", []):
        if row.get("logical_ecu") == "rzc":
            return row
    raise AssertionError("hardware-map.toml missing RZC [[stlink]] entry")


def test_rzc_can_path_triage_json_reads_hardware_map(tmp_path) -> None:
    assert SCRIPT.exists(), f"{SCRIPT} missing"
    hardware_map = _write_hardware_map(tmp_path / "hardware-map.toml")

    result = _run("--hardware-map", str(hardware_map), "--format", "json")
    assert result.returncode == 0, (
        f"rzc_can_path_triage.py --format json failed:\n"
        f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )

    payload = json.loads(result.stdout)
    row = _load_rzc_row(hardware_map)
    assert payload["logical_ecu"] == "rzc"
    assert payload["stlink_serial"] == row["stlink_serial"]
    assert payload["com_port"] == row["com_port"]
    assert payload["flashed_image"] == row["flashed_image"]
    assert payload["expected_ids"] == {
        "heartbeat": "0x012",
        "request": "0x7E2",
        "response": "0x7EA",
    }
    assert any("STB -> GND" in item["details"] for item in payload["checks"])
    assert any("60 ohm" in item["details"] for item in payload["checks"])
    assert any("0x012" in item for item in payload["success_gates"])


def test_rzc_can_path_triage_text_mentions_probe_and_measurements(tmp_path) -> None:
    hardware_map = _write_hardware_map(tmp_path / "hardware-map.toml")
    row = _load_rzc_row(hardware_map)
    result = _run("--hardware-map", str(hardware_map))
    assert result.returncode == 0, (
        f"default text output failed:\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )
    assert row["stlink_serial"] in result.stdout
    assert row["com_port"] in result.stdout
    assert "PA12" in result.stdout
    assert "PA11" in result.stdout
    assert "60 ohm" in result.stdout
    assert "2.5 V" in result.stdout


def test_rzc_can_path_triage_does_not_hardcode_probe_serial(tmp_path) -> None:
    row = _load_rzc_row(_write_hardware_map(tmp_path / "hardware-map.toml"))
    source = SCRIPT.read_text(encoding="utf-8")
    assert row["stlink_serial"] not in source, (
        f"{SCRIPT} hard-codes the RZC ST-LINK serial; it must come from "
        "tools/bench/hardware-map.toml at runtime"
    )
