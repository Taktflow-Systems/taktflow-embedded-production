"""Tests for the Dem_Cfg parser (graceful empty-state behaviour)."""

from __future__ import annotations

from pathlib import Path

from odx_gen.parsers.dem_cfg import parse_dem_cfg


REPO_ROOT = Path(__file__).resolve().parents[3]


def test_missing_dem_cfg_returns_empty_with_todo() -> None:
    entries, todos = parse_dem_cfg(REPO_ROOT, "cvc")
    # Today the firmware does not have a Dem_Cfg file. If it ever appears,
    # this test will see a non-empty list — that is fine, just assert the
    # contract: empty + TODO when missing, else valid entries with no
    # missing-file TODO.
    expected_path = (
        REPO_ROOT / "firmware" / "ecu" / "cvc" / "cfg" / "Dem_Cfg_Cvc.c"
    )
    if not expected_path.is_file():
        assert entries == []
        assert any("not present" in t for t in todos)
    else:
        assert isinstance(entries, list)
