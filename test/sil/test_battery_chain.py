#!/usr/bin/env python3
"""Step-by-step battery voltage signal chain test.

Run from inside a container with vcan0 access (e.g. can-gateway):
  docker exec docker-can-gateway-1 python3 /app/test_battery_chain.py

Or from host with vcan0:
  python3 test/sil/test_battery_chain.py

Tests each hop independently. If a hop fails, all downstream hops
are skipped — fix the failing hop first.
"""

import os
import socket
import struct
import sys
import time

import can
import cantools

CAN_CHANNEL = "vcan0"
TIMEOUT = 3.0
MQTT_HOST = "localhost"
MQTT_PORT = 1883

_REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
_DB = cantools.database.load_file(os.path.join(_REPO, "gateway", "taktflow_vehicle.dbc"))


def _mqtt_inject(mv: int, soc: int):
    """Publish voltage injection via paho-mqtt."""
    import json
    import paho.mqtt.publish as publish
    publish.single(
        "taktflow/command/plant_inject",
        json.dumps({"type": "voltage", "mV": mv, "soc": soc}),
        hostname=MQTT_HOST, port=MQTT_PORT,
    )


def recv_frame(sock, target_id, timeout=TIMEOUT):
    """Wait for a specific CAN ID, return payload or None."""
    end = time.time() + timeout
    while time.time() < end:
        try:
            data = sock.recv(16)
            can_id = struct.unpack("<I", data[:4])[0] & 0x1FFFFFFF
            if can_id == target_id:
                return data[8:16]
        except socket.timeout:
            pass
    return None


def test_hop1_plant_sends_0x601():
    """Hop 1: Is plant-sim sending 0x601 on vcan0?"""
    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.bind((CAN_CHANNEL,))
    s.settimeout(TIMEOUT)
    payload = recv_frame(s, 0x601)
    s.close()
    if payload is None:
        return False, "0x601 not seen on vcan0"
    decoded = _DB.decode_message(0x601, payload, decode_choices=False)
    batt = int(decoded.get("RZC_Virtual_Sensors_BattVoltage_mV", 0))
    return True, f"0x601 present, batt={batt}mV"


def test_hop2_inject_and_check_0x601():
    """Hop 2: After MQTT injection, does 0x601 show the injected value?"""
    # Send MQTT via mosquitto_pub (assumes broker on localhost)
    _mqtt_inject(4000, 1)
    time.sleep(2)

    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.bind((CAN_CHANNEL,))
    s.settimeout(TIMEOUT)
    payload = recv_frame(s, 0x601)
    s.close()
    if payload is None:
        return False, "0x601 not seen after injection"
    decoded = _DB.decode_message(0x601, payload, decode_choices=False)
    batt = int(decoded.get("RZC_Virtual_Sensors_BattVoltage_mV", 0))
    if batt > 6000:
        return False, f"0x601 batt={batt}mV — injection didn't take effect (expected ~4000)"
    return True, f"0x601 batt={batt}mV (injected 4000)"


def test_hop5_com_rx_updates_0x303():
    """Hop 5: Does RZC's 0x303 (BatteryStatus) reflect the injected voltage?

    This tests the full chain: 0x601 → CanIf → PduR → Com → SensorFeeder → IoHwAb → Swc_Battery → 0x303
    If 0x303 still shows 12600mV after injection, the Com RX chain is broken.
    """
    # Inject low voltage
    _mqtt_inject(4000, 1)
    time.sleep(3)

    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.bind((CAN_CHANNEL,))
    s.settimeout(TIMEOUT)
    payload = recv_frame(s, 0x303)
    s.close()
    if payload is None:
        return False, "0x303 not seen"
    decoded = _DB.decode_message(0x303, payload, decode_choices=False)
    batt = int(decoded.get("Battery_Status_BatteryVoltage_mV", 0))
    if batt > 6000:
        return False, f"0x303 batt={batt}mV — RZC didn't track injection (Com RX chain broken)"
    return True, f"0x303 batt={batt}mV (matches injection)"


def test_hop9_dtc_on_undervoltage():
    """Hop 9-10: Does RZC broadcast DTC 0xE401 on undervoltage?

    Inject below 8000mV threshold and wait for DTC on 0x500.
    """
    _mqtt_inject(5000, 1)
    time.sleep(5)  # wait for debounce (3 cycles at 10ms = 30ms, but Dem needs confirmation)

    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.bind((CAN_CHANNEL,))
    s.settimeout(0.5)
    end = time.time() + 5
    while time.time() < end:
        try:
            data = s.recv(16)
            can_id = struct.unpack("<I", data[:4])[0] & 0x1FFFFFFF
            if can_id == 0x500:
                decoded = _DB.decode_message(can_id, data[8:16], decode_choices=False)
                dtc = int(decoded.get("DTC_Broadcast_Number", 0))
                source = int(decoded.get("DTC_Broadcast_ECU_Source", 0))
                s.close()
                if dtc == 0xE401:
                    return True, f"DTC 0x{dtc:06X} from source={source}"
                return False, f"DTC 0x{dtc:06X} (expected 0x00E401)"
        except socket.timeout:
            pass
    s.close()
    return False, "No DTC on 0x500 within 5s"


def main():
    tests = [
        ("Hop 1: Plant-sim sends 0x601", test_hop1_plant_sends_0x601),
        ("Hop 2: MQTT injection reaches 0x601", test_hop2_inject_and_check_0x601),
        ("Hop 5: Com RX chain (0x601 → 0x303)", test_hop5_com_rx_updates_0x303),
        ("Hop 9: DTC broadcast on undervoltage", test_hop9_dtc_on_undervoltage),
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

    # Restore normal voltage
    try:
        _mqtt_inject(12600, 100)
    except Exception:
        pass

    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
