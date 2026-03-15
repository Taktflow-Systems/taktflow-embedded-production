#!/usr/bin/env python3
"""
Godot ↔ vECU CAN Bridge — GIL (Godot-in-the-Loop)

Replaces plant_sim in the GIL stack: reads actuator CAN commands from vECUs,
sends them to Godot via UDP. Receives sensor data from Godot,
encodes as CAN messages and sends to vECUs.

System: GIL (Godot-in-the-Loop) on Raspberry Pi
Counterpart: plant-sim (SIL on VPS — autonomous physics, no Godot)

Supports multiple cars, each on its own CAN interface.
In GIL mode (Pi), uses CAN_INTERFACE env var (default: can0).
In multi-car mode, constructs vcan{N} from --vcan-start.

Usage:
    python bridge.py                          # 1 car on $CAN_INTERFACE (or can0)
    python bridge.py --cars 3                 # 3 cars on vcan0,vcan1,vcan2
    python bridge.py --cars 1 --no-can        # standalone mode (no CAN, just UDP echo)
"""

import argparse
import json
import os
import socket
import struct
import subprocess
import threading
import time
import sys

try:
    import paho.mqtt.client as mqtt
    HAS_MQTT = True
except ImportError:
    HAS_MQTT = False

try:
    import can
    HAS_CAN = True
except ImportError:
    HAS_CAN = False
    print("[godot-bridge] python-can not installed — running in standalone mode")


# ── CAN Message IDs ──────────────────────────────────────────

# Actuator commands FROM ECUs (bridge reads these)
CAN_ID_ESTOP           = 0x001
CAN_ID_SC_STATUS       = 0x013
CAN_ID_VEHICLE_STATE   = 0x100
CAN_ID_TORQUE_REQ      = 0x101
CAN_ID_STEER_CMD       = 0x102
CAN_ID_BRAKE_CMD       = 0x103

# Heartbeats FROM ECUs
CAN_ID_CVC_HB          = 0x010
CAN_ID_FZC_HB          = 0x011
CAN_ID_RZC_HB          = 0x012

# Sensor feedback TO ECUs (bridge writes these)
CAN_ID_STEERING_STATUS = 0x200
CAN_ID_BRAKE_STATUS    = 0x201
CAN_ID_LIDAR_DISTANCE  = 0x220
CAN_ID_MOTOR_STATUS    = 0x300
CAN_ID_MOTOR_CURRENT   = 0x301
CAN_ID_MOTOR_TEMP      = 0x302
CAN_ID_BATTERY_STATUS  = 0x303
CAN_ID_FZC_VSENSORS    = 0x600
CAN_ID_RZC_VSENSORS    = 0x601

# DataIDs for E2E protection (lower nibble of byte 0)
DATA_IDS = {
    CAN_ID_STEERING_STATUS: 0x02,
    CAN_ID_BRAKE_STATUS:    0x03,
    CAN_ID_LIDAR_DISTANCE:  0x04,
    CAN_ID_MOTOR_STATUS:    0x05,
    CAN_ID_MOTOR_CURRENT:   0x06,
    CAN_ID_MOTOR_TEMP:      0x07,
    CAN_ID_BATTERY_STATUS:  0x08,
    CAN_ID_FZC_HB:          0x03,  # FZC heartbeat E2E DataID
    CAN_ID_RZC_HB:          0x04,  # RZC heartbeat E2E DataID
}

# Vehicle states
VS_INIT      = 0
VS_RUN       = 1
VS_DEGRADED  = 2
VS_LIMP      = 3
VS_SAFE_STOP = 4

VS_NAMES = {VS_INIT: "INIT", VS_RUN: "RUN", VS_DEGRADED: "DEGRADED",
             VS_LIMP: "LIMP", VS_SAFE_STOP: "SAFE_STOP"}


# ── E2E Protection ───────────────────────────────────────────

def crc8_sae_j1850(data: bytes) -> int:
    """CRC-8 SAE J1850 — polynomial 0x1D, init 0xFF, xor-out 0xFF."""
    crc = 0xFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x1D) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc ^ 0xFF


def e2e_pack(can_id: int, payload: bytes, alive_counter: int) -> bytes:
    """Pack payload with E2E header (byte 0 = DataID|AliveCounter, byte 1 = CRC8)."""
    data_id = DATA_IDS.get(can_id, 0x00)
    byte0 = ((alive_counter & 0x0F) << 4) | (data_id & 0x0F)
    # CRC covers: DataID byte + payload
    crc_input = bytes([byte0]) + payload
    crc = crc8_sae_j1850(crc_input)
    return bytes([byte0, crc]) + payload


