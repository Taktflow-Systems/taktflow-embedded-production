#!/usr/bin/env python3
"""
Hop-by-hop test: Motor Overtemperature → MotorFaultStatus → SAFE_STOP

Tests each layer independently so the first failure pinpoints the bug.
Run on SIL host (laptop or VPS) with Docker SIL running.

Usage:
    python3 test/sil/test_overtemp_hops.py
"""

import json
import sys
import time

import can
import cantools
import paho.mqtt.publish as mqtt_pub

DBC_PATH = "gateway/taktflow_vehicle.dbc"
MQTT_HOST = "localhost"
MQTT_TOPIC = "taktflow/command/plant_inject"
FAULT_API = "http://localhost:8091"

CAN_MOTOR_STATUS = 0x300     # RZC → CAN: MotorFaultStatus at byte 7
CAN_MOTOR_TEMP   = 0x302     # RZC → CAN: WindingTemp1_C
CAN_VEHICLE_STATE = 0x100    # CVC → CAN: VehicleState
CAN_VSENSOR_RZC  = 0x601     # Plant-sim → CAN: virtual sensor (motor temp at bytes 2-3)
CAN_DTC          = 0x500     # Dem → CAN: DTC broadcast


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


def inject_temp(temp_c, sustained_sec=0):
    """Inject motor temperature via MQTT to plant-sim.

    If sustained_sec > 0, sends repeated injections at 100ms intervals
    to fight the motor model's thermal decay physics.
    """
    if sustained_sec > 0:
        end = time.time() + sustained_sec
        while time.time() < end:
            mqtt_pub.single(
                MQTT_TOPIC,
                json.dumps({"type": "inject_temp", "temp_c": temp_c}),
                hostname=MQTT_HOST, port=1883,
            )
            time.sleep(0.1)
    else:
        mqtt_pub.single(
            MQTT_TOPIC,
            json.dumps({"type": "inject_temp", "temp_c": temp_c}),
            hostname=MQTT_HOST, port=1883,
        )


