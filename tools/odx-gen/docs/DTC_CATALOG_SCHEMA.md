# DTC Catalog Schema

`odx_gen.schemas.dtc_catalog` defines the contract that every DTC
catalog consumed by `odx-gen` must obey. The schema is a JSON Schema
Draft 7 document, exposed both programmatically
(`DTC_CATALOG_JSON_SCHEMA`) and as the validation engine inside
`load_dtc_catalog()`.

This document describes the shape, validation rules, and intended
use of the catalog format.

## Why a schema

Before the schema, the loader read whatever fields happened to be
in `iso_15031_6_dtcs.yaml` and silently ignored anything missing.
That made the catalog format an *implicit* contract:

  * external authors had no way to know what fields were required
  * malformed catalogs produced empty / partial DTC tables instead
    of failing loudly
  * the loader could not be reused for vendor-specific catalogs
    without copy-pasting and praying

Pinning the shape into a JSON Schema turns the catalog into an
*interop contract*. Any tool that can read JSON Schema can validate
a catalog without depending on `odx-gen` source.

## File format

A DTC catalog is a YAML or JSON document with a single object at
the top level. The shipped format is YAML; JSON works equivalently
because JSON is a strict subset of YAML.

### Top-level fields

| field      | type   | required | meaning |
|------------|--------|----------|---------|
| `version`  | string | yes      | catalog format version, e.g. `"1.0.0"` |
| `source`   | string | yes      | human-readable provenance / authoring note |
| `standard` | string | no       | name of the standard the catalog follows |
| `dtcs`     | array  | yes      | non-empty list of DTC entries |

Additional top-level fields are allowed (for forward compatibility)
and are ignored by the loader.

### DTC entry fields

| field         | type   | required | meaning |
|---------------|--------|----------|---------|
| `code`        | string | yes      | DTC string matching `^[PBCU][0-9A-F]{4}$` |
| `category`    | string | yes      | one of `powertrain`, `body`, `chassis`, `network` |
| `description` | string | yes      | non-empty human-readable description |

Additional per-entry fields are allowed and ignored.

### Code prefix meaning (ISO 15031-6)

| prefix | category bit pattern | meaning      |
|--------|----------------------|--------------|
| `P`    | `0b00`               | powertrain   |
| `C`    | `0b01`               | chassis      |
| `B`    | `0b10`               | body         |
| `U`    | `0b11`               | network      |

The `numeric_code` property on `DtcEntry` (and `GenericDtc`) returns
the 16-bit ISO encoding: top two bits from the prefix, low 14 bits
from the hex digits.

## Example: shipped ISO 15031-6 catalog

```yaml
version: "1.0.0"
source: "ISO 15031-6 generic OBD-II P-codes (authored 2026-04-14)"
standard: "ISO 15031-6 / SAE J2012"
dtcs:
  - { code: "P0100", category: powertrain, description: "Mass or Volume Air Flow Circuit Malfunction" }
  - { code: "P0101", category: powertrain, description: "Mass or Volume Air Flow Circuit Range/Performance Problem" }
  ...
```

## Example: third-party vendor catalog

A vendor can author a catalog from scratch as long as it satisfies
the schema. Vendor codes can use any P / B / C / U prefix and any
hex digits; the schema does not allowlist specific code numbers.

```yaml
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
```

## CLI

Validate a catalog from the command line:

```
python -m odx_gen validate-dtc-catalog path/to/catalog.yaml
```

Exit codes:

| code | meaning                                              |
|------|------------------------------------------------------|
| `0`  | file exists and validates                            |
| `2`  | usage error (missing path, no such file)             |
| `3`  | schema validation failure (errors written to stderr) |

The `validate-dtc-catalog` subcommand has no `odxtools` dependency,
so it runs in any Python environment that has `pyyaml` and
`jsonschema`.

## Programmatic API

```python
from odx_gen.schemas.dtc_catalog import (
    DTC_CATALOG_JSON_SCHEMA,
    DtcCatalog,
    DtcEntry,
    DtcCatalogValidationError,
    load_dtc_catalog,
    validate_dtc_catalog_doc,
)

# Load + validate from a file
catalog: DtcCatalog = load_dtc_catalog("path/to/catalog.yaml")
print(catalog.version, catalog.source, len(catalog.entries))
for entry in catalog:
    print(entry.code, entry.category, entry.description)

# Validate a parsed dict directly
doc = {
    "version": "1.0.0",
    "source": "in-memory",
    "dtcs": [
        {"code": "P0100", "category": "powertrain", "description": "x"},
    ],
}
catalog = validate_dtc_catalog_doc(doc, source_path="<memory>")
```

`DtcCatalogValidationError` carries:

  * `args[0]` - human-readable summary
  * `source_path` - the file path or `"<memory>"`
  * `errors` - list of `<location>: <message>` strings, one per
    validation failure

## Backward compatibility

The legacy `odx_gen.iso_15031_6_dtcs` module is now a compatibility
shim:

  * `GenericDtc` is preserved with the same fields (`code`,
    `category`, `description`) and the same `numeric_code` property
  * `Iso15031DtcTable` is preserved with `version`, `standard`,
    `dtcs`, `source_path`
  * `load_iso_15031_6_dtcs(path)` delegates to
    `load_dtc_catalog(path)` and adapts the result back to the
    legacy shape

Existing builders (`build_from_dbc.py`) and tests
(`test_dbc_to_odx_toyota.py`) work unchanged. New code should call
`load_dtc_catalog()` directly.

## Hardcoded-literal regression

`tests/test_no_hardcoded_data_leaks_through_parser.py` scans every
ingestion-path Python module - including `schemas/dtc_catalog.py` -
for forbidden literal patterns:

  * 4+ digit hex literals (DIDs / PDU ids)
  * generic OBD-II DTC strings (`P0xxx`, `B0xxx`, `C0xxx`, `U0xxx`)
  * known DBC signal names from public reference DBCs

The schema module hosts structural rules and patterns only - never
real DTC, DID, or signal data.
