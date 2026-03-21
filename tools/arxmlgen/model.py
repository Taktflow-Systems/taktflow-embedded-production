"""
arxmlgen data model — pure Python dataclasses, no ARXML dependency.

This module defines the contract between the ARXML reader and the generators.
Generators never touch ARXML directly; they consume these dataclasses.
"""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class Signal:
    """A single CAN signal within a PDU."""

    name: str
    bit_position: int
    bit_size: int
    byte_order: str = "little_endian"
    data_type: str = "uint8_t"
    init_value: int = 0
    compu_method: str = "IDENTICAL"
    factor: float = 1.0
    offset: float = 0.0
    unit: str = ""
    min_value: float = 0.0
    max_value: float = 0.0
    e2e_protected: bool = False
    e2e_data_id: int | None = None


@dataclass
class Pdu:
    """An I-PDU — corresponds to one CAN message."""

    name: str
    pdu_id: int = 0
    can_id: int = 0
    dlc: int = 8
    direction: str = "TX"
    cycle_ms: int = 0
    timeout_ms: int = 0
    signals: list[Signal] = field(default_factory=list)
    e2e_protected: bool = False
    e2e_data_id: int | None = None
    e2e_counter_bit: int | None = None
    e2e_crc_bit: int | None = None
    e2e_max_delta: int = 2
    satisfies: str = ""    # Requirement traceability (e.g., "TSR-022 FSR-001")
    asil: str = "QM"       # ASIL classification


@dataclass
class Port:
    """An SWC port — connects an SWC to a signal via an S/R interface."""

    name: str
    direction: str = "REQUIRED"
    interface_name: str = ""
    signal_name: str = ""
    data_type: str = "uint32_t"


@dataclass
class Runnable:
    """An SWC runnable entity."""

    name: str
    period_ms: int = 0
    is_init: bool = False
    priority: int = 5
    wdgm_se_id: int = 0xFF
    read_ports: list[Port] = field(default_factory=list)
    write_ports: list[Port] = field(default_factory=list)


@dataclass
class Swc:
    """A Software Component type with ports and runnables."""

    name: str
    short_name: str = ""
    asil: str = "QM"
    ports: list[Port] = field(default_factory=list)
    runnables: list[Runnable] = field(default_factory=list)

    @property
    def provided_ports(self) -> list[Port]:
        return [p for p in self.ports if p.direction == "PROVIDED"]

    @property
    def required_ports(self) -> list[Port]:
        return [p for p in self.ports if p.direction == "REQUIRED"]


@dataclass
class Ecu:
    """Complete ECU model — populated by reader, consumed by generators."""

    name: str
    prefix: str
    swcs: list[Swc] = field(default_factory=list)
    tx_pdus: list[Pdu] = field(default_factory=list)
    rx_pdus: list[Pdu] = field(default_factory=list)
    all_signals: list[Signal] = field(default_factory=list)
    rte_signal_map: dict[str, int] = field(default_factory=dict)
    com_signal_map: dict[str, int] = field(default_factory=dict)

    # From sidecar (optional)
    uds_rx_pdu_name: str = ""
    uds_tx_pdu_name: str = ""
    dtc_events: dict[str, int] = field(default_factory=dict)
    rte_period_ms: int = 10  # Com_MainFunction_Tx call period (from *_RTE_PERIOD_MS)
    e2e_data_ids: dict[str, int] = field(default_factory=dict)
    enums: dict[str, int] = field(default_factory=dict)
    thresholds: dict[str, int | str] = field(default_factory=dict)
    rte_aliases: dict[str, str] = field(default_factory=dict)
    rte_internal_signal_count: int = 0
    rte_internal_signals: list[str] = field(default_factory=list)


@dataclass
class ProjectModel:
    """Top-level model — output of reader, input to generators."""

    name: str
    ecus: dict[str, Ecu] = field(default_factory=dict)
    platform_types: list[str] = field(default_factory=list)
    sr_interfaces: list[str] = field(default_factory=list)
    total_signals: int = 0
    total_pdus: int = 0
    total_swcs: int = 0
    total_runnables: int = 0
