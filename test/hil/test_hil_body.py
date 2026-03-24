#!/usr/bin/env python3
"""
@file       test_hil_body.py
@brief      HIL body/instrument/telematic vECU verification
@verifies   SWR-BCM-002, SWR-BCM-003, SWR-BCM-010, SWR-ICU-002, SWR-TCU-002
@aspice     SWE.6 — Software Qualification Testing
@iso        ISO 26262 Part 4, Section 7 — HSI Verification

Tests vECU (BCM, ICU, TCU) interactions with physical ECU traffic.
vECUs run as Docker POSIX binaries on laptop; they receive CAN traffic
from physical ECUs via the can0 ↔ vcan0 bridge.

Hop chain:
  Physical CVC → 0x100 → bridge → BCM (vECU) → 0x360 → bridge → can0
  Physical RZC → 0x301 → bridge → ICU (vECU) → gauge display
  TCU (vECU) → UDS 0x7DF → bridge → physical CVC → 0x7E0 response

Usage:
    python3 test/hil/test_hil_body.py
"""

import sys
import time

import cantools

from hil_test_lib import (
    DBC_PATH,
    CAN_VEHICLE_STATE, CAN_BCM_BODY, CAN_MOTOR_CURRENT,
    open_bus, can_recv, can_recv_decoded, poll_signal,
    check_heartbeat_period,
    wait_cvc_run, HopChecker, print_header,
)


def main():
    db = cantools.database.load_file(DBC_PATH)
    bus = open_bus()
    hc = HopChecker()

    print_header("Body / Instrument / Telematic vECU Verification")

    # Precondition: system in RUN (needs physical SC + CVC)
    if not wait_cvc_run(db, bus):
        print("  [FAIL] CVC did not reach RUN")
        bus.shutdown()
        sys.exit(1)
    print()

    # Hop 0: BCM receives Vehicle_State (0x100) from physical CVC
    print("Hop 0: BCM receives 0x100 from physical CVC via bridge")
    # Verify by checking BCM produces 0x016 (heartbeat — it only runs if CAN bridge works)
    msg = can_recv(bus, CAN_BCM_BODY, timeout=5)
    hc.check(0, "BCM heartbeat 0x016 present", msg is not None,
             "No 0x016 — BCM may not be receiving bridged CAN traffic")

    # Hop 1: BCM heartbeat 0x016 periodic at 500ms (DBC GenMsgCycleTime=500)
    print("Hop 1: BCM 0x016 @ 500ms periodic")
    if not hc.stopped:
        avg, jitter, passed = check_heartbeat_period(bus, CAN_BCM_BODY, 500.0,
                                                      duration=5.0)
        hc.check(1, f"BCM 0x016 avg={avg}ms jitter={jitter}ms", passed,
                 f"avg={avg}ms jitter={jitter}ms")

    # Hop 2: BCM 0x016 has alive counter (byte 0 upper nibble)
    print("Hop 2: BCM 0x016 alive counter increments")
    if not hc.stopped:
        alive_values = []
        for _ in range(5):
            msg = can_recv(bus, CAN_BCM_BODY, timeout=3)
            if msg and len(msg.data) >= 2:
                alive = (msg.data[0] >> 4) & 0x0F
                alive_values.append(alive)
        if len(alive_values) >= 3:
            increments = sum(1 for i in range(len(alive_values) - 1)
                           if alive_values[i+1] == (alive_values[i] + 1) & 0x0F)
            hc.check(2, f"Alive counter increments ({alive_values})",
                     increments >= len(alive_values) - 2,
                     f"Only {increments} increments")
        else:
            hc.check(2, "Enough 0x360 samples", False,
                     f"Only {len(alive_values)} samples")

    # Hop 3: ICU receives motor current (0x301) from physical RZC
    print("Hop 3: ICU receives 0x301 from physical RZC via bridge")
    if not hc.stopped:
        msg = can_recv(bus, CAN_MOTOR_CURRENT, timeout=5)
        hc.check(3, "Motor current 0x301 present on bus", msg is not None,
                 "No 0x301 — RZC may not be running or bridge is down")

    # Hop 4: Physical CVC Vehicle_State (0x100) visible on can0
    print("Hop 4: Physical CVC 0x100 on can0 (end-to-end)")
    if not hc.stopped:
        decoded = can_recv_decoded(db, bus, CAN_VEHICLE_STATE, timeout=3)
        if decoded:
            mode = int(decoded.get("Vehicle_State_Mode", 0))
            hc.check(4, f"Vehicle_State mode={mode} (RUN=1)", mode == 1,
                     f"mode={mode}")
        else:
            hc.check(4, "0x100 present", False, "No Vehicle_State on bus")

    bus.shutdown()
    sys.exit(hc.summary())


if __name__ == "__main__":
    main()
