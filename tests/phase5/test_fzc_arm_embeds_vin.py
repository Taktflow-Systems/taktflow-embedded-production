# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B D7 — FZC ARM VIN embed regression.

Mirrors tests/phase5/test_cvc_arm_embeds_vin.py for the FZC target.
The FZC ARM ELF must contain the 17 raw VIN bytes from fzc_identity.toml
in .rodata so Fzc_Identity_GetVin() can return them for the F190 Dcm
handler at runtime.

This is TDD-red on the D7-entry baseline because fzc_identity.toml does
not exist yet.
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
ELF_PATH = REPO_ROOT / "build" / "fzc-arm" / "fzc_firmware.elf"
MAKEFILE_ARM = REPO_ROOT / "firmware" / "platform" / "arm" / "Makefile.arm"
FZC_IDENTITY_CONFIG = (
    REPO_ROOT / "firmware" / "ecu" / "fzc" / "cfg" / "fzc_identity.toml"
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
    if not FZC_IDENTITY_CONFIG.exists():
        pytest.fail(
            f"{FZC_IDENTITY_CONFIG} is missing — D7 must add it with a 17-char "
            "VIN before this test can validate the embed step."
        )
    text = FZC_IDENTITY_CONFIG.read_text(encoding="utf-8")
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("#") or not s:
            continue
        m = re.match(r'^vin\s*=\s*"(?P<v>[^"]+)"\s*$', s)
        if m:
            v = m.group("v")
            if len(v) != 17:
                pytest.fail(
                    f"fzc_identity.toml VIN is {len(v)} chars, ISO 3779 wants 17"
                )
            return v.encode("ascii")
    pytest.fail("vin key not found in fzc_identity.toml")


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
            "TARGET=fzc",
        ],
        cwd=REPO_ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        pytest.fail(
            "FZC ARM build failed:\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
    assert ELF_PATH.exists(), f"{ELF_PATH} not produced by FZC ARM build"


def test_fzc_arm_elf_embeds_vin_from_identity_toml() -> None:
    vin_bytes = _load_vin_bytes_from_config()
    _build_arm_elf()
    elf_blob = ELF_PATH.read_bytes()
    assert vin_bytes in elf_blob, (
        "VIN bytes from fzc_identity.toml were not found in the FZC ARM ELF. "
        "D7 requires that embed_identity.py generates a header from "
        "fzc_identity.toml and main.c calls Fzc_Identity_InitFromBuffer() "
        "with it on the !PLATFORM_POSIX path."
    )
