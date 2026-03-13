#!/usr/bin/env python3
"""
@file       heartbeat_injector.py
@brief      Background heartbeat injector for PIL testing
@aspice     SWE.5 — Software Integration Testing
@iso        ISO 26262 Part 6, Section 10
@date       2026-03-10

Simulates heartbeat messages from all ECUs except the device under test (DUT).
Each heartbeat includes valid E2E protection (CRC-8 + alive counter).
Runs as a daemon thread — start before powering on the DUT.
"""

from __future__ import annotations

import logging
import threading
import time
from dataclasses import dataclass
from typing import Optional

import can

log = logging.getLogger("pil.heartbeat_injector")

# CRC-8 lookup table (polynomial 0x07, SAE J1850)
_CRC8_TABLE: list[int] = []
for _i in range(256):
    _c = _i
    for _ in range(8):
        _c = ((_c << 1) ^ 0x07) & 0xFF if _c & 0x80 else (_c << 1) & 0xFF
    _CRC8_TABLE.append(_c)


def _crc8(data: bytes) -> int:
    crc = 0x00
    for b in data:
        crc = _CRC8_TABLE[crc ^ b]
    return crc


@dataclass
class SimulatedECU:
    """Definition of a simulated ECU heartbeat."""
    name: str
    can_id: int
    ecu_id: int
    period_ms: float
    data_id: int
    operating_mode: int = 1  # RUN
    fault_status: int = 0
    _alive_counter: int = 0

    def build_frame(self) -> list[int]:
        """Build a 4-byte heartbeat frame with E2E protection.

        Byte layout (matches DBC):
          [0]: E2E_DataID (lo nibble) | E2E_AliveCounter (hi nibble)
          [1]: E2E_CRC8
          [2]: ECU_ID
          [3]: OperatingMode (lo nibble) | FaultStatus (hi nibble)
        """
        self._alive_counter = (self._alive_counter + 1) & 0x0F
        b0 = (self._alive_counter << 4) | (self.data_id & 0x0F)
        b2 = self.ecu_id & 0xFF
        b3 = (self.fault_status << 4) | (self.operating_mode & 0x0F)
        # CRC computed with CRC byte zeroed
        payload = bytes([b0, 0x00, b2, b3])
        crc = _crc8(payload)
        return [b0, crc, b2, b3]


# Default simulated ECUs (everything except CVC)
DEFAULT_SIMULATED_ECUS: list[SimulatedECU] = [
    SimulatedECU(name="FZC", can_id=0x011, ecu_id=0x11, period_ms=50, data_id=0x05),
    SimulatedECU(name="RZC", can_id=0x012, ecu_id=0x12, period_ms=50, data_id=0x06),
    SimulatedECU(name="SC",  can_id=0x013, ecu_id=0x13, period_ms=500, data_id=0x07),
    SimulatedECU(name="ICU", can_id=0x014, ecu_id=0x14, period_ms=500, data_id=0x08),
    SimulatedECU(name="TCU", can_id=0x015, ecu_id=0x15, period_ms=500, data_id=0x09),
]


class HeartbeatInjector:
    """Injects simulated ECU heartbeats on the CAN bus.

    @param channel    CAN channel (e.g. 'can0', 'COM3').
    @param interface  python-can interface type (e.g. 'socketcan', 'slcan').
    @param ecus       List of SimulatedECU definitions to inject.
    @param exclude    ECU names to exclude (e.g. ['FZC'] to simulate FZC loss).
    """

    def __init__(
        self,
        channel: str = "can0",
        interface: str = "socketcan",
        ecus: Optional[list[SimulatedECU]] = None,
        exclude: Optional[list[str]] = None,
    ) -> None:
        self._channel = channel
        self._interface = interface
        self._ecus = ecus or [SimulatedECU(
            name=e.name, can_id=e.can_id, ecu_id=e.ecu_id,
            period_ms=e.period_ms, data_id=e.data_id,
        ) for e in DEFAULT_SIMULATED_ECUS]
        self._exclude: set[str] = set(exclude or [])
        self._bus: Optional[can.Bus] = None
        self._threads: list[threading.Thread] = []
        self._running = False

    @property
    def ecus(self) -> list[SimulatedECU]:
        return self._ecus

    def exclude_ecu(self, name: str) -> None:
        """Stop injecting heartbeat for a specific ECU (live, thread-safe)."""
        self._exclude.add(name.upper())
        log.info("Excluded %s from heartbeat injection", name.upper())

    def include_ecu(self, name: str) -> None:
        """Resume injecting heartbeat for a specific ECU."""
        self._exclude.discard(name.upper())
        log.info("Re-included %s in heartbeat injection", name.upper())

    def start(self) -> None:
        """Open CAN bus and start injection threads."""
        if self._running:
            return
        self._bus = can.interface.Bus(
            channel=self._channel, interface=self._interface,
        )
        self._running = True
        for ecu in self._ecus:
            t = threading.Thread(
                target=self._inject_loop,
                args=(ecu,),
                daemon=True,
                name=f"pil-hb-{ecu.name}",
            )
            t.start()
            self._threads.append(t)
        log.info("Heartbeat injector started: %s",
                 ", ".join(f"{e.name}(0x{e.can_id:03X}@{e.period_ms}ms)"
                           for e in self._ecus))

    def stop(self) -> None:
        """Stop all injection threads and close bus."""
        self._running = False
        for t in self._threads:
            t.join(timeout=2.0)
        self._threads.clear()
        if self._bus:
            self._bus.shutdown()
            self._bus = None
        log.info("Heartbeat injector stopped")

    def _inject_loop(self, ecu: SimulatedECU) -> None:
        """Per-ECU injection loop."""
        period_sec = ecu.period_ms / 1000.0
        while self._running:
            if ecu.name.upper() not in self._exclude and self._bus:
                data = ecu.build_frame()
                msg = can.Message(
                    arbitration_id=ecu.can_id,
                    data=bytes(data),
                    is_extended_id=False,
                )
                try:
                    self._bus.send(msg)
                except can.CanError as exc:
                    log.warning("TX error %s: %s", ecu.name, exc)
            time.sleep(period_sec)
