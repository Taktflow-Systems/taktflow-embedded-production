#!/usr/bin/env python3
"""
@file       hil_test_lib.py
@brief      Shared HIL test library — common patterns for mixed-bench tests
@aspice     SWE.6 — Software Qualification Testing
@iso        ISO 26262 Part 4, Section 7 — HSI Verification
@date       2026-03-23

Forked from test/sil/sil_test_lib.py for HIL-specific operation:
  - CAN channel: can0 (physical) via CAN bridge (vcan0 ↔ can0)
  - Physical ECUs: CVC/FZC/RZC (STM32), SC (TMS570)
  - vECUs: BCM/ICU/TCU (POSIX Docker on laptop)
  - Timing: tighter tolerances (bare-metal ±5ms vs Docker ±100ms)
  - SC on bus: system can reach RUN (not stuck in SAFE_STOP)
  - Fault injection: MQTT through laptop mosquitto
  - Recovery: physical ECU reset via NRST pin or power cycle (no container restart)

Topology:
    Laptop (vcan0) ↔ CAN bridge ↔ can0 ↔ Physical CAN bus
    vECUs: BCM, ICU, TCU (Docker)
    Physical: CVC, FZC, RZC (STM32G474RE), SC (TMS570LC4357)
"""

import json
import os
import subprocess
import sys
import threading
import time
from datetime import datetime, timezone

import can
import cantools
import paho.mqtt.publish as mqtt_pub

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

DBC_PATH = os.environ.get("DBC_PATH", "gateway/taktflow_vehicle.dbc")
MQTT_HOST = os.environ.get("MQTT_HOST", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_TOPIC = "taktflow/command/plant_inject"

# HIL uses can0 (physical) by default; test runner observes on can0 which
# sees all traffic from both vcan0 (bridged) and physical ECUs
CAN_CHANNEL = os.environ.get("CAN_INTERFACE", "can0")

# CAN IDs (must match gateway/taktflow.dbc)
CAN_ESTOP         = 0x001
CAN_CVC_HEARTBEAT = 0x010
CAN_FZC_HEARTBEAT = 0x011
CAN_RZC_HEARTBEAT = 0x012
CAN_SC_STATUS     = 0x013
CAN_VEHICLE_STATE = 0x100
CAN_TORQUE_REQ    = 0x101
CAN_STEER_CMD     = 0x102
CAN_BRAKE_CMD     = 0x103
CAN_STEERING_STATUS = 0x200
CAN_BRAKE_STATUS  = 0x201
CAN_LIDAR_DIST    = 0x220
CAN_MOTOR_STATUS  = 0x300
CAN_MOTOR_CURRENT = 0x301
CAN_MOTOR_TEMP    = 0x302
CAN_BATTERY       = 0x303
CAN_BCM_BODY      = 0x016  # BCM_Heartbeat (DBC ID 22 = 0x016)
CAN_DTC           = 0x500
CAN_FZC_VSENSOR   = 0x600
CAN_RZC_VSENSOR   = 0x601

STATE_NAMES = {
    0: "INIT", 1: "RUN", 2: "DEGRADED", 3: "LIMP",
    4: "SAFE_STOP", 5: "SHUTDOWN",
}

ECU_NAMES = {
    1: "CVC", 2: "FZC", 3: "RZC", 4: "SC",
    5: "BCM", 6: "ICU", 7: "TCU",
}

# HIL timing: bare-metal ECUs have tighter jitter than Docker/VPS
HIL_HEARTBEAT_TOLERANCE_PCT = float(os.environ.get("HIL_HB_TOL_PCT", "15"))
HIL_FTTI_BUDGET_MS = int(os.environ.get("HIL_FTTI_MS", "500"))


# ---------------------------------------------------------------------------
# Git + timestamp
# ---------------------------------------------------------------------------

def get_git_hash():
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            stderr=subprocess.DEVNULL, timeout=5,
        ).decode().strip()
    except Exception:
        return "unknown"


