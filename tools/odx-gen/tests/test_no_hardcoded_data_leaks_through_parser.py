"""Regression guard: no hardcoded ECU / DBC / DTC data in parser or builder.

Scans every source file that participates in input ingestion or PDX
construction and asserts that no forbidden literal patterns appear in
the source text. All data must live in YAML / config / input files.

Covered files:
  * odx_gen/parsers/dcm_cfg.py       (existing Dcm_Cfg path)
  * odx_gen/parsers/dbc.py           (new DBC path)
  * odx_gen/build_from_dbc.py        (new DBC->PDX builder)
  * odx_gen/iso_15031_6_dtcs.py      (generic DTC table loader)

Forbidden patterns:
  * 4-digit-or-longer hex literals (anything matching 0x[0-9A-Fa-f]{4,})
    — these usually indicate a DID, DTC, or PDU id baked into source
  * DTC strings matching P0xxx / B0xxx / C0xxx / U0xxx
  * A known-suspicious set of signal names that could only have come
    from a specific DBC (guards against accidental vendor leakage)

This complements the existing Dcm_Cfg-specific check in
`test_parser_dcm_cfg.py::test_no_hardcoded_data_leaks_through_parser`.
"""

from __future__ import annotations

import re
from pathlib import Path

import pytest


_PKG_ROOT = Path(__file__).resolve().parents[1] / "odx_gen"

_SCANNED_FILES: list[Path] = [
    _PKG_ROOT / "parsers" / "dcm_cfg.py",
    _PKG_ROOT / "parsers" / "dbc.py",
    _PKG_ROOT / "build_from_dbc.py",
    _PKG_ROOT / "iso_15031_6_dtcs.py",
    _PKG_ROOT / "schemas" / "dtc_catalog.py",
]


# 4-digit (or longer) hex literal — DIDs and PDUs look like this
_HEX4_RE = re.compile(r"0x[0-9A-Fa-f]{4,}")

# Generic OBD-II DTC string
_DTC_RE = re.compile(r"\b[PBCU]0[0-9]{3}\b")

# DBC-specific signal names that would only appear if someone pasted
# DBC content directly into the builder. None of these should ever
# show up in our Python source.
_FORBIDDEN_SIGNAL_NAMES = [
    "YAW_RATE",
    "STEER_ANGLE",
    "STEER_TORQUE",
    "WHEEL_SPEED_FR",
    "WHEEL_SPEED_FL",
    "WHEEL_SPEED_RR",
    "WHEEL_SPEED_RL",
    "BRAKE_PRESSURE",
    "THROTTLE_CMD",
    "ACC_CONTROL",
    "PCM_CRUISE",
    "KINEMATICS",
]


def _strip_comments_and_strings(src: str) -> str:
    """Best-effort strip of Python comments and string literals.

    We want the regex to fail on genuine literal values embedded in
    code expressions, not on docstrings or comments that legitimately
    mention example DTC codes or signal names.
    """
    # Drop triple-quoted strings (both ''' and \"\"\")
    src = re.sub(r'"""[\s\S]*?"""', '""', src)
    src = re.sub(r"'''[\s\S]*?'''", "''", src)
    # Drop single-line '...' and "..." strings
    src = re.sub(r'"(?:\\.|[^"\\\n])*"', '""', src)
    src = re.sub(r"'(?:\\.|[^'\\\n])*'", "''", src)
    # Drop line comments
    src = re.sub(r"#[^\n]*", "", src)
    return src


@pytest.mark.parametrize("source_file", _SCANNED_FILES, ids=lambda p: p.name)
def test_no_four_digit_hex_literals(source_file: Path) -> None:
    assert source_file.is_file(), f"missing expected source file: {source_file}"
    src = _strip_comments_and_strings(source_file.read_text(encoding="utf-8"))
    matches = _HEX4_RE.findall(src)
    assert not matches, (
        f"{source_file.name}: 4+ digit hex literal(s) leaked into source: "
        f"{matches}. These belong in YAML/config, not Python."
    )


@pytest.mark.parametrize("source_file", _SCANNED_FILES, ids=lambda p: p.name)
def test_no_dtc_code_literals(source_file: Path) -> None:
    src = _strip_comments_and_strings(source_file.read_text(encoding="utf-8"))
    matches = _DTC_RE.findall(src)
    assert not matches, (
        f"{source_file.name}: DTC literal(s) leaked into source: {matches}. "
        f"DTCs must come from iso_15031_6_dtcs.yaml."
    )


@pytest.mark.parametrize("source_file", _SCANNED_FILES, ids=lambda p: p.name)
def test_no_dbc_signal_name_literals(source_file: Path) -> None:
    src = source_file.read_text(encoding="utf-8")
    leaked = [name for name in _FORBIDDEN_SIGNAL_NAMES if name in src]
    assert not leaked, (
        f"{source_file.name}: DBC signal names leaked into source: {leaked}. "
        f"Signal names must come from the DBC file at parse time."
    )
