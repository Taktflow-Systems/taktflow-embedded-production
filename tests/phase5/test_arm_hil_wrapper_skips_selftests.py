# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B follow-up - Makefile.arm must build HIL images.

The ARM wrapper delegates into firmware/platform/stm32/Makefile.stm32.
For bench flashing it must always pass HIL=1 so PLATFORM_HIL is defined
inside each ECU's main.c. Without that flag the physical STM32 images keep
their bare-board startup self-tests enabled, which can suppress BSW RUN mode
and leave the bench silent even though flashing succeeded.
"""
from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
MAKEFILE_ARM = REPO_ROOT / "firmware" / "platform" / "arm" / "Makefile.arm"
TARGET = "rzc"
ELF_PATH = REPO_ROOT / "build" / "rzc-arm" / "rzc_firmware.elf"


def _resolve_arm_gcc() -> str | None:
    on_path = shutil.which("arm-none-eabi-gcc")
    if on_path:
        return on_path
    msys_path = "/c/tools/msys64/ucrt64/bin/arm-none-eabi-gcc.exe"
    if Path(msys_path).exists():
        return msys_path
    return None


def _resolve_arm_nm() -> str | None:
    gcc = _resolve_arm_gcc()
    if not gcc:
        return None
    nm = Path(gcc).parent / "arm-none-eabi-nm"
    if nm.exists():
        return str(nm)
    nm_exe = Path(gcc).parent / "arm-none-eabi-nm.exe"
    if nm_exe.exists():
        return str(nm_exe)
    return shutil.which("arm-none-eabi-nm")


def test_makefile_arm_builds_hil_image_without_main_runselftest() -> None:
    gcc = _resolve_arm_gcc()
    nm = _resolve_arm_nm()
    if not gcc or not nm:
        pytest.skip("arm-none-eabi-gcc / arm-none-eabi-nm not available in this env")
    if not MAKEFILE_ARM.exists():
        pytest.fail(f"{MAKEFILE_ARM} missing - Phase 5 ARM wrapper not present")

    env = os.environ.copy()
    env["PATH"] = str(Path(gcc).parent) + os.pathsep + env.get("PATH", "")

    shutil.rmtree(REPO_ROOT / "build" / "stm32" / TARGET, ignore_errors=True)
    shutil.rmtree(ELF_PATH.parent, ignore_errors=True)

    result = subprocess.run(
        [
            "powershell",
            "-NoProfile",
            "-Command",
            f"& make -f '{MAKEFILE_ARM.relative_to(REPO_ROOT)}' TARGET={TARGET}",
        ],
        cwd=REPO_ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, (
        f"{TARGET} ARM HIL build failed:\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )

    assert ELF_PATH.exists(), f"{ELF_PATH} not produced by Makefile.arm"

    nm_result = subprocess.run(
        [nm, str(ELF_PATH)],
        capture_output=True,
        text=True,
        check=True,
    )
    assert "Main_RunSelfTest" not in nm_result.stdout, (
        f"{TARGET} ARM ELF still exports Main_RunSelfTest, so Makefile.arm "
        "did not forward HIL=1 / PLATFORM_HIL into the inner STM32 build."
    )