def main():
    db = cantools.database.load_file(DBC_PATH)
    bus = can.interface.Bus(channel="vcan0", interface="socketcan")
    passed = 0
    failed = 0
    stop_hop = None

    def check(hop, desc, condition, detail=""):
        nonlocal passed, failed, stop_hop
        if stop_hop:
            return
        if condition:
            print(f"  [PASS] Hop {hop}: {desc}")
            passed += 1
        else:
            print(f"  [FAIL] Hop {hop}: {desc} — {detail}")
            failed += 1
            stop_hop = hop
            print(f"  STOP — fix Hop {hop} before testing downstream")

    print("=== Motor Overtemperature Hop Test ===")
    print()

    # Precondition: Wait for CVC to reach RUN state (Vehicle_State_Mode == 1)
    print("Precondition: Waiting for CVC RUN state (up to 30s)...")
    run_seen = False
    deadline = time.time() + 30
    while time.time() < deadline:
        msg = can_recv(bus, CAN_VEHICLE_STATE, timeout=2)
        if msg:
            decoded_vs = db.decode_message(CAN_VEHICLE_STATE, msg.data, decode_choices=False)
            mode = decoded_vs.get("Vehicle_State_Mode", 0)
            if mode == 1:  # RUN
                run_seen = True
                print(f"  [OK] CVC in RUN state")
                break
            else:
                state_names = {0: "INIT", 1: "RUN", 2: "DEGRADED", 3: "LIMP", 4: "SAFE_STOP", 5: "SHUTDOWN"}
                print(f"  ... CVC state={state_names.get(mode, mode)}, waiting...")
    if not run_seen:
        print("  [FAIL] CVC never reached RUN — cannot test fault injection")
        print("         Check: all ECUs running? SC relay asserted?")
        bus.shutdown()
        sys.exit(1)
    print()

    # Hop 1: Plant-sim sends motor temp on 0x601
    print("Hop 1: Plant-sim → CAN 0x601 (virtual sensor)")
    decoded = can_recv_decoded(db, bus, CAN_VSENSOR_RZC, timeout=3)
    if decoded:
        temp_dc = decoded.get("RZC_Virtual_Sensors_MotorTemp_dC", 0)
        check(1, f"0x601 present, temp={temp_dc/10:.0f}°C", True)
    else:
        check(1, "0x601 present", False, "No 0x601 on bus")

    # Hop 2: MQTT injection changes temp on 0x601
    print("Hop 2: MQTT inject 110°C → 0x601 (sustained 3s)")
    inject_temp(110.0, sustained_sec=3)
    decoded = can_recv_decoded(db, bus, CAN_VSENSOR_RZC, timeout=3)
    if decoded:
        temp_dc = decoded.get("RZC_Virtual_Sensors_MotorTemp_dC", 0)
        check(2, f"0x601 temp={temp_dc/10:.1f}°C (expect ~110)",
               temp_dc >= 1000, f"temp={temp_dc/10:.1f}°C, expected >=100°C")
    else:
        check(2, "0x601 after inject", False, "No 0x601")

    # Keep injecting in background for remaining hops
    import threading
    _inject_stop = threading.Event()
    def _sustain():
        while not _inject_stop.is_set():
            mqtt_pub.single(MQTT_TOPIC, json.dumps({"type": "inject_temp", "temp_c": 110.0}),
                            hostname=MQTT_HOST, port=1883)
            time.sleep(0.1)
    t = threading.Thread(target=_sustain, daemon=True)
    t.start()

    # Hop 3: RZC reads temp via IoHwAb → reports on 0x302
    print("Hop 3: RZC → CAN 0x302 (Motor_Temperature)")
    time.sleep(3)
    decoded = can_recv_decoded(db, bus, CAN_MOTOR_TEMP, timeout=3)
    if decoded:
        t1 = decoded.get("Motor_Temperature_WindingTemp1_C", 0)
        check(3, f"0x302 temp={t1:.0f}°C (expect >80)", t1 > 80,
               f"temp={t1}°C, expected >80")
    else:
        check(3, "0x302 present", False, "No 0x302 on bus")

    # Hop 4: TempMonitor detects overtemp → writes RZC_SIG_TEMP_FAULT=1 to RTE
    # Cannot test RTE directly. Proxy: check Motor_Status MotorFaultStatus
    print("Hop 4: TempMonitor → RTE(TEMP_FAULT) → Motor → 0x300 MotorFaultStatus")
    time.sleep(3)  # Give time for TempMonitor (100ms) + Motor (10ms)
    decoded = can_recv_decoded(db, bus, CAN_MOTOR_STATUS, timeout=3)
    if decoded:
        fault = decoded.get("Motor_Status_MotorFaultStatus", 0)
        check(4, f"MotorFaultStatus={fault} (expect 4=OVERTEMP)",
               fault == 4, f"MotorFaultStatus={fault}")
    else:
        check(4, "0x300 present", False, "No 0x300 on bus")

    # Hop 5: CVC reads MotorFaultStatus → EVT_MOTOR_CUTOFF → SAFE_STOP
    print("Hop 5: CVC → VehicleState=SAFE_STOP")
    time.sleep(3)
    decoded = can_recv_decoded(db, bus, CAN_VEHICLE_STATE, timeout=3)
    if decoded:
        vs = decoded.get("Vehicle_State_Mode", "?")
        check(5, f"VehicleState={vs} (expect SAFE_STOP)",
               vs in ("SAFE_STOP", 4), f"VehicleState={vs}")
    else:
        check(5, "0x100 present", False, "No 0x100 on bus")

    # Hop 6: DTC broadcast
    print("Hop 6: DTC 0xE302 (overtemp) on CAN 0x500")
    decoded = can_recv_decoded(db, bus, CAN_DTC, timeout=5)
    if decoded:
        dtc = int(decoded.get("DTC_Broadcast_Number", 0))
        check(6, f"DTC=0x{dtc:06X} (expect 0x00E302)",
               dtc == 0xE302, f"DTC=0x{dtc:06X}")
    else:
        check(6, "0x500 present", False, "No DTC on bus")

    # Cleanup: stop sustained injection, clear temp override
    _inject_stop.set()
    t.join(timeout=2)
    mqtt_pub.single(MQTT_TOPIC, json.dumps({"type": "clear_temp_override"}),
                    hostname=MQTT_HOST, port=1883)
    inject_temp(25.0)

    bus.shutdown()

    print()
    print(f"=== {passed} passed, {failed} failed ===")
    if stop_hop:
        print(f"Stopped at Hop {stop_hop} — fix this before testing downstream")
    sys.exit(1 if failed > 0 else 0)


if __name__ == "__main__":
    main()
