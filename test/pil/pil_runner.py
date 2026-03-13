#!/usr/bin/env python3
"""
@file       pil_runner.py
@brief      PIL test runner — executes scenario YAML files against a single
            physical ECU (DUT) connected via CAN bus
@aspice     SWE.5 — Software Integration Testing
@iso        ISO 26262 Part 6, Section 10
@date       2026-03-10

PIL (Processor-in-the-Loop) tests validate real firmware on real hardware,
with the test harness simulating the vehicle environment:
  - Heartbeat injection: simulates all ECUs except the DUT
  - CAN observation: monitors DUT outputs (heartbeat, state, commands)
  - Fault injection: stops simulated heartbeats to trigger DUT fault handling

Usage:
    python pil_runner.py --channel can0 --interface socketcan
    python pil_runner.py --channel COM3 --interface slcan --dut cvc
    python pil_runner.py --filter PIL-001
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import sys
import time
from pathlib import Path
from typing import Any, Optional

import can
import yaml

try:
    from junit_xml import TestCase, TestSuite
except ImportError:
    TestCase = TestSuite = None  # type: ignore[assignment,misc]

# Add parent test dirs to path for can_helpers reuse
sys.path.insert(0, str(Path(__file__).parent.parent / "hil"))

from can_helpers import (
    check_e2e,
    check_heartbeat,
    crc8,
    measure_period,
    send_can,
    wait_for_message,
)
from heartbeat_injector import HeartbeatInjector

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [PIL] %(levelname)-5s %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("pil_runner")

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

ANSI_RED = "\033[0;31m"
ANSI_GREEN = "\033[0;32m"
ANSI_YELLOW = "\033[1;33m"
ANSI_BOLD = "\033[1m"
ANSI_NC = "\033[0m"

# CAN IDs
CAN_CVC_HEARTBEAT = 0x010
CAN_VEHICLE_STATE = 0x100
CAN_TORQUE_REQUEST = 0x101
CAN_DTC_BROADCAST = 0x500

VEHICLE_STATE_NAMES = {
    "INIT": 0, "RUN": 1, "DEGRADED": 2,
    "LIMP": 3, "SAFE_STOP": 4, "SHUTDOWN": 5,
}

MOTOR_RPM_BYTE_LO = 3
MOTOR_RPM_BYTE_HI = 4


def _state_name(state: int) -> str:
    for name, val in VEHICLE_STATE_NAMES.items():
        if val == state:
            return name
    return f"UNKNOWN({state})"


# ---------------------------------------------------------------------------
# CAN Monitor (simplified from HIL — single-threaded for PIL)
# ---------------------------------------------------------------------------

class PILMonitor:
    """CAN bus monitor for PIL testing.

    Captures messages from the DUT while the heartbeat injector runs
    in background threads.
    """

    def __init__(self, bus: can.Bus) -> None:
        self._bus = bus
        self._messages: dict[int, list[tuple[float, can.Message]]] = {}
        self._vehicle_state = 0
        self._state_transitions: list[tuple[float, int]] = []

    def poll(self, duration_sec: float = 0.5) -> None:
        """Poll CAN bus for the specified duration, capturing all messages."""
        deadline = time.monotonic() + duration_sec
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            msg = self._bus.recv(timeout=min(remaining, 0.05))
            if msg is None:
                continue
            ts = time.monotonic()
            arb_id = msg.arbitration_id
            if arb_id not in self._messages:
                self._messages[arb_id] = []
            self._messages[arb_id].append((ts, msg))

            # Track vehicle state from 0x100 byte[2] low nibble
            if arb_id == CAN_VEHICLE_STATE and len(msg.data) >= 3:
                new_state = msg.data[2] & 0x0F
                if new_state != self._vehicle_state:
                    self._state_transitions.append((ts, new_state))
                    log.debug("State: %s -> %s",
                              _state_name(self._vehicle_state),
                              _state_name(new_state))
                self._vehicle_state = new_state

    def reset(self) -> None:
        self._messages.clear()
        self._state_transitions.clear()
        self._vehicle_state = 0

    @property
    def vehicle_state(self) -> int:
        return self._vehicle_state

    @property
    def state_transitions(self) -> list[tuple[float, int]]:
        return list(self._state_transitions)

    def has_messages(self, can_id: int) -> bool:
        return can_id in self._messages and len(self._messages[can_id]) > 0

    def get_latest(self, can_id: int) -> Optional[can.Message]:
        msgs = self._messages.get(can_id, [])
        return msgs[-1][1] if msgs else None

    def get_history(self, can_id: int) -> list[tuple[float, can.Message]]:
        return list(self._messages.get(can_id, []))

    def get_timestamps(self, can_id: int) -> list[float]:
        return [ts for ts, _ in self._messages.get(can_id, [])]

    def wait_for_state(self, target: int, timeout_sec: float = 10.0) -> bool:
        deadline = time.monotonic() + timeout_sec
        while time.monotonic() < deadline:
            self.poll(0.1)
            if self._vehicle_state == target:
                return True
            for _, s in self._state_transitions:
                if s == target:
                    return True
        return False


# ---------------------------------------------------------------------------
# Verdict Evaluation
# ---------------------------------------------------------------------------

class VerdictResult:
    def __init__(self, desc: str, expected: str, observed: str, passed: bool):
        self.description = desc
        self.expected = expected
        self.observed = observed
        self.passed = passed


def evaluate_verdict(
    vdef: dict,
    monitor: PILMonitor,
    bus: can.Bus,
    channel: str,
    interface: str,
) -> VerdictResult:
    """Evaluate a single verdict definition against observed data."""
    vtype = vdef.get("type", "")
    desc = vdef.get("description", "")

    if vtype == "heartbeat":
        can_id = vdef.get("can_id", 0)
        period_ms = vdef.get("period_ms", 50)
        tolerance = vdef.get("tolerance_pct", 20.0)
        duration = vdef.get("duration_sec", 3.0)

        try:
            check_bus = can.interface.Bus(channel=channel, interface=interface)
            result = check_heartbeat(check_bus, can_id, period_ms, tolerance, duration)
            check_bus.shutdown()
        except Exception as exc:
            return VerdictResult(desc, f"0x{can_id:03X} @ {period_ms}ms",
                                 f"error: {exc}", False)
        return VerdictResult(
            desc,
            f"0x{can_id:03X} @ {period_ms}ms ±{tolerance}%",
            f"avg={result['avg_period_ms']}ms, count={result['count']}, "
            f"jitter={result['jitter_ms']}ms",
            result["passed"],
        )

    elif vtype == "vehicle_state":
        expected_name = vdef.get("expected", "RUN")
        within_ms = vdef.get("within_ms", 15000)
        expected_val = VEHICLE_STATE_NAMES.get(expected_name, -1)

        reached = monitor.wait_for_state(expected_val, within_ms / 1000.0)
        return VerdictResult(
            desc, expected_name,
            _state_name(monitor.vehicle_state),
            reached,
        )

    elif vtype == "can_message":
        can_id = vdef.get("can_id", 0)
        timeout = vdef.get("timeout_sec", 5.0)
        monitor.poll(timeout)
        if monitor.has_messages(can_id):
            msg = monitor.get_latest(can_id)
            return VerdictResult(
                desc, f"0x{can_id:03X} present",
                f"0x{can_id:03X} data={msg.data.hex()}" if msg else "seen",
                True,
            )
        return VerdictResult(desc, f"0x{can_id:03X} present", "not received", False)

    elif vtype == "e2e":
        can_id = vdef.get("can_id", 0)
        monitor.poll(3.0)
        msg = monitor.get_latest(can_id)
        if msg is None:
            return VerdictResult(desc, f"E2E valid on 0x{can_id:03X}",
                                 "no message", False)
        result = check_e2e(msg)
        return VerdictResult(
            desc,
            f"E2E CRC valid on 0x{can_id:03X}",
            f"CRC expected=0x{result.get('crc_expected', 0):02X} "
            f"actual=0x{result.get('crc_actual', 0):02X} "
            f"alive={result.get('alive_counter', -1)}",
            result["passed"],
        )

    elif vtype == "e2e_alive_increment":
        can_id = vdef.get("can_id", 0)
        min_samples = vdef.get("min_samples", 5)
        # Collect multiple messages and check alive counter increments
        monitor.poll(3.0)
        history = monitor.get_history(can_id)
        if len(history) < min_samples:
            return VerdictResult(desc, f">={min_samples} messages",
                                 f"only {len(history)}", False)
        alive_values = []
        for _, msg in history[-min_samples:]:
            alive = msg.data[0] >> 4  # high nibble of byte 0
            alive_values.append(alive)

        # Check monotonic increment (mod 16)
        increments_ok = True
        for i in range(1, len(alive_values)):
            expected = (alive_values[i - 1] + 1) & 0x0F
            if alive_values[i] != expected:
                increments_ok = False
                break

        return VerdictResult(
            desc,
            "alive counter increments by 1",
            f"sequence: {alive_values}",
            increments_ok,
        )

    elif vtype == "timing":
        can_id = vdef.get("can_id", 0)
        expected_ms = vdef.get("period_ms", 10)
        tolerance = vdef.get("tolerance_pct", 20.0)

        try:
            check_bus = can.interface.Bus(channel=channel, interface=interface)
            result = measure_period(check_bus, can_id, sample_count=20, timeout_sec=5.0)
            check_bus.shutdown()
        except Exception as exc:
            return VerdictResult(desc, f"{expected_ms}ms period",
                                 f"error: {exc}", False)

        if result["samples"] < 2:
            return VerdictResult(desc, f"{expected_ms}ms period",
                                 "insufficient samples", False)

        lo = expected_ms * (1 - tolerance / 100)
        hi = expected_ms * (1 + tolerance / 100)
        avg = result["avg_ms"]
        return VerdictResult(
            desc,
            f"{expected_ms}ms ±{tolerance}%",
            f"avg={avg}ms (min={result['min_ms']}, max={result['max_ms']})",
            lo <= avg <= hi,
        )

    elif vtype == "dtc_broadcast":
        within_ms = vdef.get("within_ms", 10000)
        monitor.poll(within_ms / 1000.0)
        if monitor.has_messages(CAN_DTC_BROADCAST):
            msg = monitor.get_latest(CAN_DTC_BROADCAST)
            return VerdictResult(desc, "DTC broadcast present",
                                 f"data={msg.data.hex()}" if msg else "seen", True)
        return VerdictResult(desc, "DTC broadcast present", "not received", False)

    elif vtype == "state_transition":
        from_state = vdef.get("from", "RUN")
        to_state = vdef.get("to", "DEGRADED")
        within_ms = vdef.get("within_ms", 10000)
        from_val = VEHICLE_STATE_NAMES.get(from_state, -1)
        to_val = VEHICLE_STATE_NAMES.get(to_state, -1)

        monitor.poll(within_ms / 1000.0)
        transitions = monitor.state_transitions
        for _, s in transitions:
            if s == to_val:
                return VerdictResult(
                    desc, f"{from_state} -> {to_state}",
                    f"transition to {_state_name(s)} observed", True,
                )
        return VerdictResult(
            desc, f"{from_state} -> {to_state}",
            f"current={_state_name(monitor.vehicle_state)}, "
            f"transitions={[(t, _state_name(s)) for t, s in transitions]}",
            False,
        )

    else:
        return VerdictResult(desc, "known verdict type", f"unknown: {vtype}", False)


# ---------------------------------------------------------------------------
# Scenario Executor
# ---------------------------------------------------------------------------

def execute_scenario(
    scenario_path: Path,
    bus: can.Bus,
    monitor: PILMonitor,
    injector: HeartbeatInjector,
    channel: str,
    interface: str,
) -> dict[str, Any]:
    """Execute a single PIL scenario YAML."""
    with open(scenario_path, "r", encoding="utf-8") as fh:
        scenario = yaml.safe_load(fh)

    sid = scenario.get("id", scenario_path.stem)
    name = scenario.get("name", scenario_path.stem)
    desc = scenario.get("description", "")
    verifies = scenario.get("verifies", [])
    asil = scenario.get("asil", "QM")

    log.info("%s--- %s (%s) [ASIL %s] ---%s", ANSI_BOLD, name, sid, asil, ANSI_NC)
    log.info("  Verifies: %s", ", ".join(verifies))

    start_time = time.monotonic()
    monitor.reset()

    # --- Setup steps ---
    for step in scenario.get("setup", []):
        _execute_step(step, bus, monitor, injector)

    # Reset monitor for clean observation
    monitor.reset()

    # --- Execute steps ---
    for step in scenario.get("steps", []):
        _execute_step(step, bus, monitor, injector)

    # --- Evaluate verdicts ---
    verdict_results: list[VerdictResult] = []
    for vdef in scenario.get("verdicts", []):
        evidence = evaluate_verdict(vdef, monitor, bus, channel, interface)
        verdict_results.append(evidence)
        status = f"{ANSI_GREEN}PASS{ANSI_NC}" if evidence.passed \
            else f"{ANSI_RED}FAIL{ANSI_NC}"
        log.info("  [%s] %s", status, evidence.description)
        if not evidence.passed:
            log.info("    Expected: %s | Observed: %s",
                     evidence.expected, evidence.observed)

    # --- Teardown ---
    for step in scenario.get("teardown", []):
        _execute_step(step, bus, monitor, injector)

    all_passed = all(v.passed for v in verdict_results)
    duration = time.monotonic() - start_time

    result_text = f"{ANSI_GREEN}PASS{ANSI_NC}" if all_passed \
        else f"{ANSI_RED}FAIL{ANSI_NC}"
    log.info("  Result: [%s] %s (%.1fs)\n", result_text, sid, duration)

    return {
        "id": sid, "name": name, "description": desc,
        "verifies": verifies, "asil": asil,
        "passed": all_passed, "duration_sec": round(duration, 2),
        "verdicts": [
            {"description": v.description, "expected": v.expected,
             "observed": v.observed, "passed": v.passed}
            for v in verdict_results
        ],
    }


def _execute_step(
    step: dict,
    bus: can.Bus,
    monitor: PILMonitor,
    injector: HeartbeatInjector,
) -> None:
    action = step.get("action", "")
    desc = step.get("description", "")

    if action == "wait":
        seconds = step.get("seconds", 1)
        log.info("  Step: wait %ss — %s", seconds, desc)
        # Keep polling while waiting so we capture state transitions
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            monitor.poll(min(0.2, deadline - time.monotonic()))

    elif action == "wait_state":
        state_name = step.get("state", "RUN")
        state_val = VEHICLE_STATE_NAMES.get(state_name, 1)
        timeout = step.get("timeout", 20)
        log.info("  Step: wait_state %s (%ds) — %s", state_name, timeout, desc)
        if not monitor.wait_for_state(state_val, timeout):
            raise TimeoutError(
                f"Timed out waiting for {state_name} "
                f"(current: {_state_name(monitor.vehicle_state)})"
            )

    elif action == "inject_can":
        can_id = step.get("can_id", 0)
        data = step.get("data", [])
        log.info("  Step: inject_can 0x%03X %s — %s", can_id, data, desc)
        send_can(bus, can_id, data)

    elif action == "exclude_ecu":
        ecu_name = step.get("ecu", "")
        log.info("  Step: exclude_ecu %s — %s", ecu_name, desc)
        injector.exclude_ecu(ecu_name)

    elif action == "include_ecu":
        ecu_name = step.get("ecu", "")
        log.info("  Step: include_ecu %s — %s", ecu_name, desc)
        injector.include_ecu(ecu_name)

    elif action == "poll":
        seconds = step.get("seconds", 2)
        log.info("  Step: poll %ss — %s", seconds, desc)
        monitor.poll(seconds)

    else:
        log.warning("  Unknown step action: %s", action)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="PIL test runner for taktflow-embedded (single ECU DUT)",
    )
    parser.add_argument(
        "--scenarios", "-s", type=Path,
        default=Path(__file__).parent / "scenarios",
        help="Directory containing pil_*.yaml scenario files",
    )
    parser.add_argument(
        "--output", "-o", type=Path,
        default=Path(__file__).parent / "reports" / "pil-results.json",
        help="JSON results output path",
    )
    parser.add_argument(
        "--channel", "-c",
        default=os.environ.get("PIL_CAN_CHANNEL", "can0"),
        help="CAN channel (e.g. can0, COM3, env: PIL_CAN_CHANNEL)",
    )
    parser.add_argument(
        "--interface", "-i",
        default=os.environ.get("PIL_CAN_INTERFACE", "socketcan"),
        help="python-can interface (e.g. socketcan, slcan, pcan)",
    )
    parser.add_argument(
        "--bitrate", "-b", type=int, default=500000,
        help="CAN bitrate (default: 500000)",
    )
    parser.add_argument(
        "--dut", default="cvc",
        help="Device under test ECU name (default: cvc)",
    )
    parser.add_argument(
        "--filter", "-f", default=None,
        help="Run only scenarios matching this pattern",
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true",
        help="Enable debug logging",
    )
    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    # Discover scenarios
    scenario_dir = args.scenarios
    if not scenario_dir.is_dir():
        log.error("Scenario directory not found: %s", scenario_dir)
        return 1

    yaml_files = sorted(scenario_dir.glob("pil_*.yaml"))
    if not yaml_files:
        log.error("No pil_*.yaml files found in %s", scenario_dir)
        return 1

    if args.filter:
        yaml_files = [
            f for f in yaml_files
            if args.filter.lower() in f.stem.lower()
        ]

    log.info("Found %d scenario(s), DUT=%s", len(yaml_files), args.dut.upper())
    log.info("CAN: channel=%s, interface=%s, bitrate=%d",
             args.channel, args.interface, args.bitrate)

    # Open CAN bus
    bus_kwargs: dict[str, Any] = {
        "channel": args.channel,
        "interface": args.interface,
    }
    if args.interface in ("slcan", "serial"):
        bus_kwargs["bitrate"] = args.bitrate
    if args.interface == "socketcan":
        bus_kwargs["bitrate"] = args.bitrate

    try:
        bus = can.interface.Bus(**bus_kwargs)
    except Exception as exc:
        log.error("Cannot open CAN bus: %s", exc)
        log.error("Hint: check your adapter is connected and channel is correct")
        return 1

    monitor = PILMonitor(bus)

    # Start heartbeat injector (simulates all ECUs except DUT)
    injector = HeartbeatInjector(
        channel=args.channel,
        interface=args.interface,
        exclude=[args.dut.upper()],
    )
    injector.start()

    log.info("Heartbeat injector running — waiting 2s for DUT to sync...")
    monitor.poll(2.0)

    # Execute scenarios
    results: list[dict[str, Any]] = []
    for scenario_path in yaml_files:
        try:
            result = execute_scenario(
                scenario_path, bus, monitor, injector,
                args.channel, args.interface,
            )
            results.append(result)
        except Exception as exc:
            log.error("Scenario %s crashed: %s", scenario_path.name, exc)
            results.append({
                "id": scenario_path.stem, "name": scenario_path.stem,
                "passed": False, "error": str(exc),
            })

    # Cleanup
    injector.stop()
    bus.shutdown()

    # Summary
    total = len(results)
    passed = sum(1 for r in results if r.get("passed"))
    failed = total - passed

    print(f"\n{'=' * 60}")
    print(f"  PIL Test Results — DUT: {args.dut.upper()}")
    print(f"{'=' * 60}")
    print(f"  Total:  {total}")
    print(f"  Passed: {ANSI_GREEN}{passed}{ANSI_NC}")
    print(f"  Failed: {ANSI_RED}{failed}{ANSI_NC}")
    print(f"{'=' * 60}\n")

    # Write results JSON
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as fh:
        json.dump({"dut": args.dut, "results": results}, fh, indent=2)
    log.info("Results written to %s", args.output)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
