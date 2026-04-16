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
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

import can
import cantools
import paho.mqtt.publish as mqtt_pub
from tools.bench.hardware_map import resolve_stlink_serial

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

DBC_PATH = os.environ.get("DBC_PATH", "gateway/taktflow_vehicle.dbc")
MQTT_HOST = os.environ.get("MQTT_HOST", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_TOPIC = "taktflow/command/plant_inject"
_MQTT_USER = os.environ.get("MQTT_USER", "taktflow")
_MQTT_PASS = os.environ.get("MQTT_PASSWORD", "taktflow-dev")
MQTT_AUTH = {"username": _MQTT_USER, "password": _MQTT_PASS} if _MQTT_USER else None

# HIL uses can0 (physical) by default; test runner observes on can0 which
# sees all traffic from both vcan0 (bridged) and physical ECUs
CAN_CHANNEL = os.environ.get("CAN_INTERFACE", "can0")

# CAN IDs (must match gateway/taktflow_vehicle.dbc)
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
    """Return the current git short hash (8 chars, matching Makefile GIT_HASH)."""
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short=8", "HEAD"],
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


def verify_firmware_binaries(ecus=("cvc", "fzc", "rzc"), platform="stm32"):
    """Verify that built binaries match current git HEAD before running tests.

    Extracts the GIT_HASH burned into each .elf file (via the boot banner
    string) and compares it against the current repo HEAD. Aborts if any
    binary is stale or missing.

    Call this BEFORE opening the CAN bus or starting any ECU process.
    """
    head_hash = get_git_hash()
    print(f"Pre-test: Binary version check (HEAD: {head_hash})")

    project_root = os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__)
    )))

    all_ok = True
    for ecu in ecus:
        if platform == "posix":
            elf = os.path.join(project_root, "build", "posix", f"{ecu}_posix")
        else:
            elf = os.path.join(project_root, "build", platform, f"{ecu}.elf")

        if not os.path.isfile(elf):
            print(f"  [FAIL] {ecu.upper()}: binary not found — {elf}")
            all_ok = False
            continue

        # Extract GIT_HASH from the boot banner string burned into the binary
        try:
            result = subprocess.run(
                ["strings", elf],
                capture_output=True, text=True, timeout=10,
            )
            binary_hash = None
            for line in result.stdout.splitlines():
                if "Boot" in line and "[" in line:
                    # Extract [abc12345] from "=== CVC Boot (PLL 170 MHz) [abc12345] ==="
                    start = line.find("[") + 1
                    end = line.find("]", start)
                    if end - start == 8:
                        binary_hash = line[start:end]
                        break

            if binary_hash is None:
                print(f"  [FAIL] {ecu.upper()}: no GIT_HASH in binary — rebuild")
                all_ok = False
            elif binary_hash != head_hash:
                print(f"  [FAIL] {ecu.upper()}: STALE — binary={binary_hash}, HEAD={head_hash}")
                all_ok = False
            else:
                print(f"  [OK]   {ecu.upper()}: {binary_hash}")

        except Exception as e:
            print(f"  [FAIL] {ecu.upper()}: cannot read binary — {e}")
            all_ok = False

    if not all_ok:
        print()
        print("  ABORTING: firmware binaries do not match current commit.")
        print("  Rebuild:  make -f firmware/platform/{}/Makefile.{} TARGET=<ecu>".format(
            platform, platform))
        sys.exit(1)

    print()


