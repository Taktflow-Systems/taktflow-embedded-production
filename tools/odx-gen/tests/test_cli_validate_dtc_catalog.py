"""CLI tests for `python -m odx_gen validate-dtc-catalog`."""

from __future__ import annotations

from pathlib import Path

import pytest

from odx_gen.__main__ import main


_GOOD_YAML = """\
version: "1.0.0"
source: "unit-test"
dtcs:
  - { code: "P0100", category: powertrain, description: "x" }
"""


_BAD_YAML = """\
version: "1.0.0"
dtcs:
  - { code: "P0100", category: powertrain, description: "x" }
"""


def test_cli_validate_good_catalog_returns_zero(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    p = tmp_path / "good.yaml"
    p.write_text(_GOOD_YAML, encoding="utf-8")
    rc = main(["validate-dtc-catalog", str(p)])
    out = capsys.readouterr().out
    assert rc == 0
    assert "OK" in out or "valid" in out.lower()
    assert "1" in out  # one entry


def test_cli_validate_bad_catalog_returns_nonzero(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    p = tmp_path / "bad.yaml"
    p.write_text(_BAD_YAML, encoding="utf-8")
    rc = main(["validate-dtc-catalog", str(p)])
    err = capsys.readouterr().err
    assert rc != 0
    assert "source" in err.lower() or "valid" in err.lower()


def test_cli_validate_missing_path_returns_nonzero(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    p = tmp_path / "does_not_exist.yaml"
    rc = main(["validate-dtc-catalog", str(p)])
    err = capsys.readouterr().err
    assert rc != 0
    assert "not found" in err.lower() or "no such" in err.lower()


def test_cli_validate_no_path_argument_returns_usage_error(
    capsys: pytest.CaptureFixture[str],
) -> None:
    rc = main(["validate-dtc-catalog"])
    err = capsys.readouterr().err
    assert rc != 0
    assert "usage" in err.lower() or "path" in err.lower()


def test_cli_validate_shipped_catalog_returns_zero(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """The bundled iso_15031_6_dtcs.yaml validates via the CLI."""
    shipped = (
        Path(__file__).resolve().parents[1]
        / "odx_gen"
        / "data"
        / "iso_15031_6_dtcs.yaml"
    )
    rc = main(["validate-dtc-catalog", str(shipped)])
    out = capsys.readouterr().out
    assert rc == 0
    assert "OK" in out or "valid" in out.lower()