def print_header(test_name):
    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    git_hash = get_git_hash()
    print(f"=== {test_name} [HIL] ===")
    print(f"    Date: {now}  DUT: {git_hash}")
    print(f"    CAN:  {CAN_CHANNEL}  MQTT: {MQTT_HOST}:{MQTT_PORT}")
    print()


# ---------------------------------------------------------------------------
# CAN helpers
# ---------------------------------------------------------------------------

def open_bus():
    return can.interface.Bus(channel=CAN_CHANNEL, interface="socketcan")


def can_flush(bus):
    """Drain all buffered CAN frames."""
    while bus.recv(timeout=0) is not None:
        pass


def can_recv(bus, target_id, timeout=3.0):
    """Receive a specific CAN frame (flushes stale buffer first)."""
    can_flush(bus)
    end = time.time() + timeout
    while time.time() < end:
        msg = bus.recv(timeout=0.5)
        if msg and msg.arbitration_id == target_id:
            return msg
    return None


def can_recv_decoded(db, bus, target_id, timeout=3.0):
    """Receive and decode a CAN frame."""
    msg = can_recv(bus, target_id, timeout)
    if msg is None:
        return None
    return db.decode_message(target_id, msg.data, decode_choices=False)


def poll_signal(db, bus, can_id, signal_name, condition_fn, timeout=10.0):
    """Poll a CAN signal until condition_fn(value) is True.

    Returns (value, elapsed_ms) on success, (last_value, None) on timeout.
    """
    t_start = time.time()
    last_val = None
    while (time.time() - t_start) < timeout:
        decoded = can_recv_decoded(db, bus, can_id, timeout=2.0)
        if decoded and signal_name in decoded:
            val = decoded[signal_name]
            last_val = val
            if condition_fn(val):
                elapsed_ms = (time.time() - t_start) * 1000.0
                return val, elapsed_ms
    return last_val, None


def check_heartbeat_period(bus, can_id, expected_ms, duration=3.0):
    """Measure heartbeat period and return (avg_ms, jitter_ms, passed)."""
    timestamps = []
    end = time.time() + duration
    while time.time() < end:
        msg = bus.recv(timeout=0.1)
        if msg and msg.arbitration_id == can_id:
            timestamps.append(time.time())

    if len(timestamps) < 3:
        return 0, 0, False

    deltas = [(timestamps[i+1] - timestamps[i]) * 1000
              for i in range(len(timestamps) - 1)]
    avg = sum(deltas) / len(deltas)
    jitter = max(deltas) - min(deltas)
    tol = expected_ms * HIL_HEARTBEAT_TOLERANCE_PCT / 100.0
    passed = abs(avg - expected_ms) < tol
    return round(avg, 1), round(jitter, 1), passed


# ---------------------------------------------------------------------------
# MQTT injection
# ---------------------------------------------------------------------------

# CVC reset via SSH to Windows PC (CubeProgrammer --start triggers MCU reset)
CVC_RESET_HOST = os.environ.get("CVC_RESET_HOST", "")
CVC_RESET_CMD = os.environ.get("CVC_RESET_CMD", "")


def reset_cvc_hardware():
    """Reset CVC MCU via CubeProgrammer on Windows PC.

    Set CVC_RESET_HOST and CVC_RESET_CMD environment variables, or
    call with local command if running on Windows PC directly.
    Falls back to no-op if not configured.
    """
    cmd = CVC_RESET_CMD
    if not cmd:
        # Default: local CubeProgrammer reset (when running on Windows PC)
        stcli = os.environ.get("STCLI",
            "C:/Program Files (x86)/STMicroelectronics/STM32Cube/"
            "STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe")
        sn = os.environ.get("CVC_SN", "0027003C3235510B37333439")
        cmd = f'"{stcli}" -c port=SWD sn={sn} mode=UR --start'

    host = CVC_RESET_HOST
    if host:
        full_cmd = f"ssh {host} '{cmd}'"
    else:
        full_cmd = cmd

    print(f"  [RESET] CVC hardware reset...")
    try:
        subprocess.run(full_cmd, shell=True, timeout=15,
                      capture_output=True, text=True)
        time.sleep(2)  # Wait for MCU to restart
    except Exception as e:
        print(f"  [WARN] CVC reset failed: {e}")


