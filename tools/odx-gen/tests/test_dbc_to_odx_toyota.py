"""End-to-end test for the DBC ingestion path.

Runs build_from_dbc against a real opendbc DBC (read-only reference at
H:/eclipse-opensovd/external/opendbc/...) and asserts that the resulting
PDX is valid, contains DIDs, services, and DTCs, and that the DTC set
matches the ISO 15031-6 YAML.

If the opendbc path is unavailable, the test is skipped — we do NOT
vendor opendbc into the taktflow-embedded-production repo.
"""

from __future__ import annotations

from pathlib import Path

import pytest

import odxtools

from odx_gen.build_from_dbc import build_database_from_dbc, write_pdx_from_dbc
from odx_gen.iso_15031_6_dtcs import load_iso_15031_6_dtcs


TOYOTA_PRIUS_DBC = Path(
    r"H:/eclipse-opensovd/external/opendbc/opendbc/dbc/toyota_prius_2010_pt.dbc"
)


pytestmark = pytest.mark.skipif(
    not TOYOTA_PRIUS_DBC.is_file(),
    reason=f"opendbc reference not available at {TOYOTA_PRIUS_DBC}",
)


@pytest.fixture
def built_pdx(tmp_path: Path) -> Path:
    out = tmp_path / "toyota_prius_2010_pt.pdx"
    written = write_pdx_from_dbc(
        dbc_path=TOYOTA_PRIUS_DBC,
        ecu_name="toyota_prius_2010_pt",
        output_path=out,
    )
    assert written == out
    return out


def test_pdx_file_exists_and_is_nonempty(built_pdx: Path) -> None:
    assert built_pdx.is_file()
    assert built_pdx.stat().st_size > 0


def test_pdx_round_trip_loads(built_pdx: Path) -> None:
    db = odxtools.load_pdx_file(str(built_pdx))
    containers = list(db.diag_layer_containers)
    assert len(containers) == 1


def test_pdx_has_dids_and_services(built_pdx: Path) -> None:
    db = odxtools.load_pdx_file(str(built_pdx))
    total_services = 0
    total_requests = 0
    for dlc in db.diag_layer_containers:
        for bv in dlc.base_variants:
            total_services += len(list(bv.services))
            total_requests += len(list(bv.requests))
    assert total_services > 0
    assert total_requests > 0


def test_builder_reports_did_count(tmp_path: Path) -> None:
    db = build_database_from_dbc(
        dbc_path=TOYOTA_PRIUS_DBC,
        ecu_name="toyota_prius_2010_pt",
    )
    stats = db.dbc_ingestion_stats  # type: ignore[attr-defined]
    assert stats["did_count"] > 0, "no DIDs emitted from Toyota DBC"
    assert stats["service_count"] >= stats["did_count"]
    assert stats["eligible_signal_count"] >= stats["did_count"]


def test_dtc_set_matches_iso_15031_6_yaml(tmp_path: Path) -> None:
    db = build_database_from_dbc(
        dbc_path=TOYOTA_PRIUS_DBC,
        ecu_name="toyota_prius_2010_pt",
    )
    stats = db.dbc_ingestion_stats  # type: ignore[attr-defined]

    yaml_table = load_iso_15031_6_dtcs()
    expected_codes = {d.code for d in yaml_table.dtcs}
    got_codes = set(stats["dtc_codes"])

    assert stats["dtc_count"] > 0
    assert got_codes == expected_codes
    assert stats["dtc_table_source"].endswith("iso_15031_6_dtcs.yaml")
