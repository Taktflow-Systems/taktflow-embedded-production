"""Loader for the ISO 15031-6 / SAE J2012 generic DTC subset.

All DTC codes and descriptions live in `data/iso_15031_6_dtcs.yaml`.
This module contains NO DTC literals — every value is read from the YAML
file at load time. This matches the style of `parsers/dcm_cfg.py`, where
the parser is pure structure and the data lives in a source artifact.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Optional

import yaml


_DATA_FILENAME = "iso_15031_6_dtcs.yaml"


@dataclass
class GenericDtc:
    """A single generic OBD-II DTC entry loaded from the YAML table."""

    code: str
    category: str
    description: str

    @property
    def numeric_code(self) -> int:
        """Convert textual DTC to the 16-bit encoded integer form.

        ISO 15031-6 encodes the letter prefix in the top two bits of the
        first byte:
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


@dataclass
class Iso15031DtcTable:
    """Parsed content of `iso_15031_6_dtcs.yaml`."""

    version: str
    standard: str
    dtcs: list[GenericDtc]
    source_path: str


def _default_data_path() -> Path:
    return Path(__file__).resolve().parent / "data" / _DATA_FILENAME


def load_iso_15031_6_dtcs(path: Optional[str | Path] = None) -> Iso15031DtcTable:
    """Load the ISO 15031-6 DTC subset from YAML.

    Args:
        path: Optional override for the YAML file location. Defaults to
            the package-bundled `data/iso_15031_6_dtcs.yaml`.

    Returns:
        Iso15031DtcTable populated from the YAML content.
    """
    p = Path(path) if path is not None else _default_data_path()
    if not p.is_file():
        raise FileNotFoundError(f"ISO 15031-6 DTC table not found: {p}")

    with p.open("r", encoding="utf-8") as fh:
        doc = yaml.safe_load(fh) or {}

    raw_dtcs = doc.get("dtcs") or []
    dtcs: list[GenericDtc] = []
    for entry in raw_dtcs:
        code = str(entry.get("code", "")).strip()
        if not code:
            continue
        dtcs.append(
            GenericDtc(
                code=code,
                category=str(entry.get("category", "")).strip(),
                description=str(entry.get("description", "")).strip(),
            )
        )

    return Iso15031DtcTable(
        version=str(doc.get("version", "")),
        standard=str(doc.get("standard", "")),
        dtcs=dtcs,
        source_path=str(p),
    )
