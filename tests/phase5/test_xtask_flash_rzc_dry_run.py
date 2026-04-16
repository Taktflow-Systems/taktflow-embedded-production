# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B D7 — cargo xtask flash-rzc dry-run.

Mirrors tests/phase5/test_xtask_flash_cvc_dry_run.py for the RZC target.
"""
from __future__ import annotations

import subprocess
import textwrap
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
XTASK_MANIFEST = REPO_ROOT / "tools" / "xtask" / "Cargo.toml"

RZC_STLINK_SERIAL = "TEST-RZC-SN"


def _write_hardware_map(path: Path) -> Path:
    path.write_text(
        textwrap.dedent(
            """
            [[stlink]]
            logical_ecu = "rzc"
            stlink_serial = "TEST-RZC-SN"
            com_port = "COM33"
            """
        ).strip()
        + "\n",
        encoding="utf-8",
    )
    return path


def _run_xtask(
    *args: str,
    hardware_map: Path | None = None,
) -> subprocess.CompletedProcess[str]:
    xtask_args = list(args)
    if hardware_map is not None:
        xtask_args += ["--hardware-map", str(hardware_map)]
    return subprocess.run(
        ["cargo", "run", "--quiet",
         "--manifest-path", str(XTASK_MANIFEST), "--"] + xtask_args,
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
    )


def test_flash_rzc_dry_run_prints_programmer_cli_invocation(tmp_path) -> None:
    if not XTASK_MANIFEST.exists():
        pytest.fail("xtask crate not scaffolded yet (D2 red)")

    result = _run_xtask(
        "flash-rzc",
        "--dry-run",
        hardware_map=_write_hardware_map(tmp_path / "hardware-map.toml"),
    )
    assert result.returncode == 0, (
        f"flash-rzc --dry-run failed:\nSTDOUT:\n{result.stdout}\n"
        f"STDERR:\n{result.stderr}"
    )
    combined = result.stdout + "\n" + result.stderr
    assert "STM32_Programmer_CLI" in combined
    assert f"sn={RZC_STLINK_SERIAL}" in combined, (
        f"dry-run output missing RZC ST-LINK serial "
        f"{RZC_STLINK_SERIAL}:\n{combined}"
    )
    assert "port=SWD" in combined
    assert "rzc_firmware.elf" in combined
    assert "DRY-RUN" in combined or "dry-run" in combined or "dry run" in combined


def test_flash_rzc_defaults_to_dry_run(tmp_path) -> None:
    if not XTASK_MANIFEST.exists():
        pytest.fail("xtask crate not scaffolded yet (D2 red)")

    result = _run_xtask(
        "flash-rzc",
        hardware_map=_write_hardware_map(tmp_path / "hardware-map.toml"),
    )
    assert result.returncode == 0, (
        f"flash-rzc (no flag) should succeed in default dry-run mode:\n"
        f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )
    combined = result.stdout + "\n" + result.stderr
    assert "DRY-RUN" in combined or "dry-run" in combined or "dry run" in combined


def test_flash_rzc_reads_serial_from_hardware_map() -> None:
    if not XTASK_MANIFEST.exists():
        pytest.fail("xtask crate not scaffolded yet (D2 red)")

    src = REPO_ROOT / "tools" / "xtask" / "src" / "main.rs"
    if src.exists():
        source = src.read_text(encoding="utf-8", errors="replace")
        assert RZC_STLINK_SERIAL not in source, (
            f"{src} hard-codes the RZC ST-LINK serial; it must come from "
            f"tools/bench/hardware-map.toml at runtime"
        )
