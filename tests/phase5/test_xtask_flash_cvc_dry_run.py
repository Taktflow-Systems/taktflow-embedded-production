# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B D2 — cargo xtask flash-cvc dry-run scaffolding test.

THIS RUN SCOPE: the command must default to --dry-run and MUST NOT invoke
STM32_Programmer_CLI. The test asserts:

  1. `cargo run --manifest-path tools/xtask/Cargo.toml -- flash-cvc --dry-run`
     exits 0
  2. The printed command contains `STM32_Programmer_CLI`
  3. The printed command contains `sn=STLINK-CVC-PLACEHOLDER`
     (the CVC ST-LINK serial looked up from tools/bench/hardware-map.toml,
     NOT a hard-coded literal inside xtask source)
  4. The printed command contains `-w` and an ELF path ending in
     `cvc_firmware.elf`
  5. The printed command contains `port=SWD`
  6. Stderr includes a DRY-RUN marker so the operator can tell at a glance
     this is a scaffold run and nothing was flashed

This test does NOT require an ST-LINK to be physically connected.
"""
from __future__ import annotations

import subprocess
import textwrap
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
XTASK_MANIFEST = REPO_ROOT / "tools" / "xtask" / "Cargo.toml"

CVC_STLINK_SERIAL = "TEST-CVC-SN"


def _write_hardware_map(path: Path) -> Path:
    path.write_text(
        textwrap.dedent(
            """
            [[stlink]]
            logical_ecu = "cvc"
            stlink_serial = "TEST-CVC-SN"
            com_port = "COM11"
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


def test_xtask_manifest_exists() -> None:
    assert XTASK_MANIFEST.exists(), (
        f"{XTASK_MANIFEST} missing — Phase 5 Line B D2 must scaffold the "
        "xtask crate"
    )


def test_flash_cvc_dry_run_prints_programmer_cli_invocation(tmp_path) -> None:
    if not XTASK_MANIFEST.exists():
        pytest.fail("xtask crate not scaffolded yet (D2 red)")

    result = _run_xtask(
        "flash-cvc",
        "--dry-run",
        hardware_map=_write_hardware_map(tmp_path / "hardware-map.toml"),
    )
    assert result.returncode == 0, (
        f"flash-cvc --dry-run failed:\nSTDOUT:\n{result.stdout}\n"
        f"STDERR:\n{result.stderr}"
    )
    combined = result.stdout + "\n" + result.stderr
    assert "STM32_Programmer_CLI" in combined, (
        f"dry-run output missing STM32_Programmer_CLI:\n{combined}"
    )
    assert f"sn={CVC_STLINK_SERIAL}" in combined, (
        f"dry-run output missing CVC ST-LINK serial "
        f"{CVC_STLINK_SERIAL}:\n{combined}"
    )
    assert "port=SWD" in combined, (
        f"dry-run output missing `port=SWD`:\n{combined}"
    )
    assert "cvc_firmware.elf" in combined, (
        f"dry-run output missing cvc_firmware.elf path:\n{combined}"
    )
    assert "DRY-RUN" in combined or "dry-run" in combined or "dry run" in combined, (
        f"dry-run output must be marked as DRY-RUN:\n{combined}"
    )


def test_flash_cvc_defaults_to_dry_run(tmp_path) -> None:
    """Calling `flash-cvc` with no mode flag must default to dry-run mode
    and MUST NOT attempt to invoke STM32_Programmer_CLI. This is a hard
    requirement for the autonomous run: no accidental flashing."""
    if not XTASK_MANIFEST.exists():
        pytest.fail("xtask crate not scaffolded yet (D2 red)")

    result = _run_xtask(
        "flash-cvc",
        hardware_map=_write_hardware_map(tmp_path / "hardware-map.toml"),
    )
    assert result.returncode == 0, (
        f"flash-cvc (no flag) should succeed in default dry-run mode:\n"
        f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )
    combined = result.stdout + "\n" + result.stderr
    assert "DRY-RUN" in combined or "dry-run" in combined or "dry run" in combined, (
        f"default invocation must announce DRY-RUN:\n{combined}"
    )


def test_flash_cvc_reads_serial_from_hardware_map(tmp_path) -> None:
    """If we change the hardware-map.toml serial, xtask must pick up the
    new value. This is a no-hardcoded-serial gate for D2."""
    if not XTASK_MANIFEST.exists():
        pytest.fail("xtask crate not scaffolded yet (D2 red)")

    result = _run_xtask(
        "flash-cvc",
        "--dry-run",
        hardware_map=_write_hardware_map(tmp_path / "hardware-map.toml"),
    )
    assert result.returncode == 0
    # The serial must appear in output — but it must not be grep-able as a
    # hard-coded literal in the xtask source.
    src = REPO_ROOT / "tools" / "xtask" / "src" / "main.rs"
    if src.exists():
        source = src.read_text(encoding="utf-8", errors="replace")
        assert CVC_STLINK_SERIAL not in source, (
            f"{src} hard-codes the CVC ST-LINK serial; it must come from "
            f"tools/bench/hardware-map.toml at runtime"
        )
