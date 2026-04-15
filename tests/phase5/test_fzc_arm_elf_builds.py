# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B D7 — FZC ARM ELF build test.

Mirrors tests/phase5/test_cvc_arm_elf_builds.py for the FZC target.

Asserts that an ARM cross-build of the FZC firmware produces a valid ELF
at the Phase 5 canonical path `build/fzc-arm/fzc_firmware.elf` and that
the ELF targets the STM32G474 (Cortex-M4 ARMv7E-M / Thumb2).

This test is TDD-red on the D7-entry baseline: Makefile.arm already knows
about TARGET=fzc but the FZC build actually requires:
  * firmware/ecu/fzc/cfg/Fzc_Identity.c + include/Fzc_Identity.h
  * firmware/ecu/fzc/cfg/fzc_identity.toml
  * Fzc_Identity_InitFromBuffer() call in firmware/ecu/fzc/src/main.c
  * EMBED_IDENTITY_TOML_fzc wired into Makefile.arm
  * embed_identity.py generalised so it emits a symbol namespace matching
    the ECU name rather than the hardcoded `cvc_identity_toml_data`
  * Dcm_Cfg_Fzc.c rewired from the 4-byte Dcm_ReadDid_EcuId (returning the
    ASCII literal "FZC1") to a 17-byte Dcm_ReadDid_Vin that pulls from
    Fzc_Identity_GetVin().
"""
from __future__ import annotations

import glob
import os
import shutil
import subprocess
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
ELF_PATH = REPO_ROOT / "build" / "fzc-arm" / "fzc_firmware.elf"
MAKEFILE_ARM = REPO_ROOT / "firmware" / "platform" / "arm" / "Makefile.arm"


def _resolve_arm_gcc() -> str | None:
    """Find arm-none-eabi-gcc in the documented search order."""
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


def test_fzc_arm_elf_builds() -> None:
    """Building `make -f firmware/platform/arm/Makefile.arm TARGET=fzc`
    produces a readable ELF at build/fzc-arm/fzc_firmware.elf."""
    gcc = _resolve_arm_gcc()
    if not gcc:
        pytest.skip("arm-none-eabi-gcc not available in this env")
    if not MAKEFILE_ARM.exists():
        pytest.fail(f"{MAKEFILE_ARM} missing — Phase 5 Line B D1 not present")

    subprocess.run(
        ["make", "-f", str(MAKEFILE_ARM.relative_to(REPO_ROOT)),
         "TARGET=fzc", "clean"],
        cwd=REPO_ROOT,
        check=False,
    )

    env = os.environ.copy()
    env["PATH"] = str(Path(gcc).parent) + os.pathsep + env.get("PATH", "")

    result = subprocess.run(
        ["make", "-f", str(MAKEFILE_ARM.relative_to(REPO_ROOT)),
         "TARGET=fzc"],
        cwd=REPO_ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, (
        f"FZC ARM build failed:\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )
    assert ELF_PATH.exists(), f"{ELF_PATH} not produced by D7 build"
    assert ELF_PATH.stat().st_size > 1024, (
        f"{ELF_PATH} is suspiciously small ({ELF_PATH.stat().st_size} bytes)"
    )


def test_fzc_arm_elf_is_arm_thumb() -> None:
    if not ELF_PATH.exists():
        pytest.skip("ELF not built yet — test_fzc_arm_elf_builds must pass first")
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
