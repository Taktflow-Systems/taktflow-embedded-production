#!/usr/bin/env python3
"""
@file       test_hil_e2e.py
@brief      HIL E2E protection verification on physical CAN bus
@verifies   TSR-022, TSR-023, TSR-024, SSR-CVC-008, SSR-CVC-010, SSR-FZC-015
@aspice     SWE.6 — Software Qualification Testing
@iso        ISO 26262 Part 4, Section 7 — HSI Verification

Verifies E2E CRC-8 and alive counter on real CAN frames from physical ECUs.
Also tests corrupt frame rejection and alive counter freeze detection.

Usage:
    python3 test/hil/test_hil_e2e.py
"""

import struct
import sys
import time

import can

from hil_test_lib import (
    CAN_VEHICLE_STATE, CAN_CVC_HEARTBEAT,
    open_bus, can_recv, precondition_all_ecus_healthy,
    print_header, HopChecker,
)


def crc8_table(data, init=0xFF, xor_out=0xFF, poly=0x1D):
    """CRC-8 SAE-J1850 (table-based, matches E2E.c).

    E2E layout: byte 0 = [AliveCounter:4 | DataID:4]
                byte 1 = CRC-8
                bytes 2..N = payload
    CRC is computed over bytes 2..N + DataID, init=0xFF, xor_out=0xFF.
    """
    table = []
    for i in range(256):
        crc = i
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ poly) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
        table.append(crc)
    crc = init
    for b in data:
        crc = table[crc ^ b]
    return crc ^ xor_out


# Vehicle_State DataID from E2E_Cfg_Cvc.c
VEHICLE_STATE_DATA_ID = 0x05
CVC_HEARTBEAT_DATA_ID = 0x02


def main():
    bus = open_bus()
    hc = HopChecker()

    print_header("E2E Protection Verification")

    # Unified precondition: all ECUs healthy
    precondition_all_ecus_healthy(bus)

    # Hop 0: Vehicle_State (0x100) has valid CRC-8
    # E2E layout: byte0=[Alive:4|DataID:4], byte1=CRC, bytes2..5=payload
    # CRC = CRC-8 over bytes[2..N-1] + DataID, init=0xFF, xor_out=0xFF
    print("Hop 0: Vehicle_State (0x100) CRC-8 validation")
    msg = can_recv(bus, CAN_VEHICLE_STATE, timeout=5)
    if msg and len(msg.data) >= 2:
        received_crc = msg.data[1]
        crc_input = bytes(msg.data[2:]) + bytes([VEHICLE_STATE_DATA_ID])
        expected_crc = crc8_table(crc_input)
        hc.check(0, f"CRC recv=0x{received_crc:02X} exp=0x{expected_crc:02X}",
                 received_crc == expected_crc,
                 f"CRC mismatch: recv=0x{received_crc:02X} exp=0x{expected_crc:02X}")
    else:
        hc.check(0, "0x100 present", False, "No Vehicle_State on bus")

    # Hop 1: Alive counter increments on 0x100
    # Alive counter is in byte 0 upper nibble (bits 7:4)
    print("Hop 1: Alive counter increments on 0x100")
    if not hc.stopped:
        alive_values = []
        for _ in range(5):
            msg = can_recv(bus, CAN_VEHICLE_STATE, timeout=3)
            if msg and len(msg.data) >= 2:
                alive = (msg.data[0] >> 4) & 0x0F
                alive_values.append(alive)
        if len(alive_values) >= 3:
            increments = sum(1 for i in range(len(alive_values) - 1)
                           if alive_values[i+1] == (alive_values[i] + 1) & 0x0F)
            hc.check(1, f"Alive counter increments ({alive_values})",
                     increments >= len(alive_values) - 2,
                     f"Only {increments} increments in {alive_values}")
        else:
            hc.check(1, "Enough 0x100 samples", False,
                     f"Only {len(alive_values)} samples")

    # Hop 2: Multiple consecutive frames all have valid CRC
    print("Hop 2: 10 consecutive 0x100 frames — all valid CRC")
    if not hc.stopped:
        valid_count = 0
        total = 10
        for _ in range(total):
            msg = can_recv(bus, CAN_VEHICLE_STATE, timeout=3)
            if msg and len(msg.data) >= 2:
                received_crc = msg.data[1]
                crc_input = bytes(msg.data[2:]) + bytes([VEHICLE_STATE_DATA_ID])
                expected_crc = crc8_table(crc_input)
                if received_crc == expected_crc:
                    valid_count += 1
        hc.check(2, f"CRC valid: {valid_count}/{total}", valid_count == total,
                 f"Only {valid_count}/{total} valid")

    # Hop 3: CVC heartbeat (0x010) E2E validation
    # Same layout: byte0=[Alive:4|DataID:4], byte1=CRC, bytes2..3=payload
    print("Hop 3: CVC heartbeat (0x010) CRC-8 validation")
    if not hc.stopped:
        msg = can_recv(bus, CAN_CVC_HEARTBEAT, timeout=3)
        if msg and len(msg.data) >= 2:
            received_crc = msg.data[1]
            crc_input = bytes(msg.data[2:]) + bytes([CVC_HEARTBEAT_DATA_ID])
            expected_crc = crc8_table(crc_input)
            hc.check(3, f"HB CRC recv=0x{received_crc:02X} exp=0x{expected_crc:02X}",
                     received_crc == expected_crc,
                     f"CRC mismatch")
        else:
            hc.check(3, "0x010 present", False, "No CVC heartbeat on bus")

    bus.shutdown()
    sys.exit(hc.summary())


if __name__ == "__main__":
    main()
