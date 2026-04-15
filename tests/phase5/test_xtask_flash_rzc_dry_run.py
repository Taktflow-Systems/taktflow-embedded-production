# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B D7 — cargo xtask flash-rzc dry-run.

Mirrors tests/phase5/test_xtask_flash_cvc_dry_run.py for the RZC target.
"""
from __future__ import annotations

import subprocess
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
XTASK_MANIFEST = REPO_ROOT / "tools" / "xtask" / "Cargo.toml"

# Serial for the RZC-assigned ST-LINK probe from tools/bench/hardware-map.toml.
RZC_STLINK_SERIAL = "0049002D3235510C37333439"


def _run_xtask(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["cargo", "run", "--quiet",
         "--manifest-path", str(XTASK_MANIFEST), "--"] + list(args),
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
    )


def test_flash_rzc_dry_run_prints_programmer_cli_invocation() -> None:
    if not XTASK_MANIFEST.exists():
        pytest.fail("xtask crate not scaffolded yet (D2 red)")

    result = _run_xtask("flash-rzc", "--dry-run")
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


def test_flash_rzc_defaults_to_dry_run() -> None:
    if not XTASK_MANIFEST.exists():
        pytest.fail("xtask crate not scaffolded yet (D2 red)")

    result = _run_xtask("flash-rzc")
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
