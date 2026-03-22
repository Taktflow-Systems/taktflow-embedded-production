#!/usr/bin/env python3
"""Shared SIL test library — common patterns for hop-by-hop tests.

Consolidates lessons learned from overtemp, VSM, and battery test development.
Eliminates code duplication and enforces consistent test patterns.

Patterns enforced:
- CVC RUN precondition with timeout
- Polling-based waits (no arbitrary time.sleep)
- FTTI timing measurement
- DTC sniffer for one-shot broadcasts
- Fault cleanup / state restoration
- Git hash + timestamp in output
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

DBC_PATH = "gateway/taktflow_vehicle.dbc"
MQTT_HOST = os.environ.get("MQTT_HOST", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_TOPIC = "taktflow/command/plant_inject"
CAN_CHANNEL = os.environ.get("CAN_INTERFACE", "vcan0")

CAN_VEHICLE_STATE = 0x100
CAN_MOTOR_STATUS  = 0x300
CAN_MOTOR_CURRENT = 0x301
CAN_MOTOR_TEMP    = 0x302
CAN_BATTERY       = 0x303
CAN_DTC           = 0x500
CAN_VSENSOR       = 0x601

STATE_NAMES = {
    0: "INIT", 1: "RUN", 2: "DEGRADED", 3: "LIMP",
    4: "SAFE_STOP", 5: "SHUTDOWN",
}

ECU_NAMES = {
    1: "CVC", 2: "FZC", 3: "RZC", 4: "SC",
    5: "BCM", 6: "ICU", 7: "TCU",
}


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
    print(f"=== {test_name} ===")
    print(f"    Date: {now}  DUT: {git_hash}")
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


# ---------------------------------------------------------------------------
# MQTT injection
# ---------------------------------------------------------------------------

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
# Preconditions
# ---------------------------------------------------------------------------

def wait_cvc_run(db, bus, timeout=30.0, stable_sec=12.0):
    """Reset faults, wait for CVC RUN, verify stable past grace period.

    CVC_POST_INIT_GRACE_CYCLES = 1000 (10s on POSIX). Fault confirmations
    are suppressed during grace. stable_sec must exceed grace to catch
    stale faults that trigger SAFE_STOP after grace expiry.
    """
    print("Precondition: Reset + waiting for stable CVC RUN state...")
    mqtt_reset()
    time.sleep(1)

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

    # Phase 2: verify stays in RUN for stable_sec
    end = time.time() + stable_sec
    while time.time() < end:
        decoded = can_recv_decoded(db, bus, CAN_VEHICLE_STATE, timeout=2)
        if decoded:
            mode = int(decoded.get("Vehicle_State_Mode", 0))
            if mode != 1:
                # Fell out of RUN — retry from phase 1
                print(f"  ... CVC dropped to {STATE_NAMES.get(mode, mode)}, retrying...")
                val, elapsed = poll_signal(
                    db, bus, CAN_VEHICLE_STATE, "Vehicle_State_Mode",
                    lambda v: int(v) == 1, timeout=timeout,
                )
                if elapsed is None:
                    print(f"  [FAIL] CVC did not re-enter RUN")
                    return False
                end = time.time() + stable_sec  # restart stability timer
    print(f"  [OK] CVC stable in RUN for {stable_sec}s")
    return True


def reset_and_wait_run(db, bus, timeout=30.0, restart_containers=True):
    """Reset all faults and wait for CVC to return to RUN.

    If restart_containers=True, restarts CVC+RZC+SC+plant-sim Docker
    containers to clear firmware-latched faults (overcurrent, overtemp).
    """
    mqtt_reset()
    if restart_containers:
        try:
            subprocess.run(
                ["sudo", "docker", "compose", "-f",
                 "docker/docker-compose.dev.yml",
                 "restart", "cvc", "rzc", "sc", "plant-sim"],
                timeout=30, capture_output=True, cwd=os.getcwd(),
            )
        except Exception:
            pass
        time.sleep(5)
    else:
        time.sleep(2)
    val, elapsed = poll_signal(
        db, bus, CAN_VEHICLE_STATE, "Vehicle_State_Mode",
        lambda v: int(v) == 1, timeout=timeout,
    )
    if elapsed is None:
        return False
    # Verify stability past grace period
    end = time.time() + 12.0
    while time.time() < end:
        decoded = can_recv_decoded(db, bus, CAN_VEHICLE_STATE, timeout=2)
        if decoded and int(decoded.get("Vehicle_State_Mode", 0)) != 1:
            # Dropped out — wait for re-entry
            val2, elapsed2 = poll_signal(
                db, bus, CAN_VEHICLE_STATE, "Vehicle_State_Mode",
                lambda v: int(v) == 1, timeout=timeout,
            )
            if elapsed2 is None:
                return False
            end = time.time() + 12.0
    return True


def verify_normal_operation(db, bus, duration=5.0):
    """Hop 0: Verify CVC stays in RUN for `duration` seconds (negative test)."""
    end = time.time() + duration
    while time.time() < end:
        decoded = can_recv_decoded(db, bus, CAN_VEHICLE_STATE, timeout=2)
        if decoded:
            mode = int(decoded.get("Vehicle_State_Mode", 0))
            if mode != 1:
                return False, f"CVC left RUN (state={STATE_NAMES.get(mode, mode)}) during normal operation"
    return True, f"CVC stayed in RUN for {duration}s"


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
        print(f"=== {self.passed} passed, {self.failed} failed ===")
        if self.stop_hop:
            print(f"Stopped at Hop {self.stop_hop}")
        return 0 if self.failed == 0 else 1
