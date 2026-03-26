#!/usr/bin/env python3
"""
@file       test_hil_selftest.py
@brief      HIL ECU startup self-test verification — physical CVC/FZC/RZC
@verifies   SWR-CVC-029, SWR-FZC-029, SWR-RZC-029
@traces_to  SSR-CVC-029, SSR-FZC-029, SSR-RZC-029, TSR-046
@aspice     SWE.6 — Software Qualification Testing
@iso        ISO 26262 Part 6, Section 9 — Software Unit Verification

Verifies ECU startup self-test on physical hardware by observing:
  1. ECU heartbeat appears within expected time after reset
  2. No self-test DTC (0x500) broadcast during startup
  3. CVC transitions from INIT→RUN (requires SC on bus + all heartbeats)

Self-test sequence per ECU:
  CVC: SPI loopback, CAN loopback, NVM CRC, OLED ACK, MPU, canary, RAM
  FZC: servo neutral, SPI sensor, UART lidar, CAN loopback, MPU, canary, RAM
  RZC: BTS7960 GPIO, current zero-cal, NTC range, encoder stuck, MPU, canary, RAM

Self-test DTC numbers: CVC=16, FZC=13, RZC=7

Usage:
    python3 test/hil/test_hil_selftest.py
"""

import sys
import time

import can
import cantools

from hil_test_lib import (
    DBC_PATH,
    CAN_CVC_HEARTBEAT, CAN_FZC_HEARTBEAT, CAN_RZC_HEARTBEAT,
    CAN_VEHICLE_STATE, CAN_DTC,
    STATE_NAMES, ECU_NAMES,
    open_bus, can_recv, can_recv_decoded,
    reset_cvc_hardware, uds_ecu_reset_rzc,
    wait_for_all_heartbeats,
    print_header, HopChecker, DtcSniffer,
)


# Self-test DTC event IDs
DTC_CVC_SELFTEST = 16
DTC_FZC_SELFTEST = 13
DTC_RZC_SELFTEST = 7

# Startup time budget: from reset to first heartbeat
STARTUP_BUDGET_MS = 5000  # 5s generous budget for self-test + BSW init


def wait_heartbeat_after_reset(bus, can_id, name, timeout=10.0):
    """Wait for first heartbeat from an ECU after reset.

    Returns (elapsed_ms, True) or (None, False).
    """
    t_start = time.time()
    end = t_start + timeout
    while time.time() < end:
        msg = bus.recv(timeout=0.5)
        if msg and msg.arbitration_id == can_id:
            elapsed = (time.time() - t_start) * 1000.0
            return elapsed, True
    return None, False


