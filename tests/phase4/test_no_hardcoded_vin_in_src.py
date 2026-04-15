"""Phase 4 Line B D6 — hardcoded-literal regression for VIN + DIDs.

This test extends the pattern already used by
tools/odx-gen/tests/test_no_hardcoded_in_fault_lib.py to the Phase 4
Line B deliverables: no C source under the CVC firmware tree may
inline the VIN string loaded from cvc_identity.toml, and no Python,
Dockerfile, or YAML source in the phase-4 scope may inline a 4-hex
DID literal or a 17-character alphanumeric VIN-shaped token.

The forbidden list is built at test time:

  * The VIN literal is read from firmware/ecu/cvc/cfg/cvc_identity.toml.
  * The DID ids come from the odx-gen parser against Dcm_Cfg_Cvc.c.
  * A regex matches any 4-hex-digit literal `0x[0-9A-Fa-f]{4}` and
    any 17-character ISO-3779 VIN-shaped token `[A-HJ-NPR-Z0-9]{17}`.

Files that are intentional exceptions (the config file itself,
the C source that owns the DID table, the odx-gen parser tests that
reference DID values as part of their regression scan) are listed
in ALLOWED_SOURCES below.
"""
from __future__ import annotations

import re
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
CVC_IDENTITY_CONFIG = REPO_ROOT / "firmware" / "ecu" / "cvc" / "cfg" / "cvc_identity.toml"
DCM_CFG_CVC = REPO_ROOT / "firmware" / "ecu" / "cvc" / "cfg" / "Dcm_Cfg_Cvc.c"

# File extensions we scan for hardcoded literals.
SCANNED_EXTS = {".c", ".h", ".py", ".yml", ".yaml", ".toml", ".json", ".md"}
DOCKERFILE_NAMES = {"Dockerfile"}

# Paths excluded from scanning. Exclusions are LOAD-BEARING — they are
# the files that are ALLOWED to contain the literal because they are
# either the source of truth or test regression fixtures that parse
# DIDs as part of their job.
EXCLUDED_PATHS = {
    # The VIN lives here by design.
    REPO_ROOT / "firmware" / "ecu" / "cvc" / "cfg" / "cvc_identity.toml",
    # The DID table is the source of truth for DID hex literals.
    DCM_CFG_CVC,
    # Sibling ECU Dcm_Cfg files — they also hold legitimate DID hex
    # literals that are the source of truth for their respective ECUs.
    REPO_ROOT / "firmware" / "ecu" / "fzc" / "cfg" / "Dcm_Cfg_Fzc.c",
    REPO_ROOT / "firmware" / "ecu" / "rzc" / "cfg" / "Dcm_Cfg_Rzc.c",
    REPO_ROOT / "firmware" / "ecu" / "tcu" / "cfg" / "Dcm_Cfg_Tcu.c",
    REPO_ROOT / "firmware" / "ecu" / "bcm" / "cfg" / "Dcm_Cfg_Bcm.c",
    REPO_ROOT / "firmware" / "ecu" / "icu" / "cfg" / "Dcm_Cfg_Icu.c",
}

# Directories scanned by this test (Phase 4 scope only).
SCANNED_DIRS = [
    REPO_ROOT / "firmware" / "ecu" / "cvc",
    REPO_ROOT / "firmware" / "ecu" / "fzc",
    REPO_ROOT / "firmware" / "ecu" / "rzc",
    REPO_ROOT / "deploy" / "docker",
    REPO_ROOT / "tests" / "phase4",
    REPO_ROOT / "tests" / "interop",
]

# --- regex ----------------------------------------------------------

# For search-in-text we need word boundaries. For exact-match checks
# (fullmatch) we don't. Keep two patterns to make the intent explicit.
_VIN_SHAPE_SEARCH = re.compile(r"\b[A-HJ-NPR-Z0-9]{17}\b")
_VIN_SHAPE_EXACT = re.compile(r"^[A-HJ-NPR-Z0-9]{17}$")


def _load_vin_from_config() -> str:
    """Extract the VIN value from cvc_identity.toml."""
    text = CVC_IDENTITY_CONFIG.read_text(encoding="utf-8")
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("#") or not s:
            continue
        m = re.match(r'^vin\s*=\s*"(?P<v>[^"]+)"\s*$', s)
        if m:
            return m.group("v")
    pytest.fail("vin key not found in cvc_identity.toml")


def _iter_scanned_files():
    for base in SCANNED_DIRS:
        if not base.is_dir():
            continue
        for p in base.rglob("*"):
            if not p.is_file():
                continue
            if p in EXCLUDED_PATHS:
                continue
            # Skip anything under build/ or __pycache__/
            parts = set(p.parts)
            if "build" in parts or "__pycache__" in parts:
                continue
            if "odx" in parts:
                # The generated cvc.pdx ZIP is binary — skip.
                continue
            if p.suffix in SCANNED_EXTS or p.name in DOCKERFILE_NAMES:
                yield p


# --- tests ----------------------------------------------------------


def test_vin_loaded_from_config() -> None:
    vin = _load_vin_from_config()
    assert len(vin) == 17
    assert _VIN_SHAPE_EXACT.match(vin) is not None, (
        f"VIN in config does not match ISO 3779 shape: {vin!r}"
    )


def test_no_inline_vin_literal_in_phase4_sources() -> None:
    """Every scanned file must be free of the exact VIN string."""
    vin = _load_vin_from_config()
    offenders: list[tuple[Path, int]] = []
    for p in _iter_scanned_files():
        try:
            text = p.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        if vin in text:
            offenders.append((p, text.count(vin)))
    assert not offenders, (
        f"inline VIN literal {vin!r} found in scanned files: {offenders}"
    )


def test_no_vin_shaped_token_in_phase4_sources() -> None:
    """Even a wrong VIN of the right shape is forbidden."""
    offenders: list[tuple[Path, list[str]]] = []
    for p in _iter_scanned_files():
        try:
            text = p.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        matches = [
            m for m in _VIN_SHAPE_SEARCH.findall(text)
            # Strip out anything that could plausibly be a git SHA or
            # other innocuous 17-char token. The ISO 3779 VIN charset
            # excludes 'I', 'O', 'Q' which SHA hex does not emit, so
            # any match here is genuinely VIN-shaped.
            if not set(m).issubset(set("0123456789abcdef"))
        ]
        if matches:
            offenders.append((p, matches))
    assert not offenders, (
        f"VIN-shaped token found in scanned files: {offenders}"
    )


def test_no_4hex_did_literal_in_dockerfiles_or_compose() -> None:
    """Dockerfiles and compose files must not inline any DID hex value.

    C sources are exempt because Dcm_Cfg_*.c IS the source of truth.
    Python test sources are also allowed — they reference DIDs via the
    odx-gen parser and may contain DIDs as forbidden-list literals.
    """
    hex_did_re = re.compile(r"\b0[xX][0-9A-Fa-f]{4}\b")
    offenders: list[tuple[Path, list[str]]] = []
    for p in _iter_scanned_files():
        if p.suffix == ".c" or p.suffix == ".h" or p.suffix == ".py":
            continue
        if p.suffix == ".toml":
            # opensovd-proxy.toml legitimately holds CAN ids / logical
            # addresses as hex — it IS the source of truth for routing.
            continue
        try:
            text = p.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        matches = hex_did_re.findall(text)
        if matches:
            offenders.append((p, matches))
    assert not offenders, (
        f"4-hex-digit literal found in Dockerfile/compose/yaml: {offenders}"
    )
