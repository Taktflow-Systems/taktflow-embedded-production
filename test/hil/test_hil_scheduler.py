#!/usr/bin/env python3
"""
@file       test_hil_scheduler.py
@brief      HIL platform scheduler timing verification
@verifies   SWR-SCHM-001, SWR-SCHM-002
@traces_to  TSR-025, TSR-026, TSR-027, SSR-CVC-011, SSR-FZC-018, SSR-RZC-011
@aspice     SWE.6 — Software Qualification Testing
@iso        ISO 26262 Part 4, Section 7 — HSI Verification

Verifies platform scheduler timing by measuring CAN frame periods on
physical ECUs. Unlike test_hil_heartbeat (which checks if heartbeats
exist), this test measures statistical timing precision to validate
the platform timer (GPT/RTI) and scheduler (SchM) implementation.

Pass criteria:
  - Mean period within ±5% of expected (bare-metal should be <±2%)
  - Standard deviation < 10% of expected (low jitter)
  - No gaps > 2× expected period (no missed scheduler ticks)

Measures:
  - CVC 0x010 @ 50ms  (STM32G474RE SchM 10ms tick, heartbeat every 5th)
  - FZC 0x011 @ 50ms  (STM32G474RE SchM 10ms tick)
  - RZC 0x012 @ 50ms  (STM32F413ZH SchM 10ms tick)
  - CVC 0x100 @ 10ms  (Vehicle_State — fastest cyclic, tests scheduler resolution)
  - RZC 0x302 @ 100ms (Motor_Temperature — slower cyclic)

Usage:
    python3 test/hil/test_hil_scheduler.py
"""

import math
import sys
import time

import can

from hil_test_lib import (
    CAN_CVC_HEARTBEAT, CAN_FZC_HEARTBEAT, CAN_RZC_HEARTBEAT,
    CAN_VEHICLE_STATE, CAN_MOTOR_TEMP,
    CAN_CHANNEL,
    open_bus, wait_for_all_heartbeats,
    precondition_all_ecus_healthy,
    print_header, HopChecker,
)


# ---------------------------------------------------------------------------
# Timing measurement
# ---------------------------------------------------------------------------

def measure_timing(bus, can_id, duration=5.0, sample_limit=200):
    """Collect timestamps for a CAN ID and compute timing statistics.

    Returns dict with:
      count, mean_ms, std_ms, min_ms, max_ms, max_gap_ms, missed_ticks
    """
    timestamps = []
    end = time.time() + duration
    while time.time() < end and len(timestamps) < sample_limit:
        msg = bus.recv(timeout=0.1)
        if msg and msg.arbitration_id == can_id:
            timestamps.append(time.time())

    if len(timestamps) < 5:
        return None

    deltas_ms = [(timestamps[i+1] - timestamps[i]) * 1000.0
                 for i in range(len(timestamps) - 1)]
    n = len(deltas_ms)
    mean = sum(deltas_ms) / n
    variance = sum((d - mean) ** 2 for d in deltas_ms) / n
    std = math.sqrt(variance)

    return {
        "count": len(timestamps),
        "mean_ms": round(mean, 2),
        "std_ms": round(std, 2),
        "min_ms": round(min(deltas_ms), 2),
        "max_ms": round(max(deltas_ms), 2),
        "max_gap_ms": round(max(deltas_ms), 2),
        "missed_ticks": sum(1 for d in deltas_ms if d > mean * 2.0),
    }


def check_timing(hc, hop, name, can_id, expected_ms, bus, duration=5.0):
    """Measure and validate timing for one CAN message."""
    stats = measure_timing(bus, can_id, duration=duration)
    if stats is None:
        hc.check(hop, f"{name} timing", False, f"0x{can_id:03X} not on bus")
        return

    mean = stats["mean_ms"]
    std = stats["std_ms"]
    max_gap = stats["max_gap_ms"]
    missed = stats["missed_ticks"]
    count = stats["count"]

    # Criteria
    mean_tol = expected_ms * 0.05  # ±5% mean tolerance
    std_limit = expected_ms * 0.10  # 10% std limit
    gap_limit = expected_ms * 2.0   # no gaps > 2× period

    mean_ok = abs(mean - expected_ms) < mean_tol
    std_ok = std < std_limit
    gap_ok = max_gap < gap_limit

    detail = (f"n={count} mean={mean:.1f}ms std={std:.1f}ms "
              f"min={stats['min_ms']:.1f}ms max={max_gap:.1f}ms "
              f"missed={missed}")

    all_ok = mean_ok and std_ok and gap_ok
    verdict = []
    if not mean_ok:
        verdict.append(f"mean {mean:.1f}ms outside ±5% of {expected_ms}ms")
    if not std_ok:
        verdict.append(f"std {std:.1f}ms > {std_limit:.1f}ms limit")
    if not gap_ok:
        verdict.append(f"max_gap {max_gap:.1f}ms > {gap_limit:.1f}ms")

    hc.check(hop, f"{name} {detail}",
             all_ok, " | ".join(verdict) if verdict else "")


