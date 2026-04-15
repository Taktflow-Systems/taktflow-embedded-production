"""DTC catalog schema and loader.

The DTC catalog is a YAML (or JSON) document that lists generic or
vendor-specific Diagnostic Trouble Codes for use by the ODX builder.
This module is the authoritative contract: every DTC catalog the
toolchain accepts must validate against `DTC_CATALOG_JSON_SCHEMA`.

Schema shape (top level):

    version:  string         e.g. "1.0.0"
    source:   string         human-readable provenance
    standard: string         optional, e.g. "ISO 15031-6 / SAE J2012"
    dtcs:     array of obj   non-empty list of DTC entries

Each DTC entry:

    code:        string  matching ^[PBCU][0-9A-F]{4}$
    category:    string  one of powertrain | body | chassis | network
    description: string  non-empty

The schema is intentionally a plain JSON Schema dict (not pydantic):

  * keeps the dependency surface small (jsonschema is already
    transitively available; pydantic v2 is not in pyproject.toml)
  * lets third-party catalog authors validate their files with
    *any* JSON Schema implementation, not just our Python loader
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable, Mapping, Optional, Union

import yaml
from jsonschema import Draft7Validator
from jsonschema.exceptions import ValidationError as _JsonSchemaValidationError


# --- the schema itself (JSON Schema Draft 7) --------------------------


DTC_CATALOG_JSON_SCHEMA: dict[str, Any] = {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "title": "odx-gen DTC Catalog",
    "type": "object",
    "additionalProperties": True,  # tolerate forward-compat fields
    "required": ["version", "source", "dtcs"],
    "properties": {
        "version": {
            "type": "string",
            "minLength": 1,
        },
        "source": {
            "type": "string",
            "minLength": 1,
        },
        "standard": {
            "type": "string",
        },
        "dtcs": {
            "type": "array",
            "minItems": 1,
            "items": {
                "type": "object",
                "additionalProperties": True,
                "required": ["code", "category", "description"],
                "properties": {
                    "code": {
                        "type": "string",
                        "pattern": r"^[PBCU][0-9A-F]{4}$",
                    },
                    "category": {
                        "type": "string",
                        "enum": [
                            "powertrain",
                            "body",
                            "chassis",
                            "network",
                        ],
                    },
                    "description": {
                        "type": "string",
                        "minLength": 1,
                    },
                },
            },
        },
    },
}


# --- typed in-memory model -------------------------------------------


class DtcCatalogValidationError(ValueError):
    """Raised when a DTC catalog document fails schema validation."""

    def __init__(
        self,
        message: str,
        *,
        source_path: Optional[str] = None,
        errors: Optional[list[str]] = None,
    ) -> None:
        super().__init__(message)
        self.source_path = source_path
        self.errors = errors or []


@dataclass(frozen=True)
class DtcEntry:
    """A single DTC entry within a catalog.

    Field names match the existing `iso_15031_6_dtcs.yaml` shape so
    the loader can be migrated without breaking callers that still
    read `code` / `category` / `description`.
    """

    code: str
    category: str
    description: str

    @property
    def numeric_code(self) -> int:
        """Convert textual DTC to the 16-bit ISO 15031-6 encoded integer."""
        letter = self.code[0].upper()
        prefix_bits = {"P": 0b00, "C": 0b01, "B": 0b10, "U": 0b11}[letter]
        rest = int(self.code[1:], 16)
        return (prefix_bits << 14) | rest


@dataclass(frozen=True)
class DtcCatalog:
    """Parsed and validated DTC catalog."""

    version: str
    source: str
    entries: tuple[DtcEntry, ...]
    standard: str = ""
    source_path: str = ""

    def __iter__(self) -> Iterable[DtcEntry]:
        return iter(self.entries)

    def __len__(self) -> int:
        return len(self.entries)

    @property
    def codes(self) -> tuple[str, ...]:
        return tuple(e.code for e in self.entries)


# --- validation helpers -----------------------------------------------


def _format_errors(errors: list[_JsonSchemaValidationError]) -> list[str]:
    out: list[str] = []
    for err in errors:
        loc = "/".join(str(p) for p in err.absolute_path) or "<root>"
        out.append(f"{loc}: {err.message}")
    return out


def validate_dtc_catalog_doc(
    doc: Any,
    *,
    source_path: str = "",
) -> DtcCatalog:
    """Validate an in-memory document and return a typed `DtcCatalog`.

    Args:
        doc: Parsed YAML/JSON content (typically a dict).
        source_path: Optional provenance for error messages.

    Raises:
        DtcCatalogValidationError: if the document fails the schema.
    """
    if not isinstance(doc, Mapping):
        raise DtcCatalogValidationError(
            f"DTC catalog must be a mapping at top level, got {type(doc).__name__}",
            source_path=source_path or None,
        )

    validator = Draft7Validator(DTC_CATALOG_JSON_SCHEMA)
    errors = sorted(validator.iter_errors(doc), key=lambda e: list(e.absolute_path))
    if errors:
        formatted = _format_errors(errors)
        raise DtcCatalogValidationError(
            f"DTC catalog at {source_path or '<memory>'} failed validation: "
            + "; ".join(formatted),
            source_path=source_path or None,
            errors=formatted,
        )

    entries = tuple(
        DtcEntry(
            code=str(e["code"]),
            category=str(e["category"]),
            description=str(e["description"]),
        )
        for e in doc["dtcs"]
    )
    return DtcCatalog(
        version=str(doc["version"]),
        source=str(doc["source"]),
        entries=entries,
        standard=str(doc.get("standard", "")),
        source_path=source_path,
    )


def load_dtc_catalog(path: Union[str, Path]) -> DtcCatalog:
    """Load and validate a DTC catalog from a YAML or JSON file.

    Args:
        path: Filesystem path to a YAML/JSON DTC catalog.

    Raises:
        FileNotFoundError: if `path` does not exist.
        DtcCatalogValidationError: if the file content fails the schema.
    """
    p = Path(path)
    if not p.is_file():
        raise FileNotFoundError(f"DTC catalog not found: {p}")

    with p.open("r", encoding="utf-8") as fh:
        try:
            doc = yaml.safe_load(fh)
        except yaml.YAMLError as exc:
            raise DtcCatalogValidationError(
                f"DTC catalog at {p} is not valid YAML/JSON: {exc}",
                source_path=str(p),
            ) from exc

    return validate_dtc_catalog_doc(doc or {}, source_path=str(p))
