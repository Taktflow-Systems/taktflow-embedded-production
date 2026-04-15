# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B D7 — RZC ARM VIN embed regression.

Mirrors tests/phase5/test_cvc_arm_embeds_vin.py for the RZC target.
The RZC ARM ELF must contain the 17 raw VIN bytes from rzc_identity.toml
in .rodata so Rzc_Identity_GetVin() can return them for the F190 Dcm
handler at runtime.
"""
from __future__ import annotations

import glob
import os
import re
import shutil
import subprocess
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
ELF_PATH = REPO_ROOT / "build" / "rzc-arm" / "rzc_firmware.elf"
MAKEFILE_ARM = REPO_ROOT / "firmware" / "platform" / "arm" / "Makefile.arm"
RZC_IDENTITY_CONFIG = (
    REPO_ROOT / "firmware" / "ecu" / "rzc" / "cfg" / "rzc_identity.toml"
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


def _load_vin_bytes_from_config() -> bytes:
    if not RZC_IDENTITY_CONFIG.exists():
        pytest.fail(
            f"{RZC_IDENTITY_CONFIG} is missing — D7 must add it with a 17-char VIN."
        )
    text = RZC_IDENTITY_CONFIG.read_text(encoding="utf-8")
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("#") or not s:
            continue
        m = re.match(r'^vin\s*=\s*"(?P<v>[^"]+)"\s*$', s)
        if m:
            v = m.group("v")
            if len(v) != 17:
                pytest.fail(
                    f"rzc_identity.toml VIN is {len(v)} chars, ISO 3779 wants 17"
                )
            return v.encode("ascii")
    pytest.fail("vin key not found in rzc_identity.toml")


def _build_arm_elf() -> None:
    gcc = _resolve_arm_gcc()
    if not gcc:
        pytest.skip("arm-none-eabi-gcc not available in this env")
    if not MAKEFILE_ARM.exists():
        pytest.fail(f"{MAKEFILE_ARM} missing — Phase 5 Line B D1 not present")

    env = os.environ.copy()
    env["PATH"] = str(Path(gcc).parent) + os.pathsep + env.get("PATH", "")

    result = subprocess.run(
        [
            "make",
            "-f",
            str(MAKEFILE_ARM.relative_to(REPO_ROOT)),
            "TARGET=rzc",
        ],
        cwd=REPO_ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        pytest.fail(
            "RZC ARM build failed:\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
    assert ELF_PATH.exists(), f"{ELF_PATH} not produced by RZC ARM build"


def test_rzc_arm_elf_embeds_vin_from_identity_toml() -> None:
    vin_bytes = _load_vin_bytes_from_config()
    _build_arm_elf()
    elf_blob = ELF_PATH.read_bytes()
    assert vin_bytes in elf_blob, (
        "VIN bytes from rzc_identity.toml were not found in the RZC ARM ELF. "
        "D7 requires that embed_identity.py generates a header from "
        "rzc_identity.toml and main.c calls Rzc_Identity_InitFromBuffer() "
        "with it on the !PLATFORM_POSIX path."
    )