def precondition_all_ecus_healthy(bus, timeout=15.0):
    """Verify all 7 ECUs are healthy before running any test.

    Checks:
      1. Reset plant-sim faults via MQTT (clears stale latches)
      2. All 4 physical ECU heartbeats present (CVC, FZC, RZC, SC)
      3. At least 1 vECU heartbeat present (BCM/ICU/TCU)
      4. CVC in RUN state (byte 2 of 0x100 Vehicle_State)
      5. No stale MotorFaultStatus on 0x300

    Aborts test with sys.exit(1) if precondition fails.
    """
    print("Precondition: All ECUs must be healthy")

    # Step 1: Reset plant-sim faults to clear stale latches from prior tests
    try:
        mqtt_pub.single(
            MQTT_TOPIC, json.dumps({"type": "reset"}),
            hostname=MQTT_HOST, port=MQTT_PORT, auth=MQTT_AUTH,
        )
        print("  [OK] Plant-sim faults reset via MQTT")
    except Exception as e:
        print(f"  [WARN] MQTT fault reset failed: {e}")

    time.sleep(0.5)  # allow plant-sim to process reset

    physical = {
        CAN_CVC_HEARTBEAT: "CVC",
        CAN_FZC_HEARTBEAT: "FZC",
        CAN_RZC_HEARTBEAT: "RZC",
        CAN_SC_STATUS: "SC",
    }
    vecu = {
        0x014: "ICU",
        0x015: "TCU",
        CAN_BCM_BODY: "BCM",
    }
    seen_physical = set()
    seen_vecu = set()
    cvc_state = None
    motor_fault = None

    end = time.time() + timeout
    while time.time() < end:
        msg = bus.recv(timeout=0.5)
        if msg is None:
            continue
        aid = msg.arbitration_id
        if aid in physical:
            seen_physical.add(aid)
        if aid in vecu:
            seen_vecu.add(aid)
        if aid == CAN_VEHICLE_STATE and msg.dlc >= 3:
            cvc_state = msg.data[2] & 0x0F
        if aid == CAN_MOTOR_STATUS and msg.dlc >= 8:
            motor_fault = msg.data[7]  # MotorFaultStatus at bit 56 = byte 7

        if (len(seen_physical) == 4 and len(seen_vecu) >= 1
                and cvc_state is not None and motor_fault is not None):
            break

    # Report
    missing_phys = [physical[k] for k in physical if k not in seen_physical]
    phys_names = [physical[k] for k in seen_physical]
    vecu_names = [vecu[k] for k in seen_vecu]

    if missing_phys:
        print(f"  [FAIL] Missing physical ECUs: {', '.join(missing_phys)}")
        print(f"  Present: {', '.join(phys_names)}")
        sys.exit(1)

    if not seen_vecu:
        print(f"  [FAIL] No vECU heartbeats (BCM/ICU/TCU)")
        sys.exit(1)

    state_name = STATE_NAMES.get(cvc_state, f"0x{cvc_state:02X}") if cvc_state is not None else "?"
    if cvc_state != 1:
        print(f"  [WARN] CVC in {state_name} — attempting UDS ECUReset...")
        uds_ecu_reset_cvc(bus)
        time.sleep(3.0)  # allow CVC to re-init through INIT→RUN
        # Re-check CVC state
        recheck_end = time.time() + 15.0
        while time.time() < recheck_end:
            msg = bus.recv(timeout=0.5)
            if msg and msg.arbitration_id == CAN_VEHICLE_STATE and msg.dlc >= 3:
                cvc_state = msg.data[2] & 0x0F
                if cvc_state == 1:
                    break
        state_name = STATE_NAMES.get(cvc_state, f"0x{cvc_state:02X}") if cvc_state is not None else "?"
        if cvc_state != 1:
            print(f"  [FAIL] CVC still not in RUN after ECUReset (state={state_name})")
            sys.exit(1)
        print(f"  [OK] CVC recovered to RUN via UDS ECUReset")

    if motor_fault and motor_fault != 0:
        print(f"  [WARN] Stale MotorFaultStatus={motor_fault} — hardware reset RZC")
        # UDS ECUReset doesn't clear firmware fault latches (TM_TempFault).
        # Hardware reset via STM32 programmer is the only reliable clear.
        stcli = (
            "C:/Program Files (x86)/STMicroelectronics/STM32Cube"
            "/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe"
        )
        rzc_sn = resolve_stlink_serial("rzc", env=os.environ)
        reset_cmd = f'"{stcli}" -c port=SWD sn={rzc_sn} -rst'
        host = CVC_RESET_HOST  # same PC hosts all ST-LINK probes
        try:
            if host:
                full_cmd = f"ssh {host} '{reset_cmd}'"
            else:
                full_cmd = reset_cmd
            subprocess.run(full_cmd, shell=True, timeout=15,
                           capture_output=True)
            time.sleep(2.0)  # allow RZC to boot and clear latches
            print("  [OK] RZC hardware reset — fault latches cleared")
        except Exception as e:
            print(f"  [WARN] RZC hardware reset failed: {e}")

    print(f"  [OK] Physical: {', '.join(phys_names)}")
    print(f"  [OK] vECUs:    {', '.join(vecu_names)}")
    print(f"  [OK] CVC state={state_name}")
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
# When running on Pi, set CVC_RESET_HOST=operator@192.0.2.20 to SSH to the
# private Windows bench host.
# When running on Windows PC, leave CVC_RESET_HOST empty for local execution.
CVC_RESET_HOST = os.environ.get("CVC_RESET_HOST", "")
CVC_RESET_CMD = os.environ.get("CVC_RESET_CMD", "")

