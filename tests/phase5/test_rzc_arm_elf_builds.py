# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B D7 — RZC ARM ELF build test.

Mirrors tests/phase5/test_cvc_arm_elf_builds.py for the RZC target.
TDD-red on the D7-entry baseline because RZC has no platform_target/
include directory yet and no identity module.
"""
from __future__ import annotations

import glob
import os
import shutil
import subprocess
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
ELF_PATH = REPO_ROOT / "build" / "rzc-arm" / "rzc_firmware.elf"
MAKEFILE_ARM = REPO_ROOT / "firmware" / "platform" / "arm" / "Makefile.arm"
RZC_PLATFORM_TARGET_DIR = (
    REPO_ROOT / "firmware" / "ecu" / "rzc" / "cfg" / "platform_target"
)


def _resolve_arm_gcc() -> str | None:
    cube_candidates = glob.glob(
        "/c/ST/STM32CubeCLT*/GNU-tools-for-STM32/bin/arm-none-eabi-gcc*"
    )
    if cube_candidates:
        return cube_candidates[0]
    on_path = shutil.which("arm-none-eabi-gcc")
    if on_path:
        return on_path
    msys_path = "/c/tools/msys64/ucrt64/bin/arm-none-eabi-gcc.exe"
    if Path(msys_path).exists():
        return msys_path
    return None


def _resolve_arm_readelf() -> str | None:
    gcc = _resolve_arm_gcc()
    if not gcc:
        return None
    readelf = Path(gcc).parent / "arm-none-eabi-readelf"
    if readelf.exists():
        return str(readelf)
    readelf_exe = Path(gcc).parent / "arm-none-eabi-readelf.exe"
    if readelf_exe.exists():
        return str(readelf_exe)
    return shutil.which("arm-none-eabi-readelf")


def test_rzc_platform_target_dir_exists() -> None:
    """firmware/ecu/rzc/cfg/platform_target/ must exist for the ARM build to
    resolve platform constants. D7 must create it (the scope audit flagged
    this as missing)."""
    assert RZC_PLATFORM_TARGET_DIR.is_dir(), (
        f"{RZC_PLATFORM_TARGET_DIR} is missing. D7 must create it as a "
        "sibling of firmware/ecu/fzc/cfg/platform_target/ so that "
        "Makefile.stm32 -I...platform_target resolves for TARGET=rzc."
    )


def test_rzc_arm_elf_builds() -> None:
    gcc = _resolve_arm_gcc()
    if not gcc:
        pytest.skip("arm-none-eabi-gcc not available in this env")
    if not MAKEFILE_ARM.exists():
        pytest.fail(f"{MAKEFILE_ARM} missing — Phase 5 Line B D1 not present")

    subprocess.run(
        ["make", "-f", str(MAKEFILE_ARM.relative_to(REPO_ROOT)),
         "TARGET=rzc", "clean"],
        cwd=REPO_ROOT,
        check=False,
    )

    env = os.environ.copy()
    env["PATH"] = str(Path(gcc).parent) + os.pathsep + env.get("PATH", "")

    result = subprocess.run(
        ["make", "-f", str(MAKEFILE_ARM.relative_to(REPO_ROOT)),
         "TARGET=rzc"],
        cwd=REPO_ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, (
        f"RZC ARM build failed:\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )
    assert ELF_PATH.exists(), f"{ELF_PATH} not produced by D7 build"
    assert ELF_PATH.stat().st_size > 1024, (
        f"{ELF_PATH} is suspiciously small ({ELF_PATH.stat().st_size} bytes)"
    )


def test_rzc_arm_elf_is_arm_thumb() -> None:
    if not ELF_PATH.exists():
        pytest.skip("ELF not built yet — test_rzc_arm_elf_builds must pass first")
    readelf = _resolve_arm_readelf()
    if not readelf:
        pytest.skip("arm-none-eabi-readelf not available")

    result = subprocess.run(
        [readelf, "-h", str(ELF_PATH)],
        capture_output=True,
        text=True,
        check=True,
    )
    out = result.stdout
    assert "ARM" in out, f"readelf header does not mention ARM:\n{out}"
    assert "Executable" in out or "EXEC" in out, (
        f"ELF not marked as executable:\n{out}"
    )
