"""Interop test: a third-party DTC catalog must validate against our schema.

The point of `odx_gen.schemas.dtc_catalog` is that it is a contract,
not a private format. Any vendor that authors a catalog in the
documented shape must be loadable by `load_dtc_catalog()` without
touching odx-gen source.

This test simulates a fictional "Acme Vendor" catalog written from
scratch (no copy of our YAML), proves it validates and loads, proves
it interoperates with the legacy `Iso15031DtcTable` adapter, and
proves a deliberately-broken Acme catalog is still rejected.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from odx_gen.iso_15031_6_dtcs import (
    GenericDtc,
    Iso15031DtcTable,
    load_iso_15031_6_dtcs,
)
from odx_gen.schemas.dtc_catalog import (
    DtcCatalog,
    DtcCatalogValidationError,
    load_dtc_catalog,
)


_ACME_GOOD_YAML = """\
version: "2025.07-acme-rev3"
source: "Acme Vendor diagnostic specification document AVD-DTC-001"
standard: "Acme proprietary, OBD-II compatible"
dtcs:
  - code: "P1A01"
    category: powertrain
    description: "Acme propulsion controller self-test failure"
  - code: "B1F22"
    category: body
    description: "Acme cabin climate node CAN silence"
  - code: "C2A30"
    category: chassis
    description: "Acme adaptive damper out-of-range"
  - code: "U0073"
    category: network
    description: "Control module communication bus A off"
"""


_ACME_BAD_YAML = """\
version: "2025.07-acme-rev3"
source: "Acme Vendor diagnostic specification document AVD-DTC-001"
dtcs:
  - code: "Z9999"
    category: powertrain
    description: "Not a valid OBD-II prefix"
"""


def test_acme_catalog_loads_via_schema_loader(tmp_path: Path) -> None:
    p = tmp_path / "acme_dtcs.yaml"
    p.write_text(_ACME_GOOD_YAML, encoding="utf-8")

    catalog = load_dtc_catalog(p)
    assert isinstance(catalog, DtcCatalog)
    assert catalog.version == "2025.07-acme-rev3"
    assert "Acme" in catalog.source
    assert len(catalog.entries) == 4
    assert {e.code for e in catalog.entries} == {"P1A01", "B1F22", "C2A30", "U0073"}
    assert {e.category for e in catalog.entries} == {
        "powertrain",
        "body",
        "chassis",
        "network",
    }


def test_acme_catalog_loads_via_legacy_adapter(tmp_path: Path) -> None:
    """An external vendor catalog drives the legacy builder code path."""
    p = tmp_path / "acme_dtcs.yaml"
    p.write_text(_ACME_GOOD_YAML, encoding="utf-8")

    table = load_iso_15031_6_dtcs(p)
    assert isinstance(table, Iso15031DtcTable)
    assert table.version == "2025.07-acme-rev3"
    assert len(table.dtcs) == 4
    for entry in table.dtcs:
        assert isinstance(entry, GenericDtc)
        # Numeric encoding still works for non-P0xxx codes
        n = entry.numeric_code
        assert isinstance(n, int)
        assert n >= 0


def test_acme_bad_catalog_is_rejected(tmp_path: Path) -> None:
    """A vendor catalog with an invalid code prefix must fail validation."""
    p = tmp_path / "acme_bad.yaml"
    p.write_text(_ACME_BAD_YAML, encoding="utf-8")

    with pytest.raises(DtcCatalogValidationError) as excinfo:
        load_dtc_catalog(p)

    msg = str(excinfo.value)
    assert "Z9999" in msg or "pattern" in msg or "code" in msg
    assert excinfo.value.source_path == str(p)


def test_acme_catalog_codes_are_iso_15031_6_prefixed(tmp_path: Path) -> None:
    """The schema's prefix constraint accepts P/B/C/U for any vendor."""
    p = tmp_path / "acme_dtcs.yaml"
    p.write_text(_ACME_GOOD_YAML, encoding="utf-8")
    catalog = load_dtc_catalog(p)
    for entry in catalog.entries:
        assert entry.code[0] in {"P", "B", "C", "U"}


def test_schema_is_a_genuine_contract_not_just_the_shipped_yaml(tmp_path: Path) -> None:
    """The shipped YAML and the Acme YAML share zero codes but both validate.

    This is the actual interop guarantee: the schema is a structural
    contract, not a hardcoded list of approved DTCs.
    """
    p = tmp_path / "acme_dtcs.yaml"
    p.write_text(_ACME_GOOD_YAML, encoding="utf-8")

    shipped = load_iso_15031_6_dtcs()  # bundled YAML
    acme = load_dtc_catalog(p)

    shipped_codes = {d.code for d in shipped.dtcs}
    acme_codes = {e.code for e in acme.entries}
    assert shipped_codes.isdisjoint(acme_codes), (
        "test fixture would invalidate this assertion if it ever overlapped "
        "with the shipped P0xxx range"
    )
    assert len(shipped_codes) > 0
    assert len(acme_codes) == 4