def mqtt_inject(cmd_type, **kwargs):
    payload = {"type": cmd_type}
    payload.update(kwargs)
    mqtt_pub.single(MQTT_TOPIC, json.dumps(payload),
                    hostname=MQTT_HOST, port=MQTT_PORT)


def mqtt_reset():
    """Reset all faults to nominal."""
    mqtt_inject("reset")
    mqtt_inject("voltage", mV=12600, soc=100)
    mqtt_inject("clear_temp_override")
    mqtt_inject("inject_temp", temp_c=25.0)


# ---------------------------------------------------------------------------
# Preconditions — HIL-specific
# ---------------------------------------------------------------------------

def wait_cvc_run(db, bus, timeout=30.0, stable_sec=5.0):
    """Reset faults, wait for CVC RUN, verify stable.

    HIL difference from SIL: with SC on bus, system should reach RUN.
    If CVC is in a latched state (SAFE_STOP/SHUTDOWN), performs hardware
    reset via CubeProgrammer before waiting.
    """
    print("Precondition: Reset + waiting for stable CVC RUN state...")
    mqtt_reset()
    time.sleep(1)

    # Check current state — if latched, do hardware reset
    decoded = can_recv_decoded(db, bus, CAN_VEHICLE_STATE, timeout=3)
    if decoded:
        mode = int(decoded.get("Vehicle_State_Mode", 0))
        if mode >= 4:  # SAFE_STOP(4) or SHUTDOWN(5)
            print(f"  CVC in {STATE_NAMES.get(mode, mode)} (latched) — resetting...")
            reset_cvc_hardware()
            mqtt_reset()
            time.sleep(3)

    # Phase 1: wait for RUN
    val, elapsed = poll_signal(
        db, bus, CAN_VEHICLE_STATE, "Vehicle_State_Mode",
        lambda v: int(v) == 1, timeout=timeout,
    )
    if elapsed is None:
        state = STATE_NAMES.get(int(val), val) if val is not None else "NO_SIGNAL"
        print(f"  [FAIL] CVC state={state} after {timeout}s")
        return False
    print(f"  [OK] CVC reached RUN ({elapsed:.0f}ms), verifying stability...")

    # Phase 2: verify stays in RUN
    end = time.time() + stable_sec
    while time.time() < end:
        decoded = can_recv_decoded(db, bus, CAN_VEHICLE_STATE, timeout=2)
        if decoded:
            mode = int(decoded.get("Vehicle_State_Mode", 0))
            if mode != 1:
                print(f"  ... CVC dropped to {STATE_NAMES.get(mode, mode)}, retrying...")
                val, elapsed = poll_signal(
                    db, bus, CAN_VEHICLE_STATE, "Vehicle_State_Mode",
                    lambda v: int(v) == 1, timeout=timeout,
                )
                if elapsed is None:
                    print(f"  [FAIL] CVC did not re-enter RUN")
                    return False
                end = time.time() + stable_sec
    print(f"  [OK] CVC stable in RUN for {stable_sec}s")
    return True


