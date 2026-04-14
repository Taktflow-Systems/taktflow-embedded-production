"""Tests for the Dcm service table parser.

Cross-checks the parser against an independent re-read of Dcm.h / Dcm.c.
No SID values, names, or counts are hardcoded in the assertions.
"""

from __future__ import annotations

import re
from pathlib import Path

from odx_gen.parsers.dcm_service_table import (
    parse_sid_macros,
    parse_implemented_sids,
    parse_services,
)


REPO_ROOT = Path(__file__).resolve().parents[3]
DCM_H = REPO_ROOT / "firmware" / "bsw" / "services" / "Dcm" / "include" / "Dcm.h"
DCM_C = REPO_ROOT / "firmware" / "bsw" / "services" / "Dcm" / "src" / "Dcm.c"


def _independent_macro_set() -> set[str]:
    text = DCM_H.read_text(encoding="utf-8")
    return set(re.findall(r"#define\s+DCM_SID_([A-Z0-9_]+)\s+0[xX]", text))


def _independent_implemented_set() -> set[str]:
    text = DCM_C.read_text(encoding="utf-8")
    return set(re.findall(r"\bcase\s+DCM_SID_([A-Z0-9_]+)\s*:", text))


def test_macros_extracted_match_independent_grep() -> None:
    assert DCM_H.is_file()
    macros = parse_sid_macros(DCM_H)
    expected = _independent_macro_set()
    assert set(macros.keys()) == expected
    for suffix, (val, line) in macros.items():
        assert val > 0
        assert line > 0


def test_implemented_subset_matches_independent_grep() -> None:
    macros = parse_sid_macros(DCM_H)
    implemented = parse_implemented_sids(DCM_C, macros)
    expected = _independent_implemented_set()
    assert implemented == expected
    # implemented must be a subset of macros
    assert implemented.issubset(set(macros.keys()))


def test_services_have_canonical_names_for_known_suffixes() -> None:
    services, todos = parse_services(DCM_H, DCM_C)
    expected_count = len(_independent_implemented_set())
    assert len(services) == expected_count
    for svc in services:
        assert svc.sid > 0
        assert svc.name != ""
        assert svc.source_file.endswith("Dcm.h")
        assert svc.source_line > 0
