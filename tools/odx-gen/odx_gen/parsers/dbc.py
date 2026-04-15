"""Parser for DBC (CAN network database) files.

Uses the `cantools` library to read DBC files and produce a structured
representation of messages, signals, and nodes. No DBC content is hardcoded
in this module — the parser only reflects whatever cantools returns.

Shape is purposely parser-local (not the `EcuDiagnosticModel` used by the
Dcm_Cfg path) because DBC is a CAN-bus matrix, not a UDS diagnostic layer.
The `build_from_dbc` module is responsible for any cross-walk from this
shape into ODX objects.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import cantools


@dataclass
class DbcSignal:
    """One signal inside a DBC frame."""

    name: str
    start_bit: int
    length_bits: int
    byte_order: str            # "little_endian" or "big_endian"
    is_signed: bool
    scale: float
    offset: float
    minimum: Optional[float]
    maximum: Optional[float]
    unit: str
    receivers: list[str]


@dataclass
class DbcMessage:
    """One CAN frame definition from the DBC."""

    frame_id: int
    name: str
    length_bytes: int
    senders: list[str]
    is_extended_frame: bool
    comment: Optional[str]
    signals: list[DbcSignal]


@dataclass
class DbcDatabase:
    """Result of parsing a DBC file."""

    source_path: str
    nodes: list[str]
    messages: list[DbcMessage] = field(default_factory=list)
    unresolved_todos: list[str] = field(default_factory=list)

    @property
    def signal_count(self) -> int:
        return sum(len(m.signals) for m in self.messages)


def parse_dbc(path: str | Path) -> DbcDatabase:
    """Parse a DBC file into a `DbcDatabase`.

    Args:
        path: Absolute path to a `.dbc` file.

    Returns:
        DbcDatabase populated from `cantools.database.load_file`.
    """
    p = Path(path)
    if not p.is_file():
        raise FileNotFoundError(f"DBC file not found: {p}")

    raw = cantools.database.load_file(str(p))

    node_names = [n.name for n in getattr(raw, "nodes", []) or []]

    result = DbcDatabase(source_path=str(p), nodes=node_names)

    for msg in raw.messages:
        signals: list[DbcSignal] = []
        for sig in msg.signals:
            # cantools reports byte_order as "little_endian" / "big_endian"
            byte_order = str(getattr(sig, "byte_order", "little_endian"))
            signals.append(
                DbcSignal(
                    name=sig.name,
                    start_bit=int(sig.start),
                    length_bits=int(sig.length),
                    byte_order=byte_order,
                    is_signed=bool(sig.is_signed),
                    scale=float(sig.scale) if sig.scale is not None else 1.0,
                    offset=float(sig.offset) if sig.offset is not None else 0.0,
                    minimum=(
                        float(sig.minimum)
                        if sig.minimum is not None
                        else None
                    ),
                    maximum=(
                        float(sig.maximum)
                        if sig.maximum is not None
                        else None
                    ),
                    unit=str(sig.unit or ""),
                    receivers=list(sig.receivers or []),
                )
            )

        result.messages.append(
            DbcMessage(
                frame_id=int(msg.frame_id),
                name=str(msg.name),
                length_bytes=int(msg.length),
                senders=list(msg.senders or []),
                is_extended_frame=bool(getattr(msg, "is_extended_frame", False)),
                comment=(str(msg.comment) if msg.comment else None),
                signals=signals,
            )
        )

    if not result.messages:
        result.unresolved_todos.append(
            f"TODO: no messages parsed from {p}"
        )

    return result
