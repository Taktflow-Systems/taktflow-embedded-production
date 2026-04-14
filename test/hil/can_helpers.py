#!/usr/bin/env python3
"""
@file       can_helpers.py
@brief      CAN bus utility functions for HIL test runner
@aspice     SWE.6 — Software Qualification Testing
@iso        ISO 26262 Part 4, Section 7 — HSI Verification
@date       2026-03-08

Provides reusable CAN operations: send, receive, heartbeat measurement,
E2E validation, signal decoding, period measurement, and MQTT fault injection.
"""

from __future__ import annotations

import json
import logging
import os
import struct
import time
from typing import Any, Optional

import can

log = logging.getLogger("hil.can_helpers")

# ---------------------------------------------------------------------------
# CAN Frame Operations
# ---------------------------------------------------------------------------


def send_can(
    bus: can.Bus,
    can_id: int,
    data: list[int],
    is_extended: bool = False,
) -> None:
    """Send a single CAN frame.

    @param bus        Active python-can Bus instance.
    @param can_id     CAN arbitration ID.
    @param data       Payload bytes (list of ints, max 8).
    @param is_extended  True for 29-bit extended ID.
    """
    msg = can.Message(
        arbitration_id=can_id,
        data=bytes(data),
        is_extended_id=is_extended,
    )
    bus.send(msg)
    log.debug("TX  0x%03X [%d] %s", can_id, len(data), data)


def wait_for_message(
    bus: can.Bus,
    can_id: int,
    timeout_sec: float = 5.0,
) -> Optional[can.Message]:
    """Block until a specific CAN ID is received or timeout.

    @param bus        Active python-can Bus instance.
    @param can_id     CAN arbitration ID to wait for.
    @param timeout_sec  Max seconds to wait.
    @return           Received can.Message or None on timeout.
    """
    deadline = time.monotonic() + timeout_sec
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        msg = bus.recv(timeout=min(remaining, 0.1))
        if msg is not None and msg.arbitration_id == can_id:
            return msg
    return None


def check_heartbeat(
    bus: can.Bus,
    can_id: int,
    expected_period_ms: float,
    tolerance_pct: float = 15.0,
    duration_sec: float = 2.0,
) -> dict[str, Any]:
    """Verify a periodic CAN message arrives at the expected rate.

    @param bus              Active python-can Bus instance.
    @param can_id           CAN ID of heartbeat message.
    @param expected_period_ms  Expected period in milliseconds.
    @param tolerance_pct    Acceptable deviation as percentage (default 15%).
    @param duration_sec     How long to observe.
    @return  Dict with keys: passed, count, avg_period_ms, min_period_ms,
             max_period_ms, jitter_ms, missed.
    """
    timestamps: list[float] = []
    deadline = time.monotonic() + duration_sec

    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        msg = bus.recv(timeout=min(remaining, 0.05))
        if msg is not None and msg.arbitration_id == can_id:
            timestamps.append(time.monotonic())

    if len(timestamps) < 2:
        return {
            "passed": False,
            "count": len(timestamps),
            "avg_period_ms": 0,
            "min_period_ms": 0,
            "max_period_ms": 0,
            "jitter_ms": 0,
            "missed": 0,
            "error": f"Only {len(timestamps)} messages in {duration_sec}s",
        }

    deltas_ms = [
        (timestamps[i + 1] - timestamps[i]) * 1000
        for i in range(len(timestamps) - 1)
    ]
    avg_ms = sum(deltas_ms) / len(deltas_ms)
    min_ms = min(deltas_ms)
    max_ms = max(deltas_ms)
    jitter_ms = max_ms - min_ms

    tolerance_ms = expected_period_ms * tolerance_pct / 100.0
    lo = expected_period_ms - tolerance_ms
    hi = expected_period_ms + tolerance_ms

    missed = sum(1 for d in deltas_ms if d < lo or d > hi)
    # Allow up to 10% outliers (Linux scheduling jitter)
    passed = missed <= max(1, len(deltas_ms) * 0.10)

    return {
        "passed": passed,
        "count": len(timestamps),
        "avg_period_ms": round(avg_ms, 2),
        "min_period_ms": round(min_ms, 2),
        "max_period_ms": round(max_ms, 2),
        "jitter_ms": round(jitter_ms, 2),
        "missed": missed,
    }


