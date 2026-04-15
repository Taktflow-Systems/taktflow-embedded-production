# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B D1 — CVC ARM ELF build test.

Asserts that an ARM cross-build of the CVC firmware produces a valid ELF
at the Phase 5 canonical path and that the ELF targets the STM32G474
(Cortex-M4 ARMv7E-M / Thumb2) resolved from hardware-map.toml.

This test is TDD-red on a clean checkout: the Phase 5 Makefile.arm wrapper
and the build/cvc-arm/cvc_firmware.elf path do not exist yet.

The test builds on top of the existing firmware/platform/stm32/Makefile.stm32
(which already targets STM32G474) and adds a thin Phase 5 wrapper to rename
the output to the Phase 5 canonical `build/cvc-arm/cvc_firmware.elf` path
so it can be consumed by `cargo xtask flash-cvc`.

arm-none-eabi-gcc lookup order (per Phase 5 Line B subset ground rules):
  1. /c/ST/STM32CubeCLT*/GNU-tools-for-STM32/bin/arm-none-eabi-gcc
  2. arm-none-eabi-gcc on PATH
  3. MSYS2 ucrt64 arm-none-eabi-gcc
"""
from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
ELF_PATH = REPO_ROOT / "build" / "cvc-arm" / "cvc_firmware.elf"
MAKEFILE_ARM = REPO_ROOT / "firmware" / "platform" / "arm" / "Makefile.arm"


def _resolve_arm_gcc() -> str | None:
    """Find arm-none-eabi-gcc in the documented search order."""
    import glob

    # 1) STM32CubeCLT
    cube_candidates = glob.glob(
        "/c/ST/STM32CubeCLT*/GNU-tools-for-STM32/bin/arm-none-eabi-gcc*"
    )
    if cube_candidates:
        return cube_candidates[0]

    # 2) PATH
    on_path = shutil.which("arm-none-eabi-gcc")
    if on_path:
        return on_path

    # 3) MSYS2 ucrt64 fallback
    msys_path = "/c/tools/msys64/ucrt64/bin/arm-none-eabi-gcc.exe"
    if Path(msys_path).exists():
        return msys_path

    return None


def _resolve_arm_readelf() -> str | None:
    """Find arm-none-eabi-readelf next to arm-none-eabi-gcc."""
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


def test_arm_gcc_available() -> None:
    """Sanity: arm-none-eabi-gcc must be resolvable via at least one of the
    documented paths. This is what makes D1 meaningful — if the compiler is
    missing the whole D1 test below will fail with a clear environment
    error rather than a silent pass."""
    gcc = _resolve_arm_gcc()
    assert gcc is not None, (
        "arm-none-eabi-gcc not found. Looked at:\n"
        "  1. /c/ST/STM32CubeCLT*/GNU-tools-for-STM32/bin/arm-none-eabi-gcc\n"
        "  2. PATH\n"
        "  3. /c/tools/msys64/ucrt64/bin/arm-none-eabi-gcc.exe\n"
        "Install STM32CubeCLT or the MSYS2 arm-none-eabi toolchain before "
        "running Phase 5 Line B."
    )


def test_makefile_arm_exists() -> None:
    """The Phase 5 canonical Makefile wrapper must exist."""
    assert MAKEFILE_ARM.exists(), (
        f"{MAKEFILE_ARM} is missing. Phase 5 Line B D1 introduces it as a "
        "thin wrapper around firmware/platform/stm32/Makefile.stm32 that "
        "normalises output to build/cvc-arm/cvc_firmware.elf."
    )


def test_cvc_arm_elf_builds() -> None:
    """Building `make -f firmware/platform/arm/Makefile.arm TARGET=cvc`
    produces a readable ELF at build/cvc-arm/cvc_firmware.elf."""
    gcc = _resolve_arm_gcc()
    if not gcc:
        pytest.skip("arm-none-eabi-gcc not available in this env")
    if not MAKEFILE_ARM.exists():
        pytest.fail(f"{MAKEFILE_ARM} missing — D1 Makefile.arm not yet added")

    # Clean so the build is hermetic
    subprocess.run(
        ["make", "-f", str(MAKEFILE_ARM.relative_to(REPO_ROOT)),
         "TARGET=cvc", "clean"],
        cwd=REPO_ROOT,
        check=False,
    )

    env = os.environ.copy()
    # Make sure the resolved gcc's directory is first on PATH so the
    # Makefile.stm32 `PREFIX = arm-none-eabi-` picks it up.
    env["PATH"] = str(Path(gcc).parent) + os.pathsep + env.get("PATH", "")

    result = subprocess.run(
        ["make", "-f", str(MAKEFILE_ARM.relative_to(REPO_ROOT)),
         "TARGET=cvc"],
        cwd=REPO_ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, (
        f"ARM build failed:\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )
    assert ELF_PATH.exists(), f"{ELF_PATH} not produced by D1 build"
    assert ELF_PATH.stat().st_size > 1024, (
        f"{ELF_PATH} is suspiciously small ({ELF_PATH.stat().st_size} bytes)"
    )


def test_cvc_arm_elf_is_arm_thumb() -> None:
    """readelf -h must confirm the ELF is for ARM and Thumb-capable."""
    if not ELF_PATH.exists():
        pytest.skip("ELF not built yet — prerequisite test_cvc_arm_elf_builds "
                    "must succeed first")
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