def main():
    bus = open_bus()
    hc = HopChecker()

    print_header("Platform Scheduler Timing Verification")

    # Unified precondition: all ECUs healthy
    precondition_all_ecus_healthy(bus)
    print()

    # Hop 0: CVC heartbeat 0x010 @ 50ms — basic scheduler validation
    print("Hop 0: CVC heartbeat 0x010 @ 50ms (SchM 10ms × 5)")
    check_timing(hc, 0, "CVC_HB", CAN_CVC_HEARTBEAT, 50.0, bus)

    # Hop 1: FZC heartbeat 0x011 @ 50ms
    print("Hop 1: FZC heartbeat 0x011 @ 50ms")
    if not hc.stopped:
        check_timing(hc, 1, "FZC_HB", CAN_FZC_HEARTBEAT, 50.0, bus)

    # Hop 2: RZC heartbeat 0x012 @ 50ms
    print("Hop 2: RZC heartbeat 0x012 @ 50ms")
    if not hc.stopped:
        check_timing(hc, 2, "RZC_HB", CAN_RZC_HEARTBEAT, 50.0, bus)

    # Hop 3: Vehicle_State 0x100 @ 10ms — fastest cyclic, tests timer resolution
    print("Hop 3: Vehicle_State 0x100 @ 10ms (scheduler tick resolution)")
    if not hc.stopped:
        check_timing(hc, 3, "VS_100", CAN_VEHICLE_STATE, 10.0, bus, duration=3.0)

    # Hop 4: Motor_Temperature 0x302 @ 100ms — slower cyclic from RZC
    print("Hop 4: Motor_Temperature 0x302 @ 100ms (RZC slow cyclic)")
    if not hc.stopped:
        check_timing(hc, 4, "MT_302", CAN_MOTOR_TEMP, 100.0, bus, duration=5.0)

    # Hop 5: Cross-ECU phase check — verify CVC and FZC heartbeats don't
    # systematically collide (would indicate shared bus arbitration issues)
    print("Hop 5: Cross-ECU arbitration — CVC/FZC phase diversity")
    if not hc.stopped:
        cvc_ts = []
        fzc_ts = []
        end = time.time() + 3.0
        while time.time() < end:
            msg = bus.recv(timeout=0.05)
            if msg is None:
                continue
            if msg.arbitration_id == CAN_CVC_HEARTBEAT:
                cvc_ts.append(time.time())
            elif msg.arbitration_id == CAN_FZC_HEARTBEAT:
                fzc_ts.append(time.time())

        if len(cvc_ts) >= 10 and len(fzc_ts) >= 10:
            # Check that CVC and FZC don't always arrive within 1ms of each other
            collisions = 0
            for ct in cvc_ts:
                for ft in fzc_ts:
                    if abs(ct - ft) < 0.001:  # 1ms window
                        collisions += 1
                        break
            collision_pct = collisions / len(cvc_ts) * 100.0
            hc.check(5, f"CVC/FZC collision={collision_pct:.0f}% "
                     f"({collisions}/{len(cvc_ts)} frames within 1ms)",
                     collision_pct < 50,
                     f"{collision_pct:.0f}% collision rate — possible phase lock")
        else:
            hc.check(5, "Cross-ECU phase check", False,
                     f"Too few samples: CVC={len(cvc_ts)} FZC={len(fzc_ts)}")

    bus.shutdown()
    sys.exit(hc.summary())


if __name__ == "__main__":
    main()
