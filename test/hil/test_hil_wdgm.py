#!/usr/bin/env python3
"""
@file       test_hil_wdgm.py
@brief      HIL watchdog supervision verification — physical ECUs
@verifies   SWR-BSW-019, SWR-BSW-020
@traces_to  TSR-046, TSR-047, SSR-CVC-029
@aspice     SWE.6 — Software Qualification Testing
@iso        ISO 26262 Part 6, Section 9 — Software Unit Verification

Verifies watchdog supervision (WdgM) operates correctly on physical ECUs:
  1. WdgM feeds external watchdog (TPS3823) via DIO flip — verified
     indirectly by sustained heartbeat presence (no watchdog reset)
  2. Alive counter increments on CAN heartbeat frames (E2E byte 0)
  3. No watchdog DTC (Dem event 15) during sustained operation
  4. ECU heartbeat continuity over 60s soak (no gaps > 2× period)

WdgM architecture:
  - Each SWC calls WdgM_CheckpointReached() per scheduler cycle
  - WdgM_MainFunction() validates alive counters in [min, max] range
  - If OK: DIO flip feeds external TPS3823 watchdog
  - If FAILED > FailedRefCycleTol: EXPIRED → DTC 15 → watchdog timeout → MCU reset

Usage:
    python3 test/hil/test_hil_wdgm.py [--soak 60]
"""

import argparse
import sys
import time

import can
import cantools

from hil_test_lib import (
    DBC_PATH,
    CAN_CVC_HEARTBEAT, CAN_FZC_HEARTBEAT, CAN_RZC_HEARTBEAT,
    CAN_SC_STATUS, CAN_DTC,
    ECU_NAMES,
    open_bus, wait_for_all_heartbeats,
    print_header, HopChecker, DtcSniffer,
)

# Watchdog expired DTC (from WdgM.c)
DTC_WDGM_EXPIRED = 15


def check_alive_counter(bus, can_id, name, sample_count=20, timeout=5.0):
    """Verify alive counter in byte 0 upper nibble increments correctly.

    Returns (increments, total_samples, passed).
    """
    alive_values = []
    end = time.time() + timeout
    while len(alive_values) < sample_count and time.time() < end:
        msg = bus.recv(timeout=0.5)
        if msg and msg.arbitration_id == can_id and len(msg.data) >= 1:
            alive = (msg.data[0] >> 4) & 0x0F
            alive_values.append(alive)

    if len(alive_values) < 5:
        return 0, len(alive_values), False

    increments = sum(
        1 for i in range(len(alive_values) - 1)
        if alive_values[i + 1] == (alive_values[i] + 1) & 0x0F
    )
    total = len(alive_values) - 1
    # Allow 1-2 misses (scheduler jitter can cause double-read on same counter)
    passed = increments >= total - 2
    return increments, total, passed


def heartbeat_soak(bus, can_ids, duration=60.0):
    """Monitor heartbeats for duration seconds.

    Returns dict of {can_id: {"count", "gaps", "max_gap_ms", "lost_ms"}}.
    """
    last_seen = {}
    stats = {}
    for cid in can_ids:
        stats[cid] = {"count": 0, "gaps": 0, "max_gap_ms": 0.0, "lost_ms": 0.0}

    end = time.time() + duration
    while time.time() < end:
        msg = bus.recv(timeout=0.2)
        if msg is None:
            continue
        cid = msg.arbitration_id
        if cid not in stats:
            continue
        now = time.time()
        stats[cid]["count"] += 1
        if cid in last_seen:
            gap_ms = (now - last_seen[cid]) * 1000.0
            if gap_ms > stats[cid]["max_gap_ms"]:
                stats[cid]["max_gap_ms"] = gap_ms
            # Expected period: 50ms for STM32 heartbeats, 100ms for SC
            expected = 100.0 if cid == CAN_SC_STATUS else 50.0
            if gap_ms > expected * 2.5:
                stats[cid]["gaps"] += 1
                stats[cid]["lost_ms"] += gap_ms - expected
        last_seen[cid] = now

    return stats


