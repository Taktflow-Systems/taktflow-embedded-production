# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B D7 — cargo xtask flash-fzc dry-run.

Mirrors tests/phase5/test_xtask_flash_cvc_dry_run.py for the FZC target.
THIS RUN SCOPE: the command must default to --dry-run and MUST NOT invoke
STM32_Programmer_CLI.
"""
from __future__ import annotations

import subprocess
import textwrap
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
XTASK_MANIFEST = REPO_ROOT / "tools" / "xtask" / "Cargo.toml"

FZC_STLINK_SERIAL = "TEST-FZC-SN"


def _write_hardware_map(path: Path) -> Path:
    path.write_text(
        textwrap.dedent(
            """
            [[stlink]]
            logical_ecu = "fzc"
            stlink_serial = "TEST-FZC-SN"
            com_port = "COM22"
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


def test_flash_fzc_dry_run_prints_programmer_cli_invocation(tmp_path) -> None:
    if not XTASK_MANIFEST.exists():
        pytest.fail("xtask crate not scaffolded yet (D2 red)")

    result = _run_xtask(
        "flash-fzc",
        "--dry-run",
        hardware_map=_write_hardware_map(tmp_path / "hardware-map.toml"),
    )
    assert result.returncode == 0, (
        f"flash-fzc --dry-run failed:\nSTDOUT:\n{result.stdout}\n"
        f"STDERR:\n{result.stderr}"
    )
    combined = result.stdout + "\n" + result.stderr
    assert "STM32_Programmer_CLI" in combined, (
        f"dry-run output missing STM32_Programmer_CLI:\n{combined}"
    )
    assert f"sn={FZC_STLINK_SERIAL}" in combined, (
        f"dry-run output missing FZC ST-LINK serial "
        f"{FZC_STLINK_SERIAL}:\n{combined}"
    )
    assert "port=SWD" in combined, (
        f"dry-run output missing `port=SWD`:\n{combined}"
    )
    assert "fzc_firmware.elf" in combined, (
        f"dry-run output missing fzc_firmware.elf path:\n{combined}"
    )
    assert "DRY-RUN" in combined or "dry-run" in combined or "dry run" in combined, (
        f"dry-run output must be marked as DRY-RUN:\n{combined}"
    )


def test_flash_fzc_defaults_to_dry_run(tmp_path) -> None:
    if not XTASK_MANIFEST.exists():
        pytest.fail("xtask crate not scaffolded yet (D2 red)")

    result = _run_xtask(
        "flash-fzc",
        hardware_map=_write_hardware_map(tmp_path / "hardware-map.toml"),
    )
    assert result.returncode == 0, (
        f"flash-fzc (no flag) should succeed in default dry-run mode:\n"
        f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )
    combined = result.stdout + "\n" + result.stderr
    assert "DRY-RUN" in combined or "dry-run" in combined or "dry run" in combined, (
        f"default invocation must announce DRY-RUN:\n{combined}"
    )


def test_flash_fzc_reads_serial_from_hardware_map() -> None:
    if not XTASK_MANIFEST.exists():
        pytest.fail("xtask crate not scaffolded yet (D2 red)")

    src = REPO_ROOT / "tools" / "xtask" / "src" / "main.rs"
    if src.exists():
        source = src.read_text(encoding="utf-8", errors="replace")
        assert FZC_STLINK_SERIAL not in source, (
            f"{src} hard-codes the FZC ST-LINK serial; it must come from "
            f"tools/bench/hardware-map.toml at runtime"
        )