# ── Car Bridge Instance ──────────────────────────────────────

class CarBridge:
    """Bridge for one car: one vcan interface ↔ one Godot UDP port pair."""

    def __init__(self, car_index: int, can_interface: str,
                 godot_sensor_port: int, godot_actuator_port: int,
                 spi_pedal_port: int, use_can: bool = True,
                 godot_host: str = "192.168.0.105"):
        self.car_index = car_index
        self.can_interface = can_interface
        self.use_can = use_can and HAS_CAN

        # UDP: receive sensor data FROM Godot
        self.sensor_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sensor_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sensor_sock.bind(("0.0.0.0", godot_sensor_port))
        self.sensor_sock.setblocking(False)

        # UDP: send actuator commands TO Godot
        self.actuator_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.godot_actuator_addr = (godot_host, godot_actuator_port)

        # UDP: send pedal override to CVC SPI
        self.spi_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.spi_pedal_addr = ("127.0.0.1", spi_pedal_port)

        # CAN bus
        self.can_bus = None
        if self.use_can:
            try:
                self.can_bus = can.Bus(channel=can_interface, interface="socketcan")
                print(f"[Car {car_index}] CAN connected: {can_interface}")
            except Exception as e:
                print(f"[Car {car_index}] CAN failed ({e}) — running without CAN")
                self.use_can = False

        # Alive counters for E2E
        self.alive_counters = {}

        # Latest actuator state from ECUs
        self.actuator_state = {
            "car_index": car_index,
            "motor_torque_pct": 0.0,
            "brake_force_pct": 0.0,
            "steer_cmd_deg": 0.0,
            "kill_relay": False,
            "vehicle_state": "INIT",
            "estop": False,
            "ecu_heartbeats": {"CVC": 0, "FZC": 0, "RZC": 0, "SC": 0},
        }

        # Latest sensor data from Godot
        self.sensor_data = {}

        # ECU heartbeat tracking
        self.last_heartbeat = {"CVC": 0, "FZC": 0, "RZC": 0, "SC": 0}

        # DTC tracking (root cause for SAFE_STOP)
        self.active_dtcs = []

        # Vehicle state (for startup sequence)
        self.vehicle_state = VS_INIT
        self.start_time = time.time()

        # TX scheduling
        self.tx_counters = {}

    def get_alive(self, can_id: int) -> int:
        """Get and increment alive counter for a CAN ID."""
        count = self.alive_counters.get(can_id, 0)
        self.alive_counters[can_id] = (count + 1) & 0x0F
        return count

    # ── CAN RX (actuator commands from ECUs) ─────────────────

    def poll_can(self) -> None:
        """Non-blocking read of all available CAN messages."""
        if not self.can_bus:
            return

        while True:
            msg = self.can_bus.recv(timeout=0)
            if msg is None:
                break
            self._handle_can_rx(msg)

    def _handle_can_rx(self, msg: can.Message) -> None:
        aid = msg.arbitration_id
        d = msg.data
        now = time.time()

        if aid == CAN_ID_ESTOP and len(d) >= 4:
            self.actuator_state["estop"] = bool(d[2])

        elif aid == CAN_ID_SC_STATUS and len(d) >= 4:
            relay_killed = bool(d[2] & 0x01)
            self.actuator_state["kill_relay"] = relay_killed

        elif aid == CAN_ID_VEHICLE_STATE and len(d) >= 4:
            vs = d[2] & 0x0F
            self.vehicle_state = vs
            self.actuator_state["vehicle_state"] = VS_NAMES.get(vs, "UNKNOWN")

        elif aid == CAN_ID_TORQUE_REQ and len(d) >= 8:
            torque_raw = d[2]  # 0-100%
            direction = d[3] & 0x03  # 0=stop, 1=fwd, 2=rev
            torque_pct = torque_raw / 100.0
            if direction == 2:
                torque_pct = -torque_pct
            elif direction == 0:
                torque_pct = 0.0
            self.actuator_state["motor_torque_pct"] = torque_pct

        elif aid == CAN_ID_STEER_CMD and len(d) >= 6:
            raw = struct.unpack_from("<h", d, 2)[0]
            angle_deg = raw * 0.01 - 45.0
            self.actuator_state["steer_cmd_deg"] = angle_deg

        elif aid == CAN_ID_BRAKE_CMD and len(d) >= 4:
            brake_pct = d[2] / 100.0
            self.actuator_state["brake_force_pct"] = brake_pct

        elif aid == CAN_ID_CVC_HB:
            self.last_heartbeat["CVC"] = now
        elif aid == CAN_ID_FZC_HB:
            self.last_heartbeat["FZC"] = now
        elif aid == CAN_ID_RZC_HB:
            self.last_heartbeat["RZC"] = now
        elif aid == CAN_ID_SC_STATUS:
            self.last_heartbeat["SC"] = now

        # DTC broadcast (0x500)
        elif aid == 0x500 and len(d) >= 4:
            dtc_code = struct.unpack_from("<H", d, 2)[0]
            dtc_hex = f"0x{dtc_code:04X}"
            if dtc_hex not in self.active_dtcs:
                self.active_dtcs.append(dtc_hex)
                print(f"[Car {self.car_index}] DTC: {dtc_hex}")

    # ── CAN TX (sensor feedback to ECUs) ─────────────────────

    def send_heartbeats(self) -> None:
        """Send fake heartbeat frames so CVC sees FZC/RZC/SC as alive.
        Workaround for firmware regression where ECUs don't TX heartbeats.
        DLC=4 to match CanIf_Cfg (generated config uses 4-byte heartbeats)."""
        if not self.can_bus:
            return

        # Heartbeat format (4 bytes): E2E byte0 + CRC byte1 + ECU_ID + state/fault
        # CanIf config: DLC=4 for heartbeat frames
        vs = self.vehicle_state & 0x0F
        for can_id, ecu_id in [
            (CAN_ID_FZC_HB, 0x02),
            (CAN_ID_RZC_HB, 0x03),
        ]:
            payload = bytes([ecu_id, vs])  # 2 bytes: ECU_ID + state
            data = e2e_pack(can_id, payload, self.get_alive(can_id))
            self._can_send(can_id, data[:4])

    def send_sensor_can(self) -> None:
        """Send virtual sensor data to ECUs (replaces plant_sim).
        Only sends 0x600 (FZC sensors) and 0x601 (RZC sensors).
        ECUs generate their own status messages (0x200, 0x300, etc.)."""
        if not self.can_bus:
            return

        sd = self.sensor_data if self.sensor_data else {
            "motor_rpm": 0, "motor_current_a": 0, "motor_temp_c": 25,
            "battery_voltage_v": 12.6, "vehicle_speed_kmh": 0,
            "steer_angle_deg": 0, "lidar_distance_m": 12.0,
        }

        # FZC Virtual Sensors (0x600) — steering angle + brake position
        steer_deg = sd.get("steer_angle_deg", 0.0)
        steer_spi = int((steer_deg + 45.0) / 90.0 * 16383)
        steer_spi = max(0, min(16383, steer_spi))
        brake_adc = int(self.actuator_state["brake_force_pct"] * 1000)
        fzc_data = struct.pack("<HHHxx", steer_spi, min(brake_adc, 1000), 0)
        self._can_send(CAN_ID_FZC_VSENSORS, fzc_data)

        # RZC Virtual Sensors (0x601) — motor current, temp, battery, rpm
        motor_current_ma = int(sd.get("motor_current_a", 0) * 1000)
        motor_temp_raw = int(sd.get("motor_temp_c", 25) * 10)
        battery_mv = int(sd.get("battery_voltage_v", 12.6) * 1000)
        motor_rpm = int(sd.get("motor_rpm", 0))
        rzc_data = struct.pack("<HHHH",
                               min(motor_current_ma, 30000),
                               min(motor_temp_raw, 2000),
                               min(battery_mv, 20000),
                               min(motor_rpm, 10000))
        self._can_send(CAN_ID_RZC_VSENSORS, rzc_data)

    def _can_send(self, can_id: int, data: bytes) -> None:
        try:
            msg = can.Message(arbitration_id=can_id, data=data, is_extended_id=False)
            self.can_bus.send(msg)
        except Exception:
            pass

    # ── UDP ───────────────────────────────────────────────────

    def poll_godot_sensors(self) -> None:
        """Non-blocking read of sensor data from Godot."""
        while True:
            try:
                data, _ = self.sensor_sock.recvfrom(4096)
                parsed = json.loads(data.decode("utf-8"))
                self.sensor_data = parsed
            except BlockingIOError:
                break
            except (json.JSONDecodeError, UnicodeDecodeError):
                continue

    def send_actuator_to_godot(self) -> None:
        """Send current actuator state to Godot."""
        # Update heartbeat status and build root cause
        now = time.time()
        root_causes = list(self.active_dtcs)  # copy DTCs

        for ecu in ["CVC", "FZC", "RZC", "SC"]:
            age = now - self.last_heartbeat.get(ecu, 0)
            alive = age < 1.0 if self.last_heartbeat[ecu] > 0 else False
            self.actuator_state["ecu_heartbeats"][ecu] = 1 if alive else 0
            if not alive and self.last_heartbeat[ecu] > 0:
                root_causes.append(f"{ecu}_HEARTBEAT_LOST")

        self.actuator_state["active_dtcs"] = self.active_dtcs
        self.actuator_state["root_cause"] = root_causes

        payload = json.dumps(self.actuator_state).encode("utf-8")
        try:
            self.actuator_sock.sendto(payload, self.godot_actuator_addr)
        except Exception:
            pass

    def send_pedal_spi(self) -> None:
        """Forward pedal from Godot to CVC via SPI UDP.
        Uses the exact same mapping as fault_inject/pedal_udp.py:
          position = pct * 1000 / 100
          angle = (position * 16383) // 1000
        Dead zone: position < 67 (angle < 1097) = torque 0.
        When idle, sends 0xFFFF to clear override (CVC returns to default oscillation)."""
        if not self.sensor_data:
            return

        pedal_pct = self.sensor_data.get("pedal_pct", 0.0)

        if pedal_pct < 1.0:
            # Clear override — CVC returns to default idle oscillation (dead zone)
            angle = 0xFFFF
        else:
            # Same mapping as fault_inject/pedal_udp.py
            position = pedal_pct * 1000.0 / 100.0
            angle = int((position * 16383) // 1000)
            angle = max(0, min(16383, angle))

        try:
            self.spi_sock.sendto(struct.pack("<H", angle), self.spi_pedal_addr)
        except Exception:
            pass

    # ── Lifecycle ─────────────────────────────────────────────

    def tick(self) -> None:
        """One bridge cycle: poll inputs, send outputs."""
        self.poll_can()
        self.poll_godot_sensors()
        self.send_heartbeats()
        self.send_sensor_can()
        self.send_actuator_to_godot()
        self.send_pedal_spi()

    def close(self) -> None:
        self.sensor_sock.close()
        self.actuator_sock.close()
        self.spi_sock.close()
        if self.can_bus:
            self.can_bus.shutdown()


# ── Standalone Echo Mode ─────────────────────────────────────

class StandaloneEcho:
    """Minimal vECU: reads pedal/steering from Godot, maps to actuator commands.
    No CAN, no firmware — just direct pedal→torque, steer→angle passthrough.
    This lets the car actually drive in Godot without the full ECU stack."""

    def __init__(self, car_index: int, sensor_port: int, actuator_port: int,
                 godot_host: str = "192.168.0.105"):
        self.car_index = car_index
        self.sensor_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sensor_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sensor_sock.bind(("0.0.0.0", sensor_port))
        self.sensor_sock.setblocking(False)
        self.actuator_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.actuator_addr = (godot_host, actuator_port)
        self.start_time = time.time()
        self.sensor_data = {}

    def tick(self) -> None:
        while True:
            try:
                data, _ = self.sensor_sock.recvfrom(4096)
                self.sensor_data = json.loads(data.decode("utf-8"))
            except BlockingIOError:
                break
            except (json.JSONDecodeError, UnicodeDecodeError):
                continue

        sd = self.sensor_data
        state = "RUN" if time.time() - self.start_time > 2.0 else "INIT"

        # Direct passthrough: pedal → torque, steer → angle, brake → brake/reverse
        pedal_pct = sd.get("pedal_pct", 0.0)
        steer_deg = sd.get("steer_input_deg", 0.0)
        brake_pct = sd.get("brake_input_pct", 0.0)
        estop = sd.get("estop_input", False)
        speed = sd.get("vehicle_speed_kmh", 0.0)

        if estop:
            torque = 0.0
            brake = 1.0
        elif brake_pct > 1.0 and speed < 3.0:
            # Reverse: brake key at low speed → negative torque (like real car)
            torque = -(brake_pct / 100.0) * 0.32  # 32% of max (max_reverse/max_engine)
            brake = 0.0
        elif brake_pct > 1.0:
            # Normal braking at speed
            torque = 0.0
            brake = brake_pct / 100.0
        else:
            # Forward drive
            torque = pedal_pct / 100.0
            brake = 0.0

        response = {
            "car_index": self.car_index,
            "motor_torque_pct": torque,
            "brake_force_pct": brake,
            "steer_cmd_deg": steer_deg,
            "kill_relay": estop,
            "vehicle_state": state,
            "estop": estop,
            "ecu_heartbeats": {"CVC": 1, "FZC": 1, "RZC": 1, "SC": 1},
        }
        payload = json.dumps(response).encode("utf-8")
        try:
            self.actuator_sock.sendto(payload, self.actuator_addr)
        except Exception:
            pass

    def close(self) -> None:
        self.sensor_sock.close()
        self.actuator_sock.close()


# ── Main ─────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Godot ↔ vECU CAN Bridge")
    parser.add_argument("--cars", type=int, default=1, help="Number of cars (1-3)")
    parser.add_argument("--no-can", action="store_true", help="Run without CAN (standalone echo)")
    parser.add_argument("--rate", type=int, default=100, help="Bridge tick rate in Hz")
    parser.add_argument("--vcan-start", type=int, default=0, help="Starting vcan index (default 0 for GIL/Pi)")
    parser.add_argument("--godot-host", type=str,
                        default=os.environ.get("GODOT_HOST", "192.168.0.105"),
                        help="Godot machine IP address")
    args = parser.parse_args()

    num_cars = max(1, min(3, args.cars))
    tick_interval = 1.0 / args.rate
    use_can = not args.no_can and HAS_CAN

    # CAN_INTERFACE env var overrides vcan{N} construction for single-car GIL mode
    can_iface_env = os.environ.get("CAN_INTERFACE", "")

    print(f"[godot-bridge] GIL mode — {num_cars} car(s), CAN={'enabled' if use_can else 'disabled'}, {args.rate} Hz")

    bridges = []
    for i in range(num_cars):
        sensor_port = 5001 + i * 2    # Godot sends sensor data here
        actuator_port = 5002 + i * 2  # Bridge sends actuator commands here
        spi_port = 9100 + i           # CVC SPI pedal UDP port (9100 for car 0)
        # Use CAN_INTERFACE env var for single car, else construct vcan{N}
        if can_iface_env and num_cars == 1:
            can_iface = can_iface_env
        else:
            can_iface = f"vcan{i + args.vcan_start}"

        if use_can:
            bridge = CarBridge(i, can_iface, sensor_port, actuator_port, spi_port,
                               use_can=True, godot_host=args.godot_host)
        else:
            bridge = StandaloneEcho(i, sensor_port, actuator_port, godot_host=args.godot_host)

        bridges.append(bridge)
        print(f"[godot-bridge] Car {i}: sensor←:{sensor_port} actuator→:{actuator_port}"
              + (f" CAN:{can_iface}" if use_can else " (echo)"))

    # Command socket — listens for reset/control commands from Godot on port 5099
    cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    cmd_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    cmd_sock.bind(("0.0.0.0", 5099))
    cmd_sock.setblocking(False)
    compose_file = os.environ.get("COMPOSE_FILE", "/opt/taktflow-embedded/docker/docker-compose.gil.yml")

    def poll_commands():
        """Check for commands from Godot (reset_ecu, etc.)."""
        while True:
            try:
                data, addr = cmd_sock.recvfrom(4096)
                cmd = json.loads(data.decode("utf-8"))
                action = cmd.get("cmd", "")
                if action == "reset_ecu":
                    print("[godot-bridge] ECU RESET requested from Godot")
                    # Restart Docker containers in background
                    threading.Thread(target=_restart_docker, daemon=True).start()
                    # Send acknowledgement back
                    ack = json.dumps({"status": "resetting"}).encode("utf-8")
                    cmd_sock.sendto(ack, addr)
            except BlockingIOError:
                break
            except Exception:
                continue

    def _restart_docker():
        try:
            subprocess.run(
                ["docker", "compose", "-f", compose_file, "restart"],
                timeout=30, capture_output=True
            )
            print("[godot-bridge] Docker containers restarted")
        except Exception as e:
            print(f"[godot-bridge] Docker restart failed: {e}")

    print("[godot-bridge] Running. Press Ctrl+C to stop.")
    print(f"[godot-bridge] Command port: 5099 (send {{\"cmd\":\"reset_ecu\"}})")

    try:
        while True:
            t0 = time.monotonic()
            for b in bridges:
                b.tick()
            poll_commands()
            elapsed = time.monotonic() - t0
            sleep_time = tick_interval - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)
    except KeyboardInterrupt:
        print("\n[godot-bridge] Shutting down...")
    finally:
        for b in bridges:
            b.close()
        cmd_sock.close()


if __name__ == "__main__":
    main()