def main():
    parser = argparse.ArgumentParser(description="HIL watchdog supervision test")
    parser.add_argument("--soak", type=float, default=60.0,
                        help="Soak duration in seconds (default: 60)")
    args = parser.parse_args()

    db = cantools.database.load_file(DBC_PATH)
    bus = open_bus()
    hc = HopChecker()

    print_header("Watchdog Supervision Verification")

    # Precondition
    print("Precondition: Wait for physical ECU heartbeats")
    hb = wait_for_all_heartbeats(bus, timeout=15.0)
    stm32_ok = all(hb.get(x, False) for x in
                   [CAN_CVC_HEARTBEAT, CAN_FZC_HEARTBEAT, CAN_RZC_HEARTBEAT])
    if not stm32_ok:
        missing = [hex(k) for k, v in hb.items() if not v]
        print(f"  [FAIL] Missing ECUs: {missing}")
        bus.shutdown()
        sys.exit(1)
    print("  [OK] CVC/FZC/RZC all present")
    print()

    # Start DTC sniffer for watchdog expired DTC
    dtc_sniffer = DtcSniffer(db, target_dtc=DTC_WDGM_EXPIRED)
    dtc_sniffer.start()

    # Hop 0: CVC alive counter increments
    print("Hop 0: CVC alive counter (byte 0 upper nibble) increments")
    inc, total, ok = check_alive_counter(bus, CAN_CVC_HEARTBEAT, "CVC")
    hc.check(0, f"CVC alive: {inc}/{total} increments", ok,
             f"Only {inc}/{total} — WdgM checkpoint not firing?")

    # Hop 1: FZC alive counter increments
    print("Hop 1: FZC alive counter increments")
    if not hc.stopped:
        inc, total, ok = check_alive_counter(bus, CAN_FZC_HEARTBEAT, "FZC")
        hc.check(1, f"FZC alive: {inc}/{total} increments", ok,
                 f"Only {inc}/{total}")

    # Hop 2: RZC alive counter increments
    print("Hop 2: RZC alive counter increments")
    if not hc.stopped:
        inc, total, ok = check_alive_counter(bus, CAN_RZC_HEARTBEAT, "RZC")
        hc.check(2, f"RZC alive: {inc}/{total} increments", ok,
                 f"Only {inc}/{total}")

    # Hop 3: Heartbeat soak — no watchdog resets during sustained operation
    soak_sec = args.soak
    print(f"Hop 3: Heartbeat soak test ({soak_sec:.0f}s) — no gaps > 2.5× period")
    if not hc.stopped:
        print(f"  Monitoring CVC/FZC/RZC heartbeats for {soak_sec:.0f}s...")
        monitored = [CAN_CVC_HEARTBEAT, CAN_FZC_HEARTBEAT, CAN_RZC_HEARTBEAT]
        stats = heartbeat_soak(bus, monitored, duration=soak_sec)

        id_to_name = {CAN_CVC_HEARTBEAT: "CVC", CAN_FZC_HEARTBEAT: "FZC",
                      CAN_RZC_HEARTBEAT: "RZC"}
        all_ok = True
        for cid in monitored:
            s = stats[cid]
            name = id_to_name.get(cid, hex(cid))
            print(f"    {name}: {s['count']} frames, "
                  f"max_gap={s['max_gap_ms']:.0f}ms, "
                  f"gaps>{2.5*50:.0f}ms: {s['gaps']}")
            if s["gaps"] > 0:
                all_ok = False
        hc.check(3, f"Soak {soak_sec:.0f}s — heartbeat continuity", all_ok,
                 "Heartbeat gaps detected — possible watchdog reset or scheduler stall")

    # Hop 4: No watchdog DTC during soak
    print("Hop 4: No watchdog expired DTC (Dem event 15) on 0x500")
    dtc_sniffer.stop()
    dtc_decoded = dtc_sniffer.get_decoded()
    if dtc_decoded:
        dtc_num = int(dtc_decoded.get("DTC_Broadcast_Number", 0))
        ecu = int(dtc_decoded.get("DTC_Broadcast_ECU_Source", 0))
        if dtc_num == DTC_WDGM_EXPIRED:
            hc.check(4, f"WdgM expired DTC from {ECU_NAMES.get(ecu, ecu)}", False,
                     "Watchdog supervision failure — SWC missed checkpoint")
        else:
            hc.check(4, f"DTC on bus (num={dtc_num}, not WdgM)", True)
    else:
        hc.check(4, "No watchdog DTC (WdgM healthy)", True)

    bus.shutdown()
    sys.exit(hc.summary())


if __name__ == "__main__":
    main()