# Auto-detect: if running on Linux (Pi) and no host set, default to Windows PC
if not CVC_RESET_HOST and sys.platform == "linux":
    CVC_RESET_HOST = os.environ.get("PC_HOST", "operator@192.0.2.20")


def reset_cvc_hardware():
    """Reset CVC MCU via CubeProgrammer on Windows PC.

    When running on Pi: SSHes to Windows PC to invoke CubeProgrammer.
    When running on Windows PC: runs CubeProgrammer locally.
    Falls back to no-op with warning if connection fails.
    """
    cmd = CVC_RESET_CMD
    if not cmd:
        stcli = os.environ.get("STCLI",
            "C:/Program Files (x86)/STMicroelectronics/STM32Cube/"
            "STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe")
        sn = resolve_stlink_serial("cvc", env=os.environ)
        cmd = f'"{stcli}" -c port=SWD sn={sn} mode=UR --start'

    host = CVC_RESET_HOST
    if host:
        full_cmd = f"ssh {host} '{cmd}'"
    else:
        full_cmd = cmd

    print(f"  [RESET] CVC hardware reset{' via ' + host if host else ''}...")
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
                    hostname=MQTT_HOST, port=MQTT_PORT, auth=MQTT_AUTH)


def uds_ecu_reset_cvc(bus_or_channel=None):
    """Send UDS ECUReset (SID 0x11) to CVC to clear VSM latched faults.

    CVC physical request: 0x7E0, response: 0x7E8.
    Clears vehicle state machine latches and DTCs via
    Swc_VehicleState_Init() + Dem_ClearAllDTCs().
    """
    own_bus = False
    if bus_or_channel is None:
        bus_or_channel = can.interface.Bus(channel=CAN_CHANNEL, interface="socketcan")
        own_bus = True
    elif isinstance(bus_or_channel, str):
        bus_or_channel = can.interface.Bus(channel=bus_or_channel, interface="socketcan")
        own_bus = True

    req = can.Message(arbitration_id=0x7E0, data=[0x02, 0x11, 0x01, 0, 0, 0, 0, 0],
                      is_extended_id=False)
    bus_or_channel.send(req)

    end = time.time() + 2.0
    got_response = False
    while time.time() < end:
        msg = bus_or_channel.recv(timeout=0.5)
        if msg and msg.arbitration_id == 0x7E8:
            if len(msg.data) >= 2 and msg.data[1] == 0x51:
                got_response = True
                break

    if own_bus:
        bus_or_channel.shutdown()
    return got_response


def uds_ecu_reset_rzc(bus_or_channel=None):
    """Send UDS ECUReset (SID 0x11) to RZC to clear firmware fault latches.

    RZC physical request: 0x7E2, response: 0x7EA.
    Clears Motor_FaultLatched, TM_TempFault, and all SWC state via
    Swc_Motor_Init() + Swc_TempMonitor_Init() on the RZC firmware side.
    """
    own_bus = False
    if bus_or_channel is None:
        bus_or_channel = can.interface.Bus(channel=CAN_CHANNEL, interface="socketcan")
        own_bus = True
    elif isinstance(bus_or_channel, str):
        bus_or_channel = can.interface.Bus(channel=bus_or_channel, interface="socketcan")
        own_bus = True

    # UDS ECUReset: SID=0x11, sub=0x01 (hardReset)
    req = can.Message(arbitration_id=0x7E2, data=[0x02, 0x11, 0x01, 0, 0, 0, 0, 0],
                      is_extended_id=False)
    bus_or_channel.send(req)

    # Wait for positive response (0x51 0x01) on 0x7EA
    end = time.time() + 2.0
    got_response = False
    while time.time() < end:
        msg = bus_or_channel.recv(timeout=0.5)
        if msg and msg.arbitration_id == 0x7EA:
            if len(msg.data) >= 2 and msg.data[1] == 0x51:
                got_response = True
                break

    if own_bus:
        bus_or_channel.shutdown()
    return got_response


def mqtt_reset():
    """Reset all faults to nominal (plant-sim + RZC firmware latches)."""
    mqtt_inject("reset")
    mqtt_inject("voltage", mV=12600, soc=100)
    mqtt_inject("clear_temp_override")
    mqtt_inject("inject_temp", temp_c=25.0)
    # Clear RZC firmware fault latches via UDS ECUReset
    uds_ecu_reset_rzc()


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
