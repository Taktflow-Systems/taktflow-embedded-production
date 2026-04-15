# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B D7 follow-up — FZC UDS F190 multi-frame TX buffer plausibility.

Context:
    Bench test 2026-04-15 found that FZC returns UDS SingleFrame responses
    correctly (F191 HW version round-trips in ~15 ms) but emits **zero**
    wire output for F190 (20-byte VIN response requiring ISO-TP FF+CF
    segmentation). CVC works in the same scenario on the parallel
    codegen path.

    This test is a static config plausibility guard. It does NOT exercise
    the runtime Dcm-CanTp-CanIf stack; it asserts that the configured
    buffers, macros, and DID table declare enough room to hold a 20-byte
    UDS F190 response (1 positive SID echo + 2 DID bytes + 17 VIN bytes).

    If a future codegen regression shrinks any of these back below 20
    bytes, this test will red immediately and the operator will have a
    clear config pointer instead of a silent bench replay.

Assertions:
    - DCM_TX_BUF_SIZE >= 20
    - CANTP_MAX_PAYLOAD >= 20
    - Fzc_Cfg_Fzc.c F190 DID entry has DataLength == 17
    - FZC_IDENTITY_VIN_LEN == 17
"""
from __future__ import annotations

import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]

DCM_H = REPO_ROOT / "firmware" / "bsw" / "services" / "Dcm" / "include" / "Dcm.h"
CANTP_H = REPO_ROOT / "firmware" / "bsw" / "services" / "CanTp" / "include" / "CanTp.h"
FZC_DCM_CFG = REPO_ROOT / "firmware" / "ecu" / "fzc" / "cfg" / "Dcm_Cfg_Fzc.c"
FZC_IDENTITY_H = (
    REPO_ROOT / "firmware" / "ecu" / "fzc" / "include" / "Fzc_Identity.h"
)

# UDS F190 response layout: [0x62][0xF1][0x90][VIN x 17] = 20 bytes
MIN_F190_RESPONSE_BYTES = 20


def _extract_define(path: Path, name: str) -> int:
    text = path.read_text(encoding="utf-8")
    m = re.search(
        rf"#define\s+{re.escape(name)}\s+(\d+)u?\b",
        text,
    )
    assert m, f"{name} not found in {path}"
    return int(m.group(1))


def test_dcm_tx_buf_size_fits_f190_response() -> None:
    size = _extract_define(DCM_H, "DCM_TX_BUF_SIZE")
    assert size >= MIN_F190_RESPONSE_BYTES, (
        f"DCM_TX_BUF_SIZE={size} is too small for the 20-byte F190 VIN "
        "response. The Dcm handler will truncate the positive response "
        "before ever reaching PduR/CanTp."
    )


def test_cantp_max_payload_fits_f190_response() -> None:
    size = _extract_define(CANTP_H, "CANTP_MAX_PAYLOAD")
    assert size >= MIN_F190_RESPONSE_BYTES, (
        f"CANTP_MAX_PAYLOAD={size} is too small for the 20-byte F190 VIN "
        "response. CanTp_Transmit will reject the call with DET_E_PARAM_VALUE "
        "and the wire will be silent."
    )


def test_fzc_identity_vin_len_is_iso3779() -> None:
    vin_len = _extract_define(FZC_IDENTITY_H, "FZC_IDENTITY_VIN_LEN")
    assert vin_len == 17, (
        f"FZC_IDENTITY_VIN_LEN={vin_len}, ISO 3779 fixes VIN length at 17. "
        "A mismatch here breaks the DCM DID table literal and the buffer "
        "plausibility guard in Dcm_Cfg_Fzc.c."
    )


def test_fzc_did_table_f190_data_length_is_17() -> None:
    text = FZC_DCM_CFG.read_text(encoding="utf-8")
    # Match the DID table entry for 0xF190 — matches
    #   { 0xF190u, Dcm_ReadDid_Vin, 17u }
    # with whitespace tolerance.
    m = re.search(
        r"\{\s*0xF190u\s*,\s*Dcm_ReadDid_Vin\s*,\s*(\d+)u\s*\}",
        text,
    )
    assert m, "FZC DID table F190 entry not found in Dcm_Cfg_Fzc.c"
    length = int(m.group(1))
    assert length == 17, (
        f"FZC F190 DID entry declares DataLength={length}, expected 17 "
        "(ISO 3779 VIN). A mismatch here desynchronizes the Dcm handler "
        "buffer bounds check from the FZC_IDENTITY_VIN_LEN macro."
    )
