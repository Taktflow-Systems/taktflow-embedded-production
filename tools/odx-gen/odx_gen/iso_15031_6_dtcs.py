"""Loader for the ISO 15031-6 / SAE J2012 generic DTC subset.

All DTC codes and descriptions live in `data/iso_15031_6_dtcs.yaml`.
This module contains NO DTC literals - every value is read from the
YAML file at load time.

As of the schema migration, this module is a thin compatibility shim
over `odx_gen.schemas.dtc_catalog.load_dtc_catalog`. The shim keeps
the legacy `GenericDtc` / `Iso15031DtcTable` types and the
`load_iso_15031_6_dtcs()` entry point so existing callers
(builders, tests) continue to work unchanged. New code should call
`load_dtc_catalog()` directly.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from .schemas.dtc_catalog import (
    DtcCatalog,
    DtcEntry,
    load_dtc_catalog,
)


_DATA_FILENAME = "iso_15031_6_dtcs.yaml"


@dataclass
class GenericDtc:
    """Legacy alias for a single generic OBD-II DTC entry.

    Identical fields to `odx_gen.schemas.dtc_catalog.DtcEntry`.
    Kept as a separate (mutable) dataclass for backward compatibility
    with callers that may construct `GenericDtc` directly.
    """

    code: str
    category: str
    description: str

    @property
    def numeric_code(self) -> int:
        """Convert textual DTC to the 16-bit encoded integer form.

        ISO 15031-6 encodes the letter prefix in the top two bits of
        the first byte:
            P -> 0b00
            C -> 0b01
            B -> 0b10
            U -> 0b11
        The remaining 14 bits are the BCD/hex digits that follow.
        """
        letter = self.code[0].upper()
        prefix_bits = {"P": 0b00, "C": 0b01, "B": 0b10, "U": 0b11}[letter]
        rest = int(self.code[1:], 16)
        return (prefix_bits << 14) | rest

    @classmethod
    def from_entry(cls, entry: DtcEntry) -> "GenericDtc":
        return cls(
            code=entry.code,
            category=entry.category,
            description=entry.description,
        )


@dataclass
class Iso15031DtcTable:
    """Legacy view of the parsed DTC catalog.

    Wraps `DtcCatalog` so existing builder code can keep using
    `.dtcs`, `.version`, `.standard`, `.source_path`. New code should
    use `DtcCatalog` directly.
    """

    version: str
    standard: str
    dtcs: list[GenericDtc]
    source_path: str

    @classmethod
    def from_catalog(cls, catalog: DtcCatalog) -> "Iso15031DtcTable":
        return cls(
            version=catalog.version,
            standard=catalog.standard,
            dtcs=[GenericDtc.from_entry(e) for e in catalog.entries],
            source_path=catalog.source_path,
        )


def _default_data_path() -> Path:
    return Path(__file__).resolve().parent / "data" / _DATA_FILENAME


def load_iso_15031_6_dtcs(path: Optional[str | Path] = None) -> Iso15031DtcTable:
    """Load the ISO 15031-6 DTC subset from YAML.

    Delegates to the schema-validated `load_dtc_catalog()` loader and
    adapts the result back to the legacy `Iso15031DtcTable` shape.

    Args:
        path: Optional override for the YAML file location. Defaults
            to the package-bundled `data/iso_15031_6_dtcs.yaml`.

    Returns:
        Iso15031DtcTable populated from the validated YAML content.

    Raises:
        FileNotFoundError: if the file is missing.
        odx_gen.schemas.dtc_catalog.DtcCatalogValidationError:
            if the file content does not match the catalog schema.
    """
    p = Path(path) if path is not None else _default_data_path()
    catalog = load_dtc_catalog(p)
    return Iso15031DtcTable.from_catalog(catalog)
