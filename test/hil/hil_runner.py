#!/usr/bin/env python3
"""
@file       hil_runner.py
@brief      HIL test runner — executes scenario YAML files against the
            Pi-based HIL bench and evaluates pass/fail verdicts
@aspice     SWE.6 — Software Qualification Testing
@iso        ISO 26262 Part 4, Section 7 — HSI Verification
@date       2026-03-08

Forked from test/sil/verdict_checker.py for HIL-specific operation:
  - CAN channel: can0 (physical) instead of vcan0 (virtual)
  - Fault injection: MQTT (mosquitto on Pi) instead of REST API
  - Timing: tighter tolerances (Linux RT vs Docker jitter)
  - Traceability: verifies field includes TSR/SSR/SWR with ASIL level

Observation channels:
    1. CAN bus (python-can on can0) — primary real-time observation
    2. MQTT (paho-mqtt)             — fault injection + telemetry

Vehicle states (from plant_sim/simulator.py):
    0=INIT, 1=RUN, 2=DEGRADED, 3=LIMP, 4=SAFE_STOP, 5=SHUTDOWN
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import struct
import sys
import threading
import time
from dataclasses import dataclass, field
from enum import IntEnum
from pathlib import Path
from typing import Any, Optional

import can
import yaml

try:
    from junit_xml import TestCase, TestSuite
except ImportError:
    TestCase = TestSuite = None  # type: ignore[assignment,misc]

try:
    import paho.mqtt.client as paho_mqtt
except ImportError:
    paho_mqtt = None  # type: ignore[assignment]

from can_helpers import (
    check_e2e,
    check_heartbeat,
    crc8,
    decode_signal,
    inject_mqtt_fault,
    measure_period,
    reset_mqtt_faults,
    send_can,
    wait_for_message,
)

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [HIL] %(levelname)-5s %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("hil_runner")

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

ANSI_RED = "\033[0;31m"
ANSI_GREEN = "\033[0;32m"
ANSI_YELLOW = "\033[1;33m"
ANSI_CYAN = "\033[0;36m"
ANSI_BOLD = "\033[1m"
ANSI_NC = "\033[0m"


class VehicleState(IntEnum):
    """Vehicle state codes — must match plant_sim/simulator.py."""
    INIT = 0
    RUN = 1
    DEGRADED = 2
    LIMP = 3
    SAFE_STOP = 4
    SHUTDOWN = 5


VEHICLE_STATE_NAMES: dict[str, int] = {s.name: s.value for s in VehicleState}

# CAN IDs
CAN_ESTOP = 0x001
CAN_CVC_HEARTBEAT = 0x010
CAN_FZC_HEARTBEAT = 0x011
CAN_RZC_HEARTBEAT = 0x012
CAN_SC_STATUS = 0x013
CAN_VEHICLE_STATE = 0x100
CAN_TORQUE_REQUEST = 0x101
CAN_STEER_COMMAND = 0x102
CAN_BRAKE_COMMAND = 0x103
CAN_STEERING_STATUS = 0x200
CAN_BRAKE_STATUS = 0x201
CAN_LIDAR_DISTANCE = 0x220
CAN_MOTOR_STATUS = 0x300
CAN_MOTOR_CURRENT = 0x301
CAN_MOTOR_TEMP = 0x302
CAN_BATTERY_STATUS = 0x303
CAN_DTC_BROADCAST = 0x500
CAN_FZC_VIRTUAL_SENSORS = 0x600
CAN_RZC_VIRTUAL_SENSORS = 0x601

# Defaults
DEFAULT_CAN_CHANNEL = "can0"
DEFAULT_MQTT_HOST = "localhost"
DEFAULT_MQTT_PORT = 1883
DEFAULT_SCENARIO_TIMEOUT_SEC = 60

# Motor RPM byte positions in Motor_Status (0x300)
MOTOR_RPM_BYTE_LO = 3
MOTOR_RPM_BYTE_HI = 4


def _state_name(state: int) -> str:
    """Human-readable vehicle state name."""
    for s in VehicleState:
        if s.value == state:
            return s.name
    return f"UNKNOWN({state})"


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------

@dataclass
class VerdictEvidence:
    """Evidence collected during verdict evaluation."""
    description: str
    expected: str
    observed: str
    passed: bool
    timestamp: float = 0.0
    details: str = ""


@dataclass
class ScenarioResult:
    """Result of a single scenario execution."""
    scenario_id: str
    scenario_name: str
    description: str
    verifies: list[str]
    asil: str
    aspice: str
    passed: bool
    duration_sec: float
    verdicts: list[VerdictEvidence] = field(default_factory=list)
    error: Optional[str] = None


# ---------------------------------------------------------------------------
# CAN Bus Monitor (threaded)
# ---------------------------------------------------------------------------

class CANBusMonitor:
    """Threaded CAN bus monitor for HIL on can0.

    Runs a background listener and maintains:
      - Current vehicle state (from 0x100)
      - Latest CAN messages by arbitration ID
      - State transition history with timestamps
      - Motor RPM tracking
    """

    def __init__(self, channel: str = DEFAULT_CAN_CHANNEL,
                 interface: str = "socketcan") -> None:
        self.channel = channel
        self.interface = interface
        self._bus: Optional[can.Bus] = None
        self._thread: Optional[threading.Thread] = None
        self._running = False
        self._lock = threading.Lock()

        self._vehicle_state: int = VehicleState.INIT
        self._state_transitions: list[tuple[float, int]] = []
        self._latest_messages: dict[int, tuple[float, can.Message]] = {}
        self._message_history: dict[int, list[tuple[float, can.Message]]] = {}
        self._motor_rpm: int = 0
        self._can_ids_seen: set[int] = set()

    def start(self) -> None:
        """Start the CAN bus listener thread."""
        if self._running:
            return
        try:
            self._bus = can.interface.Bus(
                channel=self.channel,
                interface=self.interface,
            )
        except Exception as exc:
            log.error("Failed to open CAN bus '%s' (%s): %s",
                      self.channel, self.interface, exc)
            raise
        self._running = True
        self._thread = threading.Thread(
            target=self._listener_loop, daemon=True, name="hil-can-monitor",
        )
        self._thread.start()
        log.info("CAN bus monitor started on %s (%s)",
                 self.channel, self.interface)

    def stop(self) -> None:
        """Stop the CAN bus listener thread."""
        self._running = False
        if self._thread is not None:
            self._thread.join(timeout=3.0)
        if self._bus is not None:
            self._bus.shutdown()
            self._bus = None
        log.info("CAN bus monitor stopped")

    def reset(self) -> None:
        """Clear all captured state for a new scenario."""
        with self._lock:
            self._state_transitions.clear()
            self._latest_messages.clear()
            self._message_history.clear()
            self._motor_rpm = 0
            self._can_ids_seen.clear()
            self._vehicle_state = VehicleState.INIT

    @property
    def vehicle_state(self) -> int:
        with self._lock:
            return self._vehicle_state

    @property
    def motor_rpm(self) -> int:
        with self._lock:
            return self._motor_rpm

    @property
    def state_transitions(self) -> list[tuple[float, int]]:
        with self._lock:
            return list(self._state_transitions)

    def get_latest_message(self, can_id: int) -> Optional[tuple[float, can.Message]]:
        with self._lock:
            return self._latest_messages.get(can_id)

    def get_message_history(self, can_id: int) -> list[tuple[float, can.Message]]:
        with self._lock:
            return list(self._message_history.get(can_id, []))

    def has_seen_can_id(self, can_id: int) -> bool:
        with self._lock:
            return can_id in self._can_ids_seen

    def wait_for_state(self, target: int, timeout_sec: float = 10.0) -> bool:
        """Block until vehicle reaches target state or timeout."""
        deadline = time.monotonic() + timeout_sec
        while time.monotonic() < deadline:
            if self.vehicle_state == target:
                return True
            with self._lock:
                if any(s == target for _, s in self._state_transitions):
                    return True
            time.sleep(0.01)
        return False

    def wait_for_can_message(self, can_id: int,
                             timeout_sec: float = 5.0) -> Optional[can.Message]:
        """Wait until a specific CAN ID is received."""
        with self._lock:
            initial_count = len(self._message_history.get(can_id, []))
        deadline = time.monotonic() + timeout_sec
        while time.monotonic() < deadline:
            with self._lock:
                history = self._message_history.get(can_id, [])
                if len(history) > initial_count:
                    return history[-1][1]
            time.sleep(0.05)
        return None

    def _listener_loop(self) -> None:
        """Background CAN listener."""
        assert self._bus is not None
        while self._running:
            try:
                msg = self._bus.recv(timeout=0.1)
            except can.CanError:
                continue
            if msg is None:
                continue

            ts = time.monotonic()
            arb_id = msg.arbitration_id

            with self._lock:
                self._can_ids_seen.add(arb_id)
                self._latest_messages[arb_id] = (ts, msg)

                if arb_id not in self._message_history:
                    self._message_history[arb_id] = []
                hist = self._message_history[arb_id]
                hist.append((ts, msg))
                if len(hist) > 1000:
                    hist[:] = hist[-500:]

                # Track vehicle state from 0x100 byte[2] low nibble
                if arb_id == CAN_VEHICLE_STATE and len(msg.data) >= 3:
                    new_state = msg.data[2] & 0x0F
                    if new_state != self._vehicle_state:
                        self._state_transitions.append((ts, new_state))
                        log.debug("State: %s -> %s",
                                  _state_name(self._vehicle_state),
                                  _state_name(new_state))
                    self._vehicle_state = new_state

                # Track motor RPM from 0x300
                if arb_id == CAN_MOTOR_STATUS and len(msg.data) >= 5:
                    self._motor_rpm = (
                        msg.data[MOTOR_RPM_BYTE_LO]
                        | (msg.data[MOTOR_RPM_BYTE_HI] << 8)
                    )


# ---------------------------------------------------------------------------
# MQTT Monitor (optional — for fault injection and telemetry)
# ---------------------------------------------------------------------------

class MQTTMonitor:
    """MQTT subscriber for HIL fault injection and telemetry observation."""

    def __init__(self, host: str = DEFAULT_MQTT_HOST,
                 port: int = DEFAULT_MQTT_PORT) -> None:
        self._host = host
        self._port = port
        self._client = None
        self._lock = threading.Lock()
        self._messages: dict[str, tuple[float, dict[str, Any]]] = {}
        self._connected = False
        self._available = paho_mqtt is not None

    @property
    def available(self) -> bool:
        return self._available and self._connected

    def start(self) -> None:
        if not self._available:
            log.warning("paho-mqtt not installed — MQTT monitor disabled")
            return
        try:
            self._client = paho_mqtt.Client(
                paho_mqtt.CallbackAPIVersion.VERSION2,
                client_id="taktflow-hil-runner",
            )
            self._client.on_connect = self._on_connect
            self._client.on_message = self._on_message
            self._client.connect(self._host, self._port, keepalive=30)
            self._client.loop_start()
            log.info("MQTT monitor connecting to %s:%d",
                     self._host, self._port)
        except Exception as exc:
            log.warning("MQTT unavailable: %s (fault injection tests will skip)",
                        exc)
            self._available = False

    def stop(self) -> None:
        if self._client is not None:
            self._client.loop_stop()
            self._client.disconnect()
            self._client = None

    def reset(self) -> None:
        with self._lock:
            self._messages.clear()

    def get_latest(self, topic: str) -> Optional[dict[str, Any]]:
        with self._lock:
            entry = self._messages.get(topic)
            return entry[1] if entry else None

    def _on_connect(self, client, userdata, flags, rc, properties=None):
        rc_val = rc.value if hasattr(rc, 'value') else rc
        if rc_val == 0:
            self._connected = True
            client.subscribe("taktflow/#", qos=0)
            client.subscribe("vehicle/#", qos=0)
            log.info("MQTT connected and subscribed")
        else:
            log.warning("MQTT connect failed rc=%s", rc)

    def _on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError):
            payload = {"raw": msg.payload.hex()}
        with self._lock:
            self._messages[msg.topic] = (time.monotonic(), payload)


# ---------------------------------------------------------------------------
# Scenario Executor
# ---------------------------------------------------------------------------

class ScenarioExecutor:
    """Executes HIL scenario YAML files and evaluates verdicts.

    Steps:
      1. Setup (wait for state, reset faults)
      2. Execute steps (CAN injection, MQTT faults, waits)
      3. Evaluate verdicts (CAN presence, state, heartbeat, E2E, signals)
      4. Teardown (reset faults, zero commands)
    """

    def __init__(
        self,
        can_monitor: CANBusMonitor,
        mqtt_monitor: MQTTMonitor,
        mqtt_host: str = DEFAULT_MQTT_HOST,
        mqtt_port: int = DEFAULT_MQTT_PORT,
    ) -> None:
        self._can = can_monitor
        self._mqtt = mqtt_monitor
        self._mqtt_host = mqtt_host
        self._mqtt_port = mqtt_port
        self._inject_bus: Optional[can.Bus] = None

    def set_inject_bus(self, bus: can.Bus) -> None:
        """Set a CAN bus instance for frame injection."""
        self._inject_bus = bus

    def execute_scenario(self, scenario_path: Path) -> ScenarioResult:
        """Execute a single scenario YAML file."""
        with open(scenario_path, "r", encoding="utf-8") as fh:
            scenario = yaml.safe_load(fh)

        sid = scenario.get("id", scenario_path.stem)
        name = scenario.get("name", scenario_path.stem)
        desc = scenario.get("description", "")
        verifies = scenario.get("verifies", [])
        asil = scenario.get("asil", "QM")
        aspice = scenario.get("aspice", "SWE.6")
        timeout_sec = scenario.get("timeout_sec", DEFAULT_SCENARIO_TIMEOUT_SEC)
        requires_mqtt = scenario.get("requires_mqtt", False)

        log.info("%s--- %s (%s) [ASIL %s] ---%s",
                 ANSI_BOLD, name, sid, asil, ANSI_NC)
        log.info("  Verifies: %s", ", ".join(verifies))

        # Skip MQTT-dependent tests if MQTT unavailable
        if requires_mqtt and not self._mqtt.available:
            log.warning("  %sSKIPPED%s — requires MQTT (not available)",
                        ANSI_YELLOW, ANSI_NC)
            return ScenarioResult(
                scenario_id=sid, scenario_name=name, description=desc,
                verifies=verifies, asil=asil, aspice=aspice,
                passed=False, duration_sec=0,
                error="MQTT_UNAVAILABLE — mosquitto not running",
            )

        start_time = time.monotonic()
        self._can.reset()
        self._mqtt.reset()

        # --- Setup ---
        try:
            for step in scenario.get("setup", []):
                self._execute_step(step, timeout_sec)
        except Exception as exc:
            return ScenarioResult(
                scenario_id=sid, scenario_name=name, description=desc,
                verifies=verifies, asil=asil, aspice=aspice,
                passed=False, duration_sec=time.monotonic() - start_time,
                error=f"Setup failed: {exc}",
            )

        # Reset monitors after setup for clean observation window
        self._can.reset()
        self._mqtt.reset()

        # Re-sync vehicle state from live CAN
        self._can.wait_for_can_message(CAN_VEHICLE_STATE, timeout_sec=3.0)

        observation_start = time.monotonic()

        # --- Execute steps ---
        try:
            for step in scenario.get("steps", []):
                self._execute_step(step, timeout_sec)
        except Exception as exc:
            return ScenarioResult(
                scenario_id=sid, scenario_name=name, description=desc,
                verifies=verifies, asil=asil, aspice=aspice,
                passed=False, duration_sec=time.monotonic() - start_time,
                error=f"Step failed: {exc}",
            )

        # --- Evaluate verdicts ---
        verdict_defs = scenario.get("verdicts", [])
        verdict_results: list[VerdictEvidence] = []

        for vdef in verdict_defs:
            evidence = self._evaluate_verdict(vdef, observation_start)
            verdict_results.append(evidence)
            status = f"{ANSI_GREEN}PASS{ANSI_NC}" if evidence.passed \
                else f"{ANSI_RED}FAIL{ANSI_NC}"
            log.info("  [%s] %s", status, evidence.description)
            if not evidence.passed:
                log.info("    Expected: %s | Observed: %s",
                         evidence.expected, evidence.observed)

        # --- Teardown ---
        for step in scenario.get("teardown", []):
            try:
                self._execute_step(step, 10)
            except Exception as exc:
                log.warning("  Teardown step failed: %s", exc)

        all_passed = all(v.passed for v in verdict_results)
        duration = time.monotonic() - start_time

        result_text = f"{ANSI_GREEN}PASS{ANSI_NC}" if all_passed \
            else f"{ANSI_RED}FAIL{ANSI_NC}"
        log.info("  Result: [%s] %s (%.1fs)\n", result_text, sid, duration)

        return ScenarioResult(
            scenario_id=sid, scenario_name=name, description=desc,
            verifies=verifies, asil=asil, aspice=aspice,
            passed=all_passed, duration_sec=duration,
            verdicts=verdict_results,
        )

    # --- Step execution ---

    def _execute_step(self, step: dict, timeout_sec: int) -> None:
        action = step.get("action", "")
        desc = step.get("description", "")

        if action == "wait":
            seconds = step.get("seconds", 1)
            log.info("  Step: wait %ds — %s", seconds, desc)
            time.sleep(seconds)

        elif action == "wait_state":
            state_name = step.get("state", "RUN")
            state_val = VEHICLE_STATE_NAMES.get(state_name, 1)
            wait_timeout = step.get("timeout", 20)
            log.info("  Step: wait_state %s (%ds) — %s",
                     state_name, wait_timeout, desc)
            if not self._can.wait_for_state(state_val, wait_timeout):
                raise TimeoutError(
                    f"Timed out waiting for state {state_name} "
                    f"(current: {_state_name(self._can.vehicle_state)})"
                )

        elif action == "inject_can":
            can_id = step.get("can_id", 0)
            data = step.get("data", [])
            log.info("  Step: inject_can 0x%03X %s — %s", can_id, data, desc)
            if self._inject_bus:
                send_can(self._inject_bus, can_id, data)
            else:
                log.warning("  No inject bus — skipping CAN injection")

        elif action == "inject_mqtt":
            topic = step.get("topic", "taktflow/command/plant_inject")
            payload = step.get("payload", {})
            log.info("  Step: inject_mqtt %s — %s", topic, desc)
            inject_mqtt_fault(self._mqtt_host, topic, payload, self._mqtt_port)

        elif action == "reset_faults":
            log.info("  Step: reset_faults — %s", desc)
            reset_mqtt_faults(self._mqtt_host, self._mqtt_port)

        elif action == "reset":
            log.info("  Step: reset — %s", desc)
            reset_mqtt_faults(self._mqtt_host, self._mqtt_port)
            # Zero all actuator commands
            if self._inject_bus:
                send_can(self._inject_bus, CAN_TORQUE_REQUEST,
                         [0] * 8)
                send_can(self._inject_bus, CAN_STEER_COMMAND,
                         [0] * 8)
                send_can(self._inject_bus, CAN_BRAKE_COMMAND,
                         [0] * 8)

        else:
            log.warning("  Unknown step action: %s", action)

    # --- Verdict evaluation ---

    def _evaluate_verdict(self, vdef: dict,
                          observation_start: float) -> VerdictEvidence:
        vtype = vdef.get("type", "")
        desc = vdef.get("description", "")

        if vtype == "vehicle_state":
            return self._verdict_vehicle_state(vdef, desc)
        elif vtype == "can_message":
            return self._verdict_can_message(vdef, desc)
        elif vtype == "can_absence":
            return self._verdict_can_absence(vdef, desc)
        elif vtype == "heartbeat":
            return self._verdict_heartbeat(vdef, desc)
        elif vtype == "e2e":
            return self._verdict_e2e(vdef, desc)
        elif vtype == "signal_range":
            return self._verdict_signal_range(vdef, desc)
        elif vtype == "motor_shutdown":
            return self._verdict_motor_shutdown(vdef, desc)
        elif vtype == "dtc_broadcast":
            return self._verdict_dtc_broadcast(vdef, desc)
        elif vtype == "timing":
            return self._verdict_timing(vdef, desc)
        else:
            return VerdictEvidence(
                description=desc, expected="known verdict type",
                observed=f"unknown: {vtype}", passed=False,
            )

    def _verdict_vehicle_state(self, vdef: dict, desc: str) -> VerdictEvidence:
        expected_name = vdef.get("expected", "RUN")
        within_ms = vdef.get("within_ms", 10000)

        # Support list of acceptable states
        if isinstance(expected_name, list):
            expected_vals = [VEHICLE_STATE_NAMES.get(s, -1) for s in expected_name]
            expected_str = "/".join(expected_name)
        else:
            expected_vals = [VEHICLE_STATE_NAMES.get(expected_name, -1)]
            expected_str = expected_name

        deadline = time.monotonic() + within_ms / 1000.0
        while time.monotonic() < deadline:
            if self._can.vehicle_state in expected_vals:
                return VerdictEvidence(
                    description=desc, expected=expected_str,
                    observed=_state_name(self._can.vehicle_state),
                    passed=True, timestamp=time.monotonic(),
                )
            # Check transition history
            for ts, s in self._can.state_transitions:
                if s in expected_vals:
                    return VerdictEvidence(
                        description=desc, expected=expected_str,
                        observed=_state_name(s), passed=True, timestamp=ts,
                    )
            time.sleep(0.01)

        return VerdictEvidence(
            description=desc, expected=expected_str,
            observed=_state_name(self._can.vehicle_state),
            passed=False,
        )

    def _verdict_can_message(self, vdef: dict, desc: str) -> VerdictEvidence:
        can_id = vdef.get("can_id", 0)
        timeout = vdef.get("timeout_sec", 5.0)

        msg = self._can.wait_for_can_message(can_id, timeout)
        if msg is None:
            # Check if already seen
            if self._can.has_seen_can_id(can_id):
                return VerdictEvidence(
                    description=desc,
                    expected=f"0x{can_id:03X} present",
                    observed="seen in history",
                    passed=True,
                )
            return VerdictEvidence(
                description=desc,
                expected=f"0x{can_id:03X} present",
                observed="not received",
                passed=False,
            )

        # Optional value checks
        min_val = vdef.get("min_value")
        max_val = vdef.get("max_value")
        byte_offset = vdef.get("byte_offset", 0)

        if min_val is not None or max_val is not None:
            value = struct.unpack_from("<H", msg.data, byte_offset)[0] \
                if len(msg.data) >= byte_offset + 2 else 0
            in_range = True
            if min_val is not None and value < min_val:
                in_range = False
            if max_val is not None and value > max_val:
                in_range = False
            return VerdictEvidence(
                description=desc,
                expected=f"0x{can_id:03X} value in [{min_val}, {max_val}]",
                observed=f"value={value}",
                passed=in_range,
            )

        return VerdictEvidence(
            description=desc,
            expected=f"0x{can_id:03X} present",
            observed=f"0x{can_id:03X} data={msg.data.hex()}",
            passed=True,
        )

    def _verdict_can_absence(self, vdef: dict, desc: str) -> VerdictEvidence:
        can_id = vdef.get("can_id", 0)
        window_sec = vdef.get("window_sec", 2.0)
        time.sleep(window_sec)
        recent = self._can.get_message_history(can_id)
        now = time.monotonic()
        recent_in_window = [t for t, _ in recent if now - t < window_sec]
        passed = len(recent_in_window) == 0
        return VerdictEvidence(
            description=desc,
            expected=f"0x{can_id:03X} absent for {window_sec}s",
            observed=f"{len(recent_in_window)} messages in window",
            passed=passed,
        )

    def _verdict_heartbeat(self, vdef: dict, desc: str) -> VerdictEvidence:
        can_id = vdef.get("can_id", 0)
        period_ms = vdef.get("period_ms", 50)
        tolerance = vdef.get("tolerance_pct", 15.0)
        duration = vdef.get("duration_sec", 2.0)

        # Use a temporary bus to avoid interference with monitor
        try:
            bus = can.interface.Bus(
                channel=self._can.channel,
                interface=self._can.interface,
            )
            result = check_heartbeat(bus, can_id, period_ms, tolerance, duration)
            bus.shutdown()
        except Exception as exc:
            return VerdictEvidence(
                description=desc, expected=f"0x{can_id:03X} @ {period_ms}ms",
                observed=f"error: {exc}", passed=False,
            )

        return VerdictEvidence(
            description=desc,
            expected=f"0x{can_id:03X} @ {period_ms}ms ±{tolerance}%",
            observed=f"avg={result['avg_period_ms']}ms, "
                     f"jitter={result['jitter_ms']}ms, "
                     f"count={result['count']}",
            passed=result["passed"],
        )

    def _verdict_e2e(self, vdef: dict, desc: str) -> VerdictEvidence:
        can_id = vdef.get("can_id", 0)
        msg = self._can.wait_for_can_message(can_id, timeout_sec=3.0)
        if msg is None:
            return VerdictEvidence(
                description=desc, expected=f"E2E valid on 0x{can_id:03X}",
                observed="no message", passed=False,
            )
        result = check_e2e(msg)
        return VerdictEvidence(
            description=desc,
            expected=f"E2E CRC valid on 0x{can_id:03X}",
            observed=f"CRC expected=0x{result.get('crc_expected', 0):02X} "
                     f"actual=0x{result.get('crc_actual', 0):02X} "
                     f"alive={result.get('alive_counter', -1)}",
            passed=result["passed"],
        )

    def _verdict_signal_range(self, vdef: dict, desc: str) -> VerdictEvidence:
        can_id = vdef.get("can_id", 0)
        byte_offset = vdef.get("byte_offset", 0)
        min_val = vdef.get("min_value", 0)
        max_val = vdef.get("max_value", 65535)

        msg = self._can.wait_for_can_message(can_id, timeout_sec=5.0)
        if msg is None:
            return VerdictEvidence(
                description=desc,
                expected=f"signal in [{min_val}, {max_val}]",
                observed="no message", passed=False,
            )
        value = struct.unpack_from("<H", msg.data, byte_offset)[0] \
            if len(msg.data) >= byte_offset + 2 else 0
        passed = min_val <= value <= max_val
        return VerdictEvidence(
            description=desc,
            expected=f"signal in [{min_val}, {max_val}]",
            observed=f"value={value}",
            passed=passed,
        )

    def _verdict_motor_shutdown(self, vdef: dict, desc: str) -> VerdictEvidence:
        within_ms = vdef.get("within_ms", 5000)
        deadline = time.monotonic() + within_ms / 1000.0
        while time.monotonic() < deadline:
            if self._can.motor_rpm == 0:
                return VerdictEvidence(
                    description=desc, expected="RPM=0",
                    observed="RPM=0", passed=True,
                )
            time.sleep(0.05)
        return VerdictEvidence(
            description=desc, expected="RPM=0",
            observed=f"RPM={self._can.motor_rpm}", passed=False,
        )

    def _verdict_dtc_broadcast(self, vdef: dict, desc: str) -> VerdictEvidence:
        dtc_code = vdef.get("dtc_code", 0)
        within_ms = vdef.get("within_ms", 5000)
        ecu_source = vdef.get("ecu_source")

        deadline = time.monotonic() + within_ms / 1000.0
        while time.monotonic() < deadline:
            history = self._can.get_message_history(CAN_DTC_BROADCAST)
            for _, msg in history:
                if len(msg.data) >= 4:
                    code = msg.data[0] | (msg.data[1] << 8)
                    source = msg.data[2]
                    if code == dtc_code:
                        if ecu_source is None or source == ecu_source:
                            return VerdictEvidence(
                                description=desc,
                                expected=f"DTC 0x{dtc_code:04X}",
                                observed=f"DTC 0x{code:04X} src={source}",
                                passed=True,
                            )
            time.sleep(0.1)

        return VerdictEvidence(
            description=desc,
            expected=f"DTC 0x{dtc_code:04X}",
            observed="not received",
            passed=False,
        )

    def _verdict_timing(self, vdef: dict, desc: str) -> VerdictEvidence:
        can_id = vdef.get("can_id", 0)
        expected_ms = vdef.get("period_ms", 10)
        tolerance = vdef.get("tolerance_pct", 15.0)

        try:
            bus = can.interface.Bus(
                channel=self._can.channel,
                interface=self._can.interface,
            )
            result = measure_period(bus, can_id, sample_count=20, timeout_sec=5.0)
            bus.shutdown()
        except Exception as exc:
            return VerdictEvidence(
                description=desc, expected=f"{expected_ms}ms period",
                observed=f"error: {exc}", passed=False,
            )

        if result["samples"] < 2:
            return VerdictEvidence(
                description=desc, expected=f"{expected_ms}ms period",
                observed="insufficient samples", passed=False,
            )

        lo = expected_ms * (1 - tolerance / 100)
        hi = expected_ms * (1 + tolerance / 100)
        avg = result["avg_ms"]
        passed = lo <= avg <= hi

        return VerdictEvidence(
            description=desc,
            expected=f"{expected_ms}ms ±{tolerance}%",
            observed=f"avg={avg}ms (min={result['min_ms']}, max={result['max_ms']})",
            passed=passed,
        )


# ---------------------------------------------------------------------------
# JUnit XML Output
# ---------------------------------------------------------------------------

def write_junit_xml(results: list[ScenarioResult], output_path: Path) -> None:
    """Write test results as JUnit XML for CI integration."""
    if TestSuite is None:
        log.warning("junit-xml not installed — skipping XML output")
        return

    test_cases = []
    for r in results:
        tc = TestCase(
            name=f"{r.scenario_id}: {r.scenario_name}",
            classname=f"hil.{r.asil}",
            elapsed_sec=r.duration_sec,
        )
        # Add requirement traceability as properties
        if hasattr(tc, 'properties'):
            for req in r.verifies:
                tc.properties = getattr(tc, 'properties', {})

        if r.error:
            tc.add_error_info(message=r.error)
        elif not r.passed:
            failures = [
                f"{v.description}: expected={v.expected} observed={v.observed}"
                for v in r.verdicts if not v.passed
            ]
            tc.add_failure_info(
                message=f"{len(failures)} verdict(s) failed",
                output="\n".join(failures),
            )

        # Stdout: verdict details
        tc.stdout = "\n".join(
            f"[{'PASS' if v.passed else 'FAIL'}] {v.description} "
            f"(expected={v.expected}, observed={v.observed})"
            for v in r.verdicts
        )

        test_cases.append(tc)

    ts = TestSuite("HIL Test Suite", test_cases)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as fh:
        TestSuite.to_file(fh, [ts], prettyprint=True)

    log.info("JUnit XML written to %s", output_path)


# ---------------------------------------------------------------------------
# Summary Report
# ---------------------------------------------------------------------------

def print_summary(results: list[ScenarioResult]) -> None:
    """Print a summary table of all scenario results."""
    total = len(results)
    passed = sum(1 for r in results if r.passed)
    failed = sum(1 for r in results if not r.passed and r.error is None)
    errors = sum(1 for r in results if r.error is not None)
    skipped = sum(1 for r in results if r.error and "UNAVAILABLE" in r.error)

    print(f"\n{'=' * 70}")
    print(f"  HIL Test Suite Results")
    print(f"{'=' * 70}")
    print(f"  Total:   {total}")
    print(f"  Passed:  {ANSI_GREEN}{passed}{ANSI_NC}")
    print(f"  Failed:  {ANSI_RED}{failed}{ANSI_NC}")
    print(f"  Errors:  {ANSI_RED}{errors}{ANSI_NC}")
    print(f"  Skipped: {ANSI_YELLOW}{skipped}{ANSI_NC}")
    print(f"{'=' * 70}")

    # Requirement coverage
    all_reqs: set[str] = set()
    passed_reqs: set[str] = set()
    for r in results:
        for req in r.verifies:
            all_reqs.add(req)
            if r.passed:
                passed_reqs.add(req)

    print(f"\n  Requirements tested: {len(all_reqs)}")
    print(f"  Requirements passed: {len(passed_reqs)}")

    # Per-ASIL breakdown
    asil_counts: dict[str, dict[str, int]] = {}
    for r in results:
        a = r.asil
        if a not in asil_counts:
            asil_counts[a] = {"total": 0, "passed": 0}
        asil_counts[a]["total"] += 1
        if r.passed:
            asil_counts[a]["passed"] += 1

    if asil_counts:
        print(f"\n  ASIL breakdown:")
        for asil in sorted(asil_counts.keys()):
            c = asil_counts[asil]
            print(f"    {asil}: {c['passed']}/{c['total']} passed")

    print(f"{'=' * 70}\n")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="HIL test runner for taktflow-embedded",
    )
    parser.add_argument(
        "--scenarios", "-s",
        type=Path,
        default=Path(__file__).parent / "scenarios",
        help="Directory containing scenario YAML files",
    )
    parser.add_argument(
        "--output", "-o",
        type=Path,
        default=Path(__file__).parent / "results" / "hil-results.xml",
        help="JUnit XML output path",
    )
    parser.add_argument(
        "--channel", "-c",
        default=os.environ.get("HIL_CAN_CHANNEL", DEFAULT_CAN_CHANNEL),
        help="CAN channel (default: can0, env: HIL_CAN_CHANNEL)",
    )
    parser.add_argument(
        "--interface", "-i",
        default=os.environ.get("HIL_CAN_INTERFACE", "socketcan"),
        help="python-can interface (default: socketcan)",
    )
    parser.add_argument(
        "--mqtt-host",
        default=os.environ.get("HIL_MQTT_HOST", DEFAULT_MQTT_HOST),
        help="MQTT broker host (default: localhost)",
    )
    parser.add_argument(
        "--mqtt-port",
        type=int,
        default=int(os.environ.get("HIL_MQTT_PORT", str(DEFAULT_MQTT_PORT))),
        help="MQTT broker port (default: 1883)",
    )
    parser.add_argument(
        "--filter", "-f",
        default=None,
        help="Run only scenarios matching this pattern (e.g. 'HIL-001')",
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable debug logging",
    )
    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    # Discover scenario files
    scenario_dir = args.scenarios
    if not scenario_dir.is_dir():
        log.error("Scenario directory not found: %s", scenario_dir)
        return 1

    yaml_files = sorted(scenario_dir.glob("hil_*.yaml"))
    if not yaml_files:
        log.error("No hil_*.yaml files found in %s", scenario_dir)
        return 1

    if args.filter:
        yaml_files = [
            f for f in yaml_files
            if args.filter.lower() in f.stem.lower()
               or args.filter.lower() in f.read_text(encoding="utf-8").lower()
        ]

    log.info("Found %d scenario(s) in %s", len(yaml_files), scenario_dir)

    # Start monitors
    can_monitor = CANBusMonitor(args.channel, args.interface)
    mqtt_monitor = MQTTMonitor(args.mqtt_host, args.mqtt_port)

    try:
        can_monitor.start()
    except Exception:
        log.error("Cannot open CAN bus — is can0 up?")
        return 1

    mqtt_monitor.start()

    # Open a second CAN bus for injection
    try:
        inject_bus = can.interface.Bus(
            channel=args.channel, interface=args.interface,
        )
    except Exception as exc:
        log.warning("Cannot open inject bus: %s", exc)
        inject_bus = None

    executor = ScenarioExecutor(
        can_monitor, mqtt_monitor, args.mqtt_host, args.mqtt_port,
    )
    if inject_bus:
        executor.set_inject_bus(inject_bus)

    # Execute scenarios
    results: list[ScenarioResult] = []
    for scenario_path in yaml_files:
        try:
            result = executor.execute_scenario(scenario_path)
            results.append(result)
        except Exception as exc:
            log.error("Scenario %s crashed: %s", scenario_path.name, exc)
            results.append(ScenarioResult(
                scenario_id=scenario_path.stem,
                scenario_name=scenario_path.stem,
                description="", verifies=[], asil="QM", aspice="SWE.6",
                passed=False, duration_sec=0,
                error=f"Unhandled exception: {exc}",
            ))

    # Cleanup
    if inject_bus:
        inject_bus.shutdown()
    can_monitor.stop()
    mqtt_monitor.stop()

    # Output
    print_summary(results)
    write_junit_xml(results, args.output)

    # Exit code: 0 if all passed, 1 otherwise
    return 0 if all(r.passed for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
