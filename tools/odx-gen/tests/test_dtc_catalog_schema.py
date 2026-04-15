"""Schema validation tests for the generic DTC catalog.

The DTC catalog YAML (`odx_gen/data/iso_15031_6_dtcs.yaml`) is the
authoritative source of generic OBD-II DTCs used by the DBC ingestion
path. Until now its shape was implicit — the loader read whatever
fields happened to be there. This test pins the shape down so that:

  1. The shipped YAML always validates against the schema.
  2. Third-party catalogs can be authored against the same contract.
  3. Malformed inputs are rejected with a clear error.

The schema lives in `odx_gen/schemas/dtc_catalog.py` and is exposed
via `load_dtc_catalog()` plus the `DtcEntry` / `DtcCatalog` types.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from odx_gen.schemas.dtc_catalog import (
    DTC_CATALOG_JSON_SCHEMA,
    DtcCatalog,
    DtcCatalogValidationError,
    DtcEntry,
    load_dtc_catalog,
    validate_dtc_catalog_doc,
)


_PKG_DATA = Path(__file__).resolve().parents[1] / "odx_gen" / "data"
_SHIPPED_YAML = _PKG_DATA / "iso_15031_6_dtcs.yaml"


# --- schema object -----------------------------------------------------


def test_schema_is_a_dict_with_required_top_level() -> None:
    """The schema is exposed as a JSON Schema dict, not an opaque object."""
    assert isinstance(DTC_CATALOG_JSON_SCHEMA, dict)
    assert DTC_CATALOG_JSON_SCHEMA.get("type") == "object"
    required = set(DTC_CATALOG_JSON_SCHEMA.get("required", []))
    assert {"version", "source", "dtcs"}.issubset(required), (
        f"schema must require version, source, dtcs; got required={required}"
    )


def test_schema_dtc_entry_requires_code_category_description() -> None:
    """Each DTC entry must have code, category, description."""
    props = DTC_CATALOG_JSON_SCHEMA["properties"]
    items = props["dtcs"]["items"]
    required = set(items.get("required", []))
    assert {"code", "category", "description"}.issubset(required)


# --- shipped catalog validates ------------------------------------------


def test_shipped_iso_15031_6_yaml_validates_against_schema() -> None:
    """The bundled YAML must be a valid DtcCatalog under the new schema."""
    assert _SHIPPED_YAML.is_file(), f"missing shipped YAML at {_SHIPPED_YAML}"
    catalog = load_dtc_catalog(_SHIPPED_YAML)
    assert isinstance(catalog, DtcCatalog)
    assert catalog.version  # non-empty
    assert catalog.source  # non-empty
    assert len(catalog.entries) > 0
    for entry in catalog.entries:
        assert isinstance(entry, DtcEntry)
        assert entry.code
        assert entry.category
        assert entry.description


def test_shipped_yaml_dtc_codes_match_iso_pattern() -> None:
    """Every shipped code is a valid ISO 15031-6 P/B/C/U style code."""
    import re

    pattern = re.compile(r"^[PBCU][0-9A-F]{4}$")
    catalog = load_dtc_catalog(_SHIPPED_YAML)
    for entry in catalog.entries:
        assert pattern.match(entry.code), f"bad code: {entry.code}"


# --- malformed inputs are rejected -------------------------------------


def test_rejects_missing_version(tmp_path: Path) -> None:
    bad = tmp_path / "no_version.yaml"
    bad.write_text(
        'source: "test"\n'
        'dtcs:\n'
        '  - { code: "P0100", category: powertrain, description: "x" }\n',
        encoding="utf-8",
    )
    with pytest.raises(DtcCatalogValidationError):
        load_dtc_catalog(bad)


def test_rejects_missing_source(tmp_path: Path) -> None:
    bad = tmp_path / "no_source.yaml"
    bad.write_text(
        'version: "1.0.0"\n'
        'dtcs:\n'
        '  - { code: "P0100", category: powertrain, description: "x" }\n',
        encoding="utf-8",
    )
    with pytest.raises(DtcCatalogValidationError):
        load_dtc_catalog(bad)


def test_rejects_missing_dtcs(tmp_path: Path) -> None:
    bad = tmp_path / "no_dtcs.yaml"
    bad.write_text(
        'version: "1.0.0"\nsource: "test"\n',
        encoding="utf-8",
    )
    with pytest.raises(DtcCatalogValidationError):
        load_dtc_catalog(bad)


def test_rejects_dtc_entry_missing_code(tmp_path: Path) -> None:
    bad = tmp_path / "no_code.yaml"
    bad.write_text(
        'version: "1.0.0"\nsource: "test"\n'
        'dtcs:\n'
        '  - { category: powertrain, description: "x" }\n',
        encoding="utf-8",
    )
    with pytest.raises(DtcCatalogValidationError):
        load_dtc_catalog(bad)


def test_rejects_dtc_entry_missing_category(tmp_path: Path) -> None:
    bad = tmp_path / "no_category.yaml"
    bad.write_text(
        'version: "1.0.0"\nsource: "test"\n'
        'dtcs:\n'
        '  - { code: "P0100", description: "x" }\n',
        encoding="utf-8",
    )
    with pytest.raises(DtcCatalogValidationError):
        load_dtc_catalog(bad)


def test_rejects_dtc_entry_missing_description(tmp_path: Path) -> None:
    bad = tmp_path / "no_description.yaml"
    bad.write_text(
        'version: "1.0.0"\nsource: "test"\n'
        'dtcs:\n'
        '  - { code: "P0100", category: powertrain }\n',
        encoding="utf-8",
    )
    with pytest.raises(DtcCatalogValidationError):
        load_dtc_catalog(bad)


def test_rejects_empty_dtcs_list(tmp_path: Path) -> None:
    bad = tmp_path / "empty_dtcs.yaml"
    bad.write_text(
        'version: "1.0.0"\nsource: "test"\ndtcs: []\n',
        encoding="utf-8",
    )
    with pytest.raises(DtcCatalogValidationError):
        load_dtc_catalog(bad)


def test_rejects_dtc_with_invalid_code_pattern(tmp_path: Path) -> None:
    bad = tmp_path / "bad_code.yaml"
    bad.write_text(
        'version: "1.0.0"\nsource: "test"\n'
        'dtcs:\n'
        '  - { code: "X9999", category: powertrain, description: "x" }\n',
        encoding="utf-8",
    )
    with pytest.raises(DtcCatalogValidationError):
        load_dtc_catalog(bad)


def test_rejects_dtc_with_invalid_category(tmp_path: Path) -> None:
    bad = tmp_path / "bad_category.yaml"
    bad.write_text(
        'version: "1.0.0"\nsource: "test"\n'
        'dtcs:\n'
        '  - { code: "P0100", category: "warp_drive", description: "x" }\n',
        encoding="utf-8",
    )
    with pytest.raises(DtcCatalogValidationError):
        load_dtc_catalog(bad)


def test_rejects_non_dict_top_level(tmp_path: Path) -> None:
    bad = tmp_path / "list_top.yaml"
    bad.write_text("- a\n- b\n", encoding="utf-8")
    with pytest.raises(DtcCatalogValidationError):
        load_dtc_catalog(bad)


def test_validate_doc_function_accepts_in_memory_dict() -> None:
    """`validate_dtc_catalog_doc` validates a parsed dict directly."""
    doc = {
        "version": "1.0.0",
        "source": "unit test",
        "dtcs": [
            {"code": "P0100", "category": "powertrain", "description": "x"},
        ],
    }
    catalog = validate_dtc_catalog_doc(doc, source_path="<memory>")
    assert isinstance(catalog, DtcCatalog)
    assert catalog.entries[0].code == "P0100"


def test_validate_doc_function_rejects_bad_in_memory_dict() -> None:
    doc = {"version": "1.0.0", "dtcs": []}  # missing source, empty dtcs
    with pytest.raises(DtcCatalogValidationError):
        validate_dtc_catalog_doc(doc, source_path="<memory>")
