# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B follow-up - hardware-map bench helpers.
"""
from __future__ import annotations

from pathlib import Path

from tools.bench.hardware_map import (
    DEFAULT_HARDWARE_MAP,
    HARDWARE_MAP_ENV,
    active_hardware_map_path,
    load_hardware_map,
    resolve_stlink_probe,
    resolve_stlink_serial,
)


TEST_MAP = """\
[[stlink]]
logical_ecu = "cvc"
stlink_serial = "TEST-CVC-SN"
com_port = "COM11"

[[stlink]]
logical_ecu = "rzc"
stlink_serial = "TEST-RZC-SN"
com_port = "COM33"
"""


def _write_map(path: Path) -> Path:
    path.write_text(TEST_MAP, encoding="utf-8")
    return path


def test_default_hardware_map_exists() -> None:
    assert DEFAULT_HARDWARE_MAP.exists(), f"{DEFAULT_HARDWARE_MAP} missing"


def test_active_hardware_map_path_prefers_env_override(tmp_path) -> None:
    custom = _write_map(tmp_path / "hardware-map.toml")
    assert active_hardware_map_path(env={HARDWARE_MAP_ENV: str(custom)}) == custom


def test_resolve_stlink_probe_reads_rzc_assignment(tmp_path) -> None:
    row = resolve_stlink_probe("rzc", path=_write_map(tmp_path / "hardware-map.toml"))
    assert row["logical_ecu"] == "rzc"
    assert row["stlink_serial"] == "TEST-RZC-SN"
    assert row["com_port"] == "COM33"


def test_resolve_stlink_serial_matches_hardware_map(tmp_path) -> None:
    map_path = _write_map(tmp_path / "hardware-map.toml")
    data = load_hardware_map(path=map_path)
    cvc = next(row for row in data["stlink"] if row["logical_ecu"] == "cvc")
    assert resolve_stlink_serial("cvc", path=map_path) == cvc["stlink_serial"]


def test_resolve_stlink_serial_prefers_env_serial_override(tmp_path) -> None:
    map_path = _write_map(tmp_path / "hardware-map.toml")
    assert (
        resolve_stlink_serial(
            "cvc",
            path=map_path,
            env={"CVC_SN": "ENV-CVC-SN"},
        )
        == "ENV-CVC-SN"
    )
