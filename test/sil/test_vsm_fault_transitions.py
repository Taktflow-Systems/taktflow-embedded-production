#!/usr/bin/env python3
"""Step-by-step VSM fault transition test.

Tests whether CVC transitions to SAFE_STOP when fault signals are
received from FZC/RZC. Each hop isolates one part of the chain.

Run from VPS host:
  python3 test/sil/test_vsm_fault_transitions.py
"""

import json
import socket
import struct
import sys
import time

import paho.mqtt.publish as publish

CAN_CHANNEL = "vcan0"
MQTT_HOST = "localhost"
MQTT_PORT = 1883
TIMEOUT = 3.0


def _mqtt_inject(msg_type, **kwargs):
    payload = {"type": msg_type}
    payload.update(kwargs)
    publish.single(
        "taktflow/command/plant_inject",
        json.dumps(payload),
        hostname=MQTT_HOST, port=MQTT_PORT,
    )


def _read_vehicle_state(timeout=TIMEOUT):
    """Read VehicleState from CAN 0x100. Returns raw state byte."""
    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.bind((CAN_CHANNEL,))
    s.settimeout(timeout)
    end = time.time() + timeout
    while time.time() < end:
        try:
            data = s.recv(16)
            can_id = struct.unpack("<I", data[:4])[0] & 0x1FFFFFFF
            if can_id == 0x100:
                s.close()
                # VehicleState at byte 2 (after E2E bytes 0-1)
                return data[10]  # byte 2 of payload
        except socket.timeout:
            pass
    s.close()
    return None


def _state_name(val):
    names = {0: "INIT", 1: "RUN", 2: "DEGRADED", 3: "LIMP", 4: "SAFE_STOP", 5: "SHUTDOWN"}
    return names.get(val, f"UNKNOWN({val})")


def _check_dtc(expected_dtc, timeout=5.0):
    """Check if a specific DTC appears on 0x500."""
    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.bind((CAN_CHANNEL,))
    s.settimeout(0.5)
    end = time.time() + timeout
    while time.time() < end:
        try:
            data = s.recv(16)
            can_id = struct.unpack("<I", data[:4])[0] & 0x1FFFFFFF
            if can_id == 0x500:
                dtc = (data[8] << 16) | (data[9] << 8) | data[10]
                if dtc == expected_dtc:
                    s.close()
                    return True
        except socket.timeout:
            pass
    s.close()
    return False


def test_hop1_cvc_in_run():
    """Hop 1: Is CVC in RUN state?"""
    state = _read_vehicle_state()
    if state is None:
        return False, "No 0x100 on bus"
    name = _state_name(state)
    if state != 1:
        return False, f"CVC state={name} (expected RUN)"
    return True, f"CVC state={name}"


def test_hop2_motor_cutoff_signal():
    """Hop 2: Does overcurrent injection produce Motor_Cutoff_Req on CAN?

    RZC detects overcurrent → sends Motor_Cutoff_Req (0x211) to CVC.
    """
    _mqtt_inject("overcurrent")
    time.sleep(5)
    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.bind((CAN_CHANNEL,))
    s.settimeout(3.0)
    for _ in range(2000):
        data = s.recv(16)
        can_id = struct.unpack("<I", data[:4])[0] & 0x1FFFFFFF
        if can_id == 0x211:
            s.close()
            return True, f"Motor_Cutoff_Req (0x211) present"
    s.close()
    return False, "No Motor_Cutoff_Req (0x211) on bus"


def test_hop3_cvc_reads_motor_cutoff():
    """Hop 3: Does CVC read the motor_cutoff signal from RTE?

    CVC reads CVC_SIG_MOTOR_CUTOFF from Com RX → RTE.
    If motor_cutoff=1, VSM should trigger EVT_MOTOR_CUTOFF.
    Check: does vehicle state change from RUN?
    """
    time.sleep(3)
    state = _read_vehicle_state()
    if state is None:
        return False, "No 0x100 on bus"
    name = _state_name(state)
    if state == 1:
        return False, f"CVC still in RUN — motor_cutoff not processed by VSM"
    if state == 4:
        return True, f"CVC state=SAFE_STOP (correct per HARA)"
    return False, f"CVC state={name} (expected SAFE_STOP, got {name})"


def test_hop4_brake_fault_signal():
    """Hop 4: Does brake fault injection produce a state change?

    FZC detects brake deviation → sends Brake_Fault (0x210).
    CVC reads brake_fault → EVT_BRAKE_FAULT → SAFE_STOP.
    """
    # Reset first
    import subprocess
    try:
        subprocess.run(
            ["curl", "-s", "-X", "POST", "http://localhost:8091/api/fault/scenario/reset"],
            timeout=30, capture_output=True,
        )
    except Exception:
        pass
    time.sleep(15)

    # Verify RUN
    state = _read_vehicle_state()
    if state != 1:
        return False, f"Pre-condition: CVC not in RUN (state={_state_name(state)})"

    # Inject brake fault
    _mqtt_inject("brake_fault")
    time.sleep(8)

    state = _read_vehicle_state()
    if state is None:
        return False, "No 0x100 on bus"
    name = _state_name(state)
    if state == 1:
        return False, f"CVC still in RUN after brake fault"
    return True, f"CVC state={name} after brake fault"


def test_hop5_battery_uv_state():
    """Hop 5: Does battery undervoltage change vehicle state?"""
    import subprocess
    try:
        subprocess.run(
            ["curl", "-s", "-X", "POST", "http://localhost:8091/api/fault/scenario/reset"],
            timeout=30, capture_output=True,
        )
    except Exception:
        pass
    time.sleep(15)

    state = _read_vehicle_state()
    if state != 1:
        return False, f"Pre-condition: CVC not in RUN (state={_state_name(state)})"

    # Hold low voltage
    for _ in range(20):
        _mqtt_inject("voltage", mV=5000, soc=1)
        time.sleep(0.5)

    state = _read_vehicle_state()
    name = _state_name(state)
    if state == 1:
        return False, f"CVC still in RUN after 10s battery UV"
    return True, f"CVC state={name} after battery UV"


def main():
    tests = [
        ("Hop 1: CVC in RUN", test_hop1_cvc_in_run),
        ("Hop 2: Motor_Cutoff_Req on CAN", test_hop2_motor_cutoff_signal),
        ("Hop 3: CVC state change on motor cutoff", test_hop3_cvc_reads_motor_cutoff),
        ("Hop 4: CVC state change on brake fault", test_hop4_brake_fault_signal),
        ("Hop 5: CVC state change on battery UV", test_hop5_battery_uv_state),
    ]

    all_pass = True
    for name, test_fn in tests:
        try:
            passed, detail = test_fn()
        except Exception as e:
            passed, detail = False, f"Exception: {e}"

        status = "PASS" if passed else "FAIL"
        print(f"  [{status}] {name}: {detail}")

        if not passed:
            all_pass = False
            print(f"  STOP — fix this hop before testing downstream")
            break

    # Restore
    try:
        _mqtt_inject("voltage", mV=12600, soc=100)
    except Exception:
        pass

    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
