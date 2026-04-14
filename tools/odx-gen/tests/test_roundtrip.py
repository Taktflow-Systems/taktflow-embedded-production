"""End-to-end round-trip tests for discovery -> extract -> build -> read.

All assertions derive expected counts from the parser output at test time.
No specific DID, service, or PDU values are hardcoded.
"""

from __future__ import annotations

import tempfile
from pathlib import Path

import odxtools
import pytest

from odx_gen.build_pdx import write_pdx
from odx_gen.discover import discover_ecus, find_repo_root
from odx_gen.extract import extract_ecu


REPO_ROOT = find_repo_root()


def test_discover_finds_at_least_one_ecu() -> None:
    ecus = discover_ecus(REPO_ROOT)
    assert len(ecus) > 0, "no ECUs discovered under firmware/ecu/"


@pytest.mark.parametrize("ecu_name", discover_ecus(REPO_ROOT))
def test_pipeline_for_each_discovered_ecu(ecu_name: str, tmp_path: Path) -> None:
    """Run extract -> build -> load and cross-check structure counts."""
    model = extract_ecu(REPO_ROOT, ecu_name)

    expected_dids = len(model.dids)
    expected_non_did_services = sum(1 for s in model.services if s.sid != 0x22)
    expected_total_services = expected_dids + expected_non_did_services

    out_path = tmp_path / f"{ecu_name}.pdx"
    write_pdx(model, out_path)
    assert out_path.is_file()
    assert out_path.stat().st_size > 0

    db = odxtools.load_pdx_file(str(out_path))

    found_services = 0
    found_requests = 0
    found_responses = 0
    for dlc in db.diag_layer_containers:
        for bv in dlc.base_variants:
            found_services += len(list(bv.services))
            found_requests += len(list(bv.requests))
            found_responses += len(list(bv.positive_responses))

    print(
        f"\n[{ecu_name}] expected={expected_total_services} services, "
        f"got={found_services}, requests={found_requests}, "
        f"pos_responses={found_responses}, todos={len(model.unresolved_todos)}"
    )

    assert found_services == expected_total_services
    assert found_requests == expected_total_services
    assert found_responses == expected_total_services