# ---------------------------------------------------------------------------
# E2E Protection Validation
# ---------------------------------------------------------------------------

CRC8_TABLE: list[int] = []
_CRC8_POLY = 0x1D  # SAE J1850 — must match E2E.c E2E_Crc8Table

for _i in range(256):
    _crc = _i
    for _ in range(8):
        if _crc & 0x80:
            _crc = ((_crc << 1) ^ _CRC8_POLY) & 0xFF
        else:
            _crc = (_crc << 1) & 0xFF
    CRC8_TABLE.append(_crc)


def crc8(data: bytes, init: int = 0xFF, xor_out: int = 0xFF) -> int:
    """Compute CRC-8 SAE-J1850 over data bytes.

    Matches firmware E2E.c: init=0xFF, xor_out=0xFF, poly=0x1D (table).

    @param data     Input bytes.
    @param init     Initial CRC value (default 0xFF).
    @param xor_out  XOR applied to final CRC (default 0xFF).
    @return         CRC-8 value (0-255).
    """
    crc = init
    for b in data:
        crc = CRC8_TABLE[crc ^ b]
    return crc ^ xor_out


# DataID lookup by CAN ID (from E2E_Cfg_*.c)
_E2E_DATA_IDS: dict[int, int] = {
    0x001: 0x01,  # EStop_Broadcast
    0x010: 0x02,  # CVC_Heartbeat
    0x011: 0x03,  # FZC_Heartbeat
    0x012: 0x04,  # RZC_Heartbeat
    0x100: 0x05,  # Vehicle_State
    0x101: 0x06,  # Torque_Request
    0x102: 0x07,  # Steer_Command
    0x103: 0x08,  # Brake_Command
}


def check_e2e(
    msg: can.Message,
    crc_byte: int = 1,
    alive_byte: int = 0,
    alive_bits: int = 4,
    data_id: int | None = None,
) -> dict[str, Any]:
    """Validate E2E CRC-8 and alive counter on a CAN message.

    E2E layout (AUTOSAR Profile P01):
      byte 0: [AliveCounter:4 | DataID:4]
      byte 1: CRC-8 (over bytes[2..N-1] + DataID, init=0xFF, xor_out=0xFF)
      bytes 2..N: payload

    @param msg         CAN message to validate.
    @param crc_byte    Byte index of CRC field (default 1).
    @param alive_byte  Byte index containing alive counter (default 0).
    @param alive_bits  Number of bits for alive counter (default 4).
    @param data_id     DataID for CRC calc (auto-detected from CAN ID if None).
    @return  Dict with keys: crc_valid, crc_expected, crc_actual,
             alive_counter, passed.
    """
    if len(msg.data) < 2:
        return {"passed": False, "error": "Message too short"}

    received_crc = msg.data[crc_byte]
    alive_counter = (msg.data[alive_byte] >> 4) & ((1 << alive_bits) - 1)

    # Determine DataID
    if data_id is None:
        data_id = _E2E_DATA_IDS.get(msg.arbitration_id, msg.data[0] & 0x0F)

    # CRC over payload bytes (2..N-1) + DataID
    crc_input = bytes(msg.data[2:]) + bytes([data_id])
    expected_crc = crc8(crc_input)

    return {
        "crc_valid": received_crc == expected_crc,
        "crc_expected": expected_crc,
        "crc_actual": received_crc,
        "alive_counter": alive_counter,
        "passed": received_crc == expected_crc,
    }


# ---------------------------------------------------------------------------
# Signal Decoding
# ---------------------------------------------------------------------------


def decode_uint16_le(data: bytes, offset: int) -> int:
    """Decode a uint16 little-endian from CAN payload.

    @param data    CAN message data bytes.
    @param offset  Byte offset of the uint16.
    @return        Decoded unsigned 16-bit integer.
    """
    return struct.unpack_from("<H", data, offset)[0]