def reset_and_wait_run(db, bus, timeout=30.0):
    """Reset all faults and wait for CVC to return to RUN.

    HIL: no container restart — physical ECUs reset via fault clearing only.
    Latched faults (overtemp) require physical reset (documented in test).
    """
    mqtt_reset()
    time.sleep(2)
    val, elapsed = poll_signal(
        db, bus, CAN_VEHICLE_STATE, "Vehicle_State_Mode",
        lambda v: int(v) == 1, timeout=timeout,
    )
    if elapsed is None:
        return False
    # Verify stability (shorter than SIL — bare-metal is faster)
    end = time.time() + 5.0
    while time.time() < end:
        decoded = can_recv_decoded(db, bus, CAN_VEHICLE_STATE, timeout=2)
        if decoded and int(decoded.get("Vehicle_State_Mode", 0)) != 1:
            val2, elapsed2 = poll_signal(
                db, bus, CAN_VEHICLE_STATE, "Vehicle_State_Mode",
                lambda v: int(v) == 1, timeout=timeout,
            )
            if elapsed2 is None:
                return False
            end = time.time() + 5.0
    return True


def verify_normal_operation(db, bus, duration=5.0):
    """Hop 0: Verify CVC stays in RUN for duration (negative test)."""
    end = time.time() + duration
    while time.time() < end:
        decoded = can_recv_decoded(db, bus, CAN_VEHICLE_STATE, timeout=2)
        if decoded:
            mode = int(decoded.get("Vehicle_State_Mode", 0))
            if mode != 1:
                return False, f"CVC left RUN (state={STATE_NAMES.get(mode, mode)})"
    return True, f"CVC stayed in RUN for {duration}s"


def wait_for_all_heartbeats(bus, timeout=10.0):
    """Wait until all 4 physical ECU heartbeats are seen on the bus.

    Returns dict of {can_id: True/False} for each expected heartbeat.
    """
    expected = {
        CAN_CVC_HEARTBEAT: False,
        CAN_FZC_HEARTBEAT: False,
        CAN_RZC_HEARTBEAT: False,
        CAN_SC_STATUS: False,
    }
    end = time.time() + timeout
    while time.time() < end and not all(expected.values()):
        msg = bus.recv(timeout=0.5)
        if msg and msg.arbitration_id in expected:
            expected[msg.arbitration_id] = True
    return expected


# ---------------------------------------------------------------------------
# DTC sniffer
# ---------------------------------------------------------------------------

class DtcSniffer:
    """Background thread that captures a specific DTC broadcast."""

    def __init__(self, db, target_dtc):
        self.db = db
        self.target_dtc = target_dtc
        self.result_frame = None
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self._thread.start()

    def stop(self, timeout=3.0):
        self._stop.set()
        self._thread.join(timeout=timeout)
        return self.result_frame

    def _run(self):
        dtc_bus = can.interface.Bus(channel=CAN_CHANNEL, interface="socketcan")
        while not self._stop.is_set():
            msg = dtc_bus.recv(timeout=0.5)
            if msg and msg.arbitration_id == CAN_DTC:
                self.result_frame = msg
                decoded = self.db.decode_message(CAN_DTC, msg.data, decode_choices=False)
                dtc_num = int(decoded.get("DTC_Broadcast_Number", 0))
                if dtc_num == self.target_dtc:
                    break
        dtc_bus.shutdown()

    def get_decoded(self):
        if self.result_frame is None:
            return None
        return self.db.decode_message(CAN_DTC, self.result_frame.data, decode_choices=False)


# ---------------------------------------------------------------------------
# Check helper
# ---------------------------------------------------------------------------

class HopChecker:
    """Manages hop pass/fail tracking with stop-on-first-failure."""

    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.stop_hop = None

    def check(self, hop, desc, condition, detail=""):
        if self.stop_hop:
            return
        if condition:
            print(f"  [PASS] Hop {hop}: {desc}")
            self.passed += 1
        else:
            print(f"  [FAIL] Hop {hop}: {desc} — {detail}")
            self.failed += 1
            self.stop_hop = hop
            print(f"  STOP — fix Hop {hop} before testing downstream")

    @property
    def stopped(self):
        return self.stop_hop is not None

    def summary(self):
        print()
        print(f"=== {self.passed} passed, {self.failed} failed [HIL] ===")
        if self.stop_hop:
            print(f"Stopped at Hop {self.stop_hop}")
        return 0 if self.failed == 0 else 1