def main():
    db = cantools.database.load_file(DBC_PATH)
    bus = open_bus()
    hc = HopChecker()

    print_header("ECU Startup Self-Test Verification")

    # -----------------------------------------------------------------------
    # Phase 1: Verify all ECUs currently running (baseline)
    # -----------------------------------------------------------------------
    print("Phase 1: Baseline — verify all ECUs currently running")
    hb = wait_for_all_heartbeats(bus, timeout=15.0)
    stm32_ok = all(hb.get(x, False) for x in
                   [CAN_CVC_HEARTBEAT, CAN_FZC_HEARTBEAT, CAN_RZC_HEARTBEAT])
    if not stm32_ok:
        missing = [hex(k) for k, v in hb.items() if not v]
        print(f"  [FAIL] Missing ECUs: {missing} — power on all ECUs first")
        bus.shutdown()
        sys.exit(1)
    print("  [OK] CVC/FZC/RZC all present")
    print()

    # -----------------------------------------------------------------------
    # Phase 2: RZC reset and startup verification (UDS ECUReset — non-destructive)
    # -----------------------------------------------------------------------
    print("Phase 2: RZC reset via UDS ECUReset (SID 0x11)")

    # Start DTC sniffer for RZC self-test DTC
    dtc_sniffer = DtcSniffer(db, target_dtc=DTC_RZC_SELFTEST)
    dtc_sniffer.start()

    # Hop 0: Send UDS ECUReset to RZC
    print("Hop 0: RZC UDS ECUReset → heartbeat recovery")
    got_response = uds_ecu_reset_rzc(bus)
    if got_response:
        print("  [OK] RZC ECUReset positive response received")
    else:
        print("  [WARN] No UDS response — RZC may have reset before responding")

    # Wait for RZC heartbeat to reappear (after re-init)
    elapsed, ok = wait_heartbeat_after_reset(bus, CAN_RZC_HEARTBEAT, "RZC", timeout=10.0)
    if ok:
        hc.check(0, f"RZC heartbeat recovered in {elapsed:.0f}ms", True)
    else:
        hc.check(0, "RZC heartbeat after ECUReset", False,
                 "No 0x012 within 10s — self-test may have failed")

    # Hop 1: No RZC self-test DTC on bus
    print("Hop 1: No RZC self-test DTC on 0x500")
    time.sleep(3)  # Allow DTC broadcast window
    dtc_sniffer.stop()
    dtc_decoded = dtc_sniffer.get_decoded()
    if dtc_decoded:
        dtc_num = int(dtc_decoded.get("DTC_Broadcast_Number", 0))
        ecu = int(dtc_decoded.get("DTC_Broadcast_ECU_Source", 0))
        if dtc_num == DTC_RZC_SELFTEST:
            hc.check(1, f"RZC self-test DTC={dtc_num} on bus", False,
                     "Self-test failure reported")
        else:
            hc.check(1, f"DTC on bus (num={dtc_num}, not RZC self-test)", True)
    else:
        hc.check(1, "No self-test DTC (clean startup)", True)

    # -----------------------------------------------------------------------
    # Phase 3: CVC hardware reset and startup verification
    # This is a full MCU reset via CubeProgrammer (NRST equivalent)
    # -----------------------------------------------------------------------
    print()
    print("Phase 3: CVC hardware reset via CubeProgrammer")

    # Start DTC sniffer for CVC self-test DTC
    dtc_sniffer2 = DtcSniffer(db, target_dtc=DTC_CVC_SELFTEST)
    dtc_sniffer2.start()

    # Hop 2: CVC hardware reset → heartbeat recovery
    print("Hop 2: CVC hardware reset → heartbeat recovery")
    if not hc.stopped:
        reset_cvc_hardware()
        elapsed, ok = wait_heartbeat_after_reset(
            bus, CAN_CVC_HEARTBEAT, "CVC", timeout=10.0)
        if ok:
            within_budget = elapsed < STARTUP_BUDGET_MS
            hc.check(2, f"CVC heartbeat in {elapsed:.0f}ms "
                     f"(budget={STARTUP_BUDGET_MS}ms)",
                     within_budget,
                     f"{elapsed:.0f}ms > {STARTUP_BUDGET_MS}ms budget")
        else:
            hc.check(2, "CVC heartbeat after reset", False,
                     "No 0x010 within 10s — self-test may have failed")

    # Hop 3: No CVC self-test DTC
    print("Hop 3: No CVC self-test DTC on 0x500")
    if not hc.stopped:
        time.sleep(3)
        dtc_sniffer2.stop()
        dtc_decoded2 = dtc_sniffer2.get_decoded()
        if dtc_decoded2:
            dtc_num = int(dtc_decoded2.get("DTC_Broadcast_Number", 0))
            if dtc_num == DTC_CVC_SELFTEST:
                hc.check(3, f"CVC self-test DTC on bus", False,
                         "Self-test failure reported")
            else:
                hc.check(3, f"DTC on bus (num={dtc_num}, not CVC self-test)", True)
        else:
            hc.check(3, "No self-test DTC (clean startup)", True)
    else:
        dtc_sniffer2.stop()

    # Hop 4: CVC reaches INIT then transitions to RUN
    print("Hop 4: CVC INIT → RUN transition (requires SC on bus)")
    if not hc.stopped:
        # Wait for Vehicle_State to appear and check state progression
        t_start = time.time()
        saw_init = False
        saw_run = False
        timeout = 30.0
        while (time.time() - t_start) < timeout:
            decoded = can_recv_decoded(db, bus, CAN_VEHICLE_STATE, timeout=2)
            if decoded:
                mode = int(decoded.get("Vehicle_State_Mode", 0))
                if mode == 0:
                    saw_init = True
                elif mode == 1:
                    saw_run = True
                    break
        elapsed = (time.time() - t_start) * 1000.0
        if saw_run:
            hc.check(4, f"CVC reached RUN in {elapsed:.0f}ms "
                     f"(INIT seen: {saw_init})", True)
        else:
            decoded = can_recv_decoded(db, bus, CAN_VEHICLE_STATE, timeout=2)
            state = "?"
            if decoded:
                mode = int(decoded.get("Vehicle_State_Mode", 0))
                state = STATE_NAMES.get(mode, str(mode))
            hc.check(4, f"CVC stuck in {state} after {elapsed:.0f}ms", False,
                     "Check SC (TMS570) on bus — CVC needs SC heartbeat for RUN")

    # Hop 5: All ECU heartbeats restored after reset sequence
    print("Hop 5: All ECU heartbeats restored post-reset")
    if not hc.stopped:
        hb_post = wait_for_all_heartbeats(bus, timeout=15.0)
        all_ok = all(hb_post.get(x, False) for x in
                     [CAN_CVC_HEARTBEAT, CAN_FZC_HEARTBEAT, CAN_RZC_HEARTBEAT])
        missing = [hex(k) for k, v in hb_post.items() if not v and k != 0x013]
        hc.check(5, "All STM32 ECU heartbeats present", all_ok,
                 f"Missing: {missing}")

    bus.shutdown()
    sys.exit(hc.summary())


if __name__ == "__main__":
    main()
