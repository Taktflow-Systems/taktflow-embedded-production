# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B follow-up — DID 0xF190 ARM VIN-handler regression.

Background
----------
PR #12 produced the first ARM ELF for the CVC ECU and the operator flashed
the STM32G474 board over ST-LINK. The live CAN test on can0 showed:

  * SID 0x3E (TesterPresent) -> 0x7E 0x00 PASS
  * SID 0x22 0xF1 0x90 (ReadDataByIdentifier VIN) -> NRC 0x31 (requestOutOfRange)

The Dcm core, FDCAN1 driver and ISO-TP SF path all work end-to-end. The gap
is that on ARM the identity store is empty, so Cvc_Identity_GetVin() returns
E_NOT_OK and the F190 callback yields E_NOT_OK -> NRC 0x31.

Why the store is empty: main.c calls Cvc_Identity_InitFromFile("cvc_identity.toml")
on every platform, but the ARM build uses --specs=nosys.specs which makes
fopen() return NULL (no filesystem). The only POSIX-vs-ARM divergence is the
identity loader source, not the Dcm config table — Dcm_Cfg_Cvc.c already
registers DID 0xF190 with handler Dcm_ReadDid_Vin and the 17-byte length.

Fix shape (this PR)
-------------------
Generate a build-time header `cvc_identity_data.h` from cvc_identity.toml,
embed it in flash as a const byte array, and have main.c call
Cvc_Identity_InitFromBuffer() with that array on the !PLATFORM_POSIX path.
The VIN literal still lives only in cvc_identity.toml — the generator emits
it as a hex byte array so the hardcoded-VIN regression test stays green.

This test
---------
Builds the ARM ELF and asserts that the 17 raw VIN bytes loaded from
cvc_identity.toml are present somewhere in the ELF image. That is necessary
and sufficient evidence that the identity blob made it into flash and is
reachable by Cvc_Identity_InitFromBuffer at boot.

The test is TDD-red on the PR #12 baseline because main.c on ARM only
attempts a fopen() that always fails — the VIN bytes never reach .rodata.
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
ELF_PATH = REPO_ROOT / "build" / "cvc-arm" / "cvc_firmware.elf"
MAKEFILE_ARM = REPO_ROOT / "firmware" / "platform" / "arm" / "Makefile.arm"
CVC_IDENTITY_CONFIG = (
    REPO_ROOT / "firmware" / "ecu" / "cvc" / "cfg" / "cvc_identity.toml"
)


def _resolve_arm_gcc() -> str | None:
    """Mirror the resolver used by test_cvc_arm_elf_builds.py."""
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
    """Extract the raw 17-byte VIN from cvc_identity.toml.

    Single source of truth — the test never inlines the VIN literal in
    Python source, matching the discipline enforced by
    tests/phase4/test_no_hardcoded_vin_in_src.py for firmware sources.
    """
    text = CVC_IDENTITY_CONFIG.read_text(encoding="utf-8")
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("#") or not s:
            continue
        m = re.match(r'^vin\s*=\s*"(?P<v>[^"]+)"\s*$', s)
        if m:
            v = m.group("v")
            if len(v) != 17:
                pytest.fail(
                    f"cvc_identity.toml VIN is {len(v)} chars, ISO 3779 wants 17"
                )
            return v.encode("ascii")
    pytest.fail("vin key not found in cvc_identity.toml")


def _build_arm_elf() -> None:
    """Build the CVC ARM ELF; skip the test if the toolchain is missing."""
    gcc = _resolve_arm_gcc()
    if not gcc:
        pytest.skip("arm-none-eabi-gcc not available in this env")
    if not MAKEFILE_ARM.exists():
        pytest.fail(f"{MAKEFILE_ARM} missing — Phase 5 D1 should have added it")

    env = os.environ.copy()
    env["PATH"] = str(Path(gcc).parent) + os.pathsep + env.get("PATH", "")

    result = subprocess.run(
        [
            "make",
            "-f",
            str(MAKEFILE_ARM.relative_to(REPO_ROOT)),
            "TARGET=cvc",
        ],
        cwd=REPO_ROOT,
        env=env,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        pytest.fail(
            "ARM build failed:\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )
    assert ELF_PATH.exists(), f"{ELF_PATH} not produced by ARM build"


def test_cvc_arm_elf_embeds_vin_from_identity_toml() -> None:
    """The ARM ELF must contain the 17 raw VIN bytes from cvc_identity.toml.

    Red-on-PR-#12: main.c on ARM only tries fopen() which always fails under
    nosys.specs, so the VIN never reaches flash and this scan returns no
    match. After the fix (generated cvc_identity_data.h + InitFromBuffer
    in main.c on !PLATFORM_POSIX), the VIN bytes are present in .rodata
    and this test passes.
    """
    vin_bytes = _load_vin_bytes_from_config()
    _build_arm_elf()
    elf_blob = ELF_PATH.read_bytes()
    assert vin_bytes in elf_blob, (
        "VIN bytes from cvc_identity.toml were not found in the ARM ELF. "
        "This means the build does not embed the identity blob; the F190 "
        "Dcm handler will return E_NOT_OK at runtime (NRC 0x31). Fix: "
        "generate build/cvc-arm/generated/cvc_identity_data.h from "
        "firmware/ecu/cvc/cfg/cvc_identity.toml and have main.c call "
        "Cvc_Identity_InitFromBuffer() with it on the !PLATFORM_POSIX path."
    )