def decode_signal(
    msg: can.Message,
    byte_offset: int,
    bit_length: int = 16,
    signed: bool = False,
    scale: float = 1.0,
    offset: float = 0.0,
) -> float:
    """Decode a CAN signal with scale and offset.

    @param msg          CAN message.
    @param byte_offset  Start byte of the signal.
    @param bit_length   Signal width in bits (8 or 16).
    @param signed       True for signed integer.
    @param scale        Multiply raw value by scale.
    @param offset       Add offset after scaling.
    @return             Physical value as float.
    """
    if bit_length == 8:
        raw = msg.data[byte_offset]
        if signed and raw > 127:
            raw -= 256
    elif bit_length == 16:
        raw = struct.unpack_from("<H", msg.data, byte_offset)[0]
        if signed and raw > 32767:
            raw -= 65536
    else:
        raw = msg.data[byte_offset]

    return raw * scale + offset


# ---------------------------------------------------------------------------
# Period Measurement
# ---------------------------------------------------------------------------


def measure_period(
    bus: can.Bus,
    can_id: int,
    sample_count: int = 20,
    timeout_sec: float = 5.0,
) -> dict[str, Any]:
    """Measure the transmission period of a CAN message.

    @param bus           Active python-can Bus instance.
    @param can_id        CAN ID to measure.
    @param sample_count  Number of messages to capture.
    @param timeout_sec   Max observation time.
    @return  Dict with keys: avg_ms, min_ms, max_ms, std_ms, samples.
    """
    timestamps: list[float] = []
    deadline = time.monotonic() + timeout_sec

    while len(timestamps) < sample_count and time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        msg = bus.recv(timeout=min(remaining, 0.1))
        if msg is not None and msg.arbitration_id == can_id:
            timestamps.append(time.monotonic())

    if len(timestamps) < 2:
        return {"avg_ms": 0, "min_ms": 0, "max_ms": 0, "std_ms": 0, "samples": 0}

    deltas_ms = [
        (timestamps[i + 1] - timestamps[i]) * 1000
        for i in range(len(timestamps) - 1)
    ]
    avg = sum(deltas_ms) / len(deltas_ms)
    variance = sum((d - avg) ** 2 for d in deltas_ms) / len(deltas_ms)
    std = variance ** 0.5

    return {
        "avg_ms": round(avg, 2),
        "min_ms": round(min(deltas_ms), 2),
        "max_ms": round(max(deltas_ms), 2),
        "std_ms": round(std, 2),
        "samples": len(timestamps),
    }


# ---------------------------------------------------------------------------
# MQTT Fault Injection
# ---------------------------------------------------------------------------


def inject_mqtt_fault(
    host: str,
    topic: str,
    payload: dict[str, Any],
    port: int = 1883,
) -> bool:
    """Publish a fault injection command via MQTT.

    @param host     MQTT broker hostname.
    @param topic    MQTT topic (e.g. taktflow/command/plant_inject).
    @param payload  Dict to JSON-encode and publish.
    @param port     MQTT broker port.
    @return         True if published successfully.
    """
    try:
        import paho.mqtt.publish as publish

        _user = os.environ.get("MQTT_USER", "taktflow")
        _pass = os.environ.get("MQTT_PASSWORD", "taktflow-dev")
        _auth = {"username": _user, "password": _pass} if _user else None
        publish.single(
            topic,
            payload=json.dumps(payload),
            hostname=host,
            port=port,
            qos=0,
            auth=_auth,
        )
        log.info("MQTT inject -> %s: %s", topic, payload)
        return True
    except Exception as exc:
        log.warning("MQTT inject failed: %s", exc)
        return False


def reset_mqtt_faults(host: str, port: int = 1883) -> bool:
    """Reset all plant-sim faults via MQTT.

    @param host  MQTT broker hostname.
    @param port  MQTT broker port.
    @return      True if published successfully.
    """
    return inject_mqtt_fault(
        host,
        "taktflow/command/plant_inject",
        {"type": "reset"},
        port,
    )
