"""Dataclasses describing the diagnostic capabilities of an ECU.

These types are pure structure — no values are embedded here. They are
populated at runtime by the parsers in `odx_gen.parsers`.
"""

from __future__ import annotations

from dataclasses import dataclass, field, asdict
from typing import Optional


@dataclass
class DidEntry:
    """A single Data Identifier entry, parsed from a Dcm_Cfg_<Ecu>.c row."""

    did_id: int                # 16-bit DID, parsed from a hex literal
    name: str                  # parsed from trailing /* comment */, may be ""
    callback: str              # C function name of the read callback
    length_bytes: int          # data length in bytes
    source_file: str           # absolute path to the parsed Dcm_Cfg_*.c
    source_line: int           # 1-based line number in source_file


@dataclass
class ServiceEntry:
    """A UDS service id implemented by Dcm.c."""

    sid: int                   # service id, parsed from a #define DCM_SID_*
    name: str                  # canonical UDS service name
    source_file: str           # absolute path to Dcm.c (or Dcm.h for SID macro)
    source_line: int


@dataclass
class DtcEntry:
    """A Diagnostic Trouble Code, parsed from Dem_Cfg_<Ecu>.c."""

    dtc_code: int
    event_id: int
    severity: str
    name: str


@dataclass
class EcuDiagnosticModel:
    """Aggregate of everything a parser pipeline can extract for one ECU."""

    ecu_name: str
    dids: list[DidEntry] = field(default_factory=list)
    services: list[ServiceEntry] = field(default_factory=list)
    dtcs: list[DtcEntry] = field(default_factory=list)

    tx_pdu_id: Optional[int] = None
    tx_pdu_id_symbol: Optional[str] = None
    rx_pdu_id: Optional[int] = None
    rx_pdu_id_symbol: Optional[str] = None
    s3_timeout_ms: Optional[int] = None

    unresolved_todos: list[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        return asdict(self)
