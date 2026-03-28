"""WebSocket Telemetry Bridge — aggregates MQTT signals into JSON snapshots.

Subscribes to MQTT taktflow/# topics, maintains a single state snapshot,
and broadcasts it to all connected WebSocket clients at 4Hz.
"""

import asyncio
import json
import logging
import os
import time
from contextlib import asynccontextmanager

import paho.mqtt.client as mqtt
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import uvicorn

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [WS-BRIDGE] %(levelname)s %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("ws_bridge")

# Vehicle state names
VEHICLE_STATES = {0: "INIT", 1: "RUN", 2: "DEGRADED", 3: "LIMP", 4: "SAFE_STOP", 5: "SHUTDOWN"}

# CAN message → sender ECU (from CAN matrix / DBC)
MSG_SENDER = {
    "EStop_Broadcast": "CVC",
    "CVC_Heartbeat": "CVC", "FZC_Heartbeat": "FZC", "RZC_Heartbeat": "RZC",
    "Vehicle_State": "CVC", "Torque_Request": "CVC",
    "Steer_Command": "CVC", "Brake_Command": "CVC",
    "Steering_Status": "FZC", "Brake_Status": "FZC",
    "Brake_Fault": "FZC", "Motor_Cutoff_Req": "FZC", "Lidar_Distance": "FZC",
    "Motor_Status": "RZC", "Motor_Current": "RZC",
    "Motor_Temperature": "RZC", "Battery_Status": "RZC",
    "Body_Status": "BCM", "Light_Status": "BCM",
    "Indicator_State": "BCM", "Door_Lock_Status": "BCM",
    "DTC_Broadcast": "ANY",
}

# CAN message → hex ID (from DBC)
MSG_CAN_ID = {
    "EStop_Broadcast": "0x001",
    "CVC_Heartbeat": "0x010", "FZC_Heartbeat": "0x011", "RZC_Heartbeat": "0x012",
    "Vehicle_State": "0x100", "Torque_Request": "0x101",
    "Steer_Command": "0x102", "Brake_Command": "0x103",
    "Steering_Status": "0x200", "Brake_Status": "0x201",
    "Brake_Fault": "0x210", "Motor_Cutoff_Req": "0x211", "Lidar_Distance": "0x220",
    "Motor_Status": "0x300", "Motor_Current": "0x301",
    "Motor_Temperature": "0x302", "Battery_Status": "0x303",
    "Body_Status": "0x360", "Light_Status": "0x400",
    "Indicator_State": "0x401", "Door_Lock_Status": "0x402",
    "DTC_Broadcast": "0x500",
}


class TelemetryState:
    """Aggregated telemetry snapshot, updated by MQTT messages."""

    def __init__(self):
        self.motor_rpm = 0
        self.motor_current_ma = 0
        self.motor_temp_c = 0
        self.motor_temp2_c = 0
        self.motor_duty_pct = 0
        self.motor_direction = 0
        self.motor_faults = 0
        self.motor_derating = 100
        self.motor_enable = 0
        self.motor_overcurrent = 0

        self.steer_actual_deg = 0.0
        self.steer_commanded_deg = 0.0
        self.steer_fault = 0
        self.steer_servo_ma = 0

        self.brake_position_pct = 0
        self.brake_commanded_pct = 0
        self.brake_fault = 0
        self.brake_servo_ma = 0

        self.battery_voltage_mv = 12600
        self.battery_soc_pct = 100
        self.battery_status = 2  # normal

        self.lidar_distance_cm = 500
        self.lidar_zone = 3  # clear
        self.lidar_signal_strength = 8000
        self.lidar_sensor_status = 0

        self.vehicle_state = 0
        self.vehicle_fault_mask = 0
        self.torque_limit = 100
        self.speed_limit = 100

        self.hb_cvc = False
        self.hb_fzc = False
        self.hb_rzc = False

        self.anomaly_score = 0.0
        self.anomaly_alert = False
        self.anomaly_features: dict = {}

        self.can_msgs_sec = 0
        self.start_time = time.time()
        self._estop_active = False

        self.events: list[dict] = []
        self.can_log: list[dict] = []  # Rolling buffer of decoded CAN messages
        self.sap_notifications: list[dict] = []  # Rolling SAP QM notifications
        self.can_errors: list[dict] = []  # Rolling CAN error-frame events

        # Controller lock state (relayed from fault_inject via MQTT)
        self.control_locked = False
        self.control_client_id: str | None = None
        self.control_remaining_sec = 0

        # E2E test runner state (relayed from fault_inject via MQTT)
        self.test_progress: dict | None = None
        self.test_result: dict | None = None

        # Heartbeat tracking
        self._hb_cvc_ts = 0.0
        self._hb_fzc_ts = 0.0
        self._hb_rzc_ts = 0.0

        # DTC deduplication — track last event time per DTC code
        self._dtc_last_seen: dict[int, float] = {}

    # Vehicle speed derivation from motor RPM
    # Wheel radius 0.15m (30cm diameter), gear ratio 8:1
    _SPEED_FACTOR = 2 * 3.14159 * 0.15 * 60 / (8 * 1000)  # RPM -> km/h

    def to_snapshot(self) -> dict:
        """Build the JSON snapshot for WebSocket broadcast."""
        now = time.time()
        # Heartbeats are alive if seen within last 500ms (tolerant of Docker CAN jitter)
        hb_timeout = 0.5
        speed_kmh = round(self.motor_rpm * self._SPEED_FACTOR, 1)
        return {
            "ts": now,
            "motor": {
                "rpm": self.motor_rpm,
                "current_ma": self.motor_current_ma,
                "temp_c": self.motor_temp_c,
                "temp2_c": self.motor_temp2_c,
                "duty_pct": self.motor_duty_pct,
                "direction": self.motor_direction,
                "faults": self.motor_faults,
                "derating": self.motor_derating,
                "enable": self.motor_enable,
                "overcurrent": self.motor_overcurrent,
            },
            "steering": {
                "actual_deg": round(self.steer_actual_deg, 2),
                "commanded_deg": round(self.steer_commanded_deg, 2),
                "fault": self.steer_fault,
                "servo_ma": self.steer_servo_ma,
            },
            "brake": {
                "position_pct": self.brake_position_pct,
                "commanded_pct": self.brake_commanded_pct,
                "fault": self.brake_fault,
                "servo_ma": self.brake_servo_ma,
            },
            "battery": {
                "voltage_mv": self.battery_voltage_mv,
                "soc_pct": self.battery_soc_pct,
                "status": self.battery_status,
            },
            "lidar": {
                "distance_cm": self.lidar_distance_cm,
                "zone": self.lidar_zone,
                "signal_strength": self.lidar_signal_strength,
                "sensor_status": self.lidar_sensor_status,
            },
            "vehicle": {
                "state": self.vehicle_state,
                "state_name": VEHICLE_STATES.get(self.vehicle_state, "UNKNOWN"),
                "fault_mask": self.vehicle_fault_mask,
                "torque_limit": self.torque_limit,
                "speed_limit": self.speed_limit,
                "speed_kmh": speed_kmh,
            },
            "heartbeats": {
                "cvc": (now - self._hb_cvc_ts) < hb_timeout,
                "fzc": (now - self._hb_fzc_ts) < hb_timeout,
                "rzc": (now - self._hb_rzc_ts) < hb_timeout,
            },
            "anomaly": {
                "score": round(self.anomaly_score, 3),
                "alert": self.anomaly_alert,
                "features": self.anomaly_features,
            },
            "stats": {
                "can_msgs_sec": self.can_msgs_sec,
                "uptime_sec": int(now - self.start_time),
            },
            "events": self.events[-20:],  # Last 20 events
            "can_log": self.can_log[-50:],  # Last 50 CAN messages
            "sap_notifications": self.sap_notifications[-20:],
            "can_errors": self.can_errors[-20:],
            "control": {
                "locked": self.control_locked,
                "client_id": self.control_client_id or "",
                "remaining_sec": self.control_remaining_sec,
            },
            "test": {
                "progress": self.test_progress,
                "result": self.test_result,
            },
        }

    def add_event(self, event_type: str, message: str):
        """Add an event to the log."""
        self.events.append({
            "ts": time.time(),
            "type": event_type,
            "msg": message,
        })
        # Keep max 100 events in memory
        if len(self.events) > 100:
            self.events = self.events[-100:]


# Global state
state = TelemetryState()
ws_clients: set[WebSocket] = set()


def _parse_float(val: str, default: float = 0.0) -> float:
    try:
        return float(val)
    except (ValueError, TypeError):
        return default


def _parse_int(val: str, default: int = 0) -> int:
    try:
        return int(float(val))
    except (ValueError, TypeError):
        return default


def on_mqtt_message(client, userdata, msg):
    """Handle incoming MQTT messages — update telemetry state."""
    topic = msg.topic
    payload = msg.payload.decode("utf-8", errors="replace")

    # CAN signal topics: taktflow/can/{MsgName}/{SignalName}
    if topic.startswith("taktflow/can/"):
        parts = topic.split("/")
        if len(parts) >= 4:
            msg_name = parts[2]
            sig_name = parts[3]
            # DBC signal names have message prefix (e.g., "Motor_Status_MotorSpeed_RPM").
            # Strip the prefix to get the short field name for matching.
            prefix = msg_name + "_"
            short_sig = sig_name[len(prefix):] if sig_name.startswith(prefix) else sig_name
            _update_signal(msg_name, short_sig, payload)

    # Stats
    elif topic == "taktflow/telemetry/stats/can_msgs_per_sec":
        state.can_msgs_sec = _parse_int(payload)

    # Reset command — full clean reset to INIT state
    elif topic == "taktflow/command/reset":
        state.anomaly_score = 0.0
        state.anomaly_alert = False
        state.anomaly_features = {}
        state.motor_faults = 0
        state.motor_overcurrent = 0
        state.motor_rpm = 0
        state.motor_current_ma = 0
        state.motor_duty_pct = 0
        state.steer_fault = 0
        state.steer_actual_deg = 0.0
        state.steer_commanded_deg = 0.0
        state.brake_fault = 0
        state.brake_position_pct = 0
        state.brake_commanded_pct = 0
        state._estop_active = False
        state.vehicle_state = 0  # INIT — plant sim will transition to RUN after 3s
        state._dtc_last_seen.clear()
        state.sap_notifications.clear()
        state.can_errors.clear()
        state.events.clear()
        state.can_log.clear()
        state.add_event("info", "System reset — all state cleared, re-initializing")

    # Controller lock state (from fault_inject API)
    elif topic == "taktflow/control/lock":
        try:
            lock_data = json.loads(payload)
            state.control_locked = bool(lock_data.get("locked", False))
            state.control_client_id = lock_data.get("client_id") or None
            state.control_remaining_sec = int(lock_data.get("remaining_sec", 0))
        except (json.JSONDecodeError, TypeError, ValueError):
            pass

    # Anomaly — ML detector publishes JSON {"score": 0.75, "raw": -0.12, "ts": ..., "features": {...}}
    elif topic.startswith("taktflow/anomaly/"):
        if topic.endswith("/score"):
            try:
                data = json.loads(payload)
                state.anomaly_score = float(data.get("score", 0))
                features = data.get("features")
                if isinstance(features, dict):
                    state.anomaly_features = features
            except (json.JSONDecodeError, TypeError, ValueError):
                state.anomaly_score = _parse_float(payload)
            was_alert = state.anomaly_alert
            state.anomaly_alert = state.anomaly_score > 0.7
            if state.anomaly_alert and not was_alert:
                state.add_event("anomaly", f"ML anomaly detected — score {state.anomaly_score:.3f}")

    # DTC alerts (structured JSON from CAN gateway)
    elif topic.startswith("taktflow/alerts/dtc/"):
        try:
            alert = json.loads(payload)
            dtc_str = alert.get("dtc", "?")
            ecu = alert.get("ecu_source", 0)
            # Parse DTC number for deduplication with CAN signal path
            try:
                dtc_num = int(dtc_str, 16) if isinstance(dtc_str, str) and dtc_str.startswith("0x") else int(dtc_str)
            except (ValueError, TypeError):
                dtc_num = 0
            now = time.time()
            last = state._dtc_last_seen.get(dtc_num, 0) if dtc_num else 0
            if not dtc_num or now - last > 5.0:
                ecu_names = {1: "CVC", 2: "FZC", 3: "RZC", 4: "SC"}
                ecu_name = ecu_names.get(ecu, f"ECU {ecu}")
                state.add_event("dtc", f"DTC {dtc_str} from {ecu_name}")
            if dtc_num:
                state._dtc_last_seen[dtc_num] = now
        except json.JSONDecodeError:
            pass

    # E2E test runner progress (from fault_inject test runner)
    elif topic == "taktflow/test/progress":
        try:
            data = json.loads(payload)
            # Clear progress when suite completes (so frontend stops showing "running")
            state.test_progress = data if data.get("state") == "running" else None
        except (json.JSONDecodeError, TypeError):
            pass

    # E2E test runner final result (from fault_inject test runner)
    elif topic == "taktflow/test/result":
        try:
            state.test_result = json.loads(payload)
        except (json.JSONDecodeError, TypeError):
            pass

    # SAP QM events
    elif topic.startswith("taktflow/sap/"):
        try:
            evt = json.loads(payload)
            qn_id = evt.get("notification_id", "?")
            defect = evt.get("defect_text", "")
            dtc = evt.get("dtc_code", "")
            state.add_event("sap", f"SAP QM: {qn_id} — DTC {dtc} {defect}")
            # Add timestamp and notification_type for frontend display
            evt["ts"] = time.time()
            if "notification_type" not in evt:
                evt["notification_type"] = "Q3"
            if "description" not in evt:
                evt["description"] = evt.get("defect_text", "")
            state.sap_notifications.append(evt)
            if len(state.sap_notifications) > 20:
                state.sap_notifications = state.sap_notifications[-20:]
        except json.JSONDecodeError:
            pass

    # Structured CAN error-frame event from gateway
    elif topic == "taktflow/can/error":
        try:
            err = json.loads(payload)
            classes = err.get("classes", [])
            if isinstance(classes, list):
                class_label = ",".join(str(v) for v in classes[:3]) if classes else "UNKNOWN"
            else:
                class_label = str(classes)
            state.add_event("can", f"CAN error frame: {class_label}")
            state.can_errors.append(err)
            if len(state.can_errors) > 50:
                state.can_errors = state.can_errors[-50:]
        except json.JSONDecodeError:
            pass


def _append_can_log(msg_name: str, sig_name: str, value):
    """Append a decoded signal to the CAN log, coalescing signals from the same frame."""
    now = time.time()
    # Coalesce signals from the same CAN frame (arrive within 50ms)
    if state.can_log:
        last = state.can_log[-1]
        if last["msg_name"] == msg_name and (now - last["ts"]) < 0.05:
            last["signals"][sig_name] = value
            return
    state.can_log.append({
        "ts": now,
        "msg_name": msg_name,
        "msg_id": MSG_CAN_ID.get(msg_name, "0x???"),
        "sender": MSG_SENDER.get(msg_name, "?"),
        "signals": {sig_name: value},
    })
    if len(state.can_log) > 200:
        state.can_log = state.can_log[-200:]


def _update_signal(msg_name: str, sig_name: str, payload: str):
    """Update the telemetry state from a decoded CAN signal."""
    # Log every signal to the CAN rolling buffer
    _append_can_log(msg_name, sig_name, payload)

    # Motor Status (0x300)
    if msg_name == "Motor_Status":
        if sig_name == "MotorSpeed_RPM":
            state.motor_rpm = _parse_int(payload)
        elif sig_name == "MotorDirection":
            state.motor_direction = _parse_int(payload)
        elif sig_name == "MotorFaultStatus":
            old = state.motor_faults
            state.motor_faults = _parse_int(payload)
            if state.motor_faults and not old:
                state.add_event("fault", f"Motor fault detected (code {state.motor_faults})")
        elif sig_name == "DutyPercent":
            state.motor_duty_pct = _parse_int(payload)
        elif sig_name == "DeratingPercent":
            state.motor_derating = _parse_int(payload)
        elif sig_name == "MotorEnable":
            state.motor_enable = _parse_int(payload)

    # Motor Current (0x301)
    elif msg_name == "Motor_Current":
        if sig_name in ("Current_mA", "Phase_mA"):
            state.motor_current_ma = _parse_int(payload)
        elif sig_name == "OvercurrentFlag":
            old = state.motor_overcurrent
            new_oc = _parse_int(payload)
            # Suppress false OC flag when current < 25000mA (firmware threshold is 25A)
            if new_oc and state.motor_current_ma < 25000:
                new_oc = 0
            state.motor_overcurrent = new_oc
            if state.motor_overcurrent and not old:
                state.add_event("fault", f"Motor overcurrent! I={state.motor_current_ma}mA")

    # Motor Temperature (0x302)
    elif msg_name == "Motor_Temperature":
        if sig_name == "WindingTemp1_C":
            state.motor_temp_c = _parse_int(payload)
        elif sig_name == "WindingTemp2_C":
            state.motor_temp2_c = _parse_int(payload)

    # Steering Status (0x200)
    elif msg_name == "Steering_Status":
        if sig_name == "ActualAngle":
            state.steer_actual_deg = _parse_float(payload)
        elif sig_name == "CommandedAngle":
            state.steer_commanded_deg = _parse_float(payload)
        elif sig_name == "SteerFaultStatus":
            old = state.steer_fault
            state.steer_fault = _parse_int(payload)
            if state.steer_fault and not old:
                state.add_event("fault", "Steering fault detected — servo failure")
        elif sig_name == "ServoCurrent_mA":
            state.steer_servo_ma = _parse_int(payload)

    # Brake Status (0x201)
    elif msg_name == "Brake_Status":
        if sig_name == "BrakePosition":
            state.brake_position_pct = _parse_int(payload)
        elif sig_name == "BrakeCommandEcho":
            state.brake_commanded_pct = _parse_int(payload)
        elif sig_name == "BrakeFaultStatus":
            old = state.brake_fault
            state.brake_fault = _parse_int(payload)
            if state.brake_fault and not old:
                state.add_event("fault", "Brake fault detected")
        elif sig_name == "ServoCurrent_mA":
            state.brake_servo_ma = _parse_int(payload)

    # Battery Status (0x303)
    elif msg_name == "Battery_Status":
        if sig_name == "BatteryVoltage_mV":
            state.battery_voltage_mv = _parse_int(payload)
        elif sig_name == "BatterySOC":
            state.battery_soc_pct = _parse_int(payload)
        elif sig_name in ("BatteryStatus", "Level"):
            state.battery_status = _parse_int(payload)

    # Lidar Distance (0x220)
    elif msg_name == "Lidar_Distance":
        if sig_name in ("Distance_cm", "Range_cm"):
            val = _parse_int(payload)
            if val > 0:
                state.lidar_distance_cm = val
        elif sig_name in ("ObstacleZone", "Zone"):
            val = _parse_int(payload)
            if val > 0:
                state.lidar_zone = val
        elif sig_name in ("SignalStrength",):
            val = _parse_int(payload)
            if val > 0:
                state.lidar_signal_strength = val
        elif sig_name == "SensorStatus":
            state.lidar_sensor_status = _parse_int(payload)

    # Vehicle State (0x100)
    elif msg_name == "Vehicle_State":
        if sig_name in ("Mode", "VehicleState"):
            old = state.vehicle_state
            new = _parse_int(payload)
            if old != new:
                old_name = VEHICLE_STATES.get(old, "?")
                new_name = VEHICLE_STATES.get(new, "?")
                state.add_event("state", f"Vehicle state: {old_name} -> {new_name}")
            state.vehicle_state = new
        elif sig_name == "FaultMask":
            state.vehicle_fault_mask = _parse_int(payload)
        elif sig_name == "TorqueLimit":
            state.torque_limit = _parse_int(payload)
        elif sig_name == "SpeedLimit":
            state.speed_limit = _parse_int(payload)

    # E-Stop — only react to the EStop_Active signal, not Source/E2E fields
    elif msg_name == "EStop_Broadcast" and sig_name in ("EStop_Active", "Active"):
        val = _parse_int(payload)
        if val and not getattr(state, "_estop_active", False):
            state._estop_active = True
            state.add_event("fault", "E-STOP activated! Emergency shutdown")
        elif not val and getattr(state, "_estop_active", False):
            state._estop_active = False
            state.add_event("info", "E-STOP cleared")

    # DTC_Broadcast (0x500) — belt-and-suspenders with taktflow/alerts/dtc/
    elif msg_name == "DTC_Broadcast":
        if sig_name in ("DTC_Number", "Number"):
            dtc_num = _parse_int(payload)
            if dtc_num:
                now = time.time()
                last = state._dtc_last_seen.get(dtc_num, 0)
                if now - last > 5.0:
                    state.add_event("dtc", f"DTC 0x{dtc_num:04X} detected")
                state._dtc_last_seen[dtc_num] = now

    # Heartbeats
    elif msg_name == "CVC_Heartbeat":
        state._hb_cvc_ts = time.time()
    elif msg_name == "FZC_Heartbeat":
        state._hb_fzc_ts = time.time()
    elif msg_name == "RZC_Heartbeat":
        state._hb_rzc_ts = time.time()


def setup_mqtt() -> mqtt.Client:
    """Create and connect the MQTT client."""
    mqtt_host = os.environ.get("MQTT_HOST", "localhost")
    mqtt_port = int(os.environ.get("MQTT_PORT", "1883"))

    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id="ws-bridge",
    )
    client.on_message = on_mqtt_message

    def on_connect(c, userdata, flags, rc, properties=None):
        if rc == 0:
            log.info("Connected to MQTT broker at %s:%d", mqtt_host, mqtt_port)
            c.subscribe("taktflow/#", qos=0)
        else:
            log.error("MQTT connect failed: rc=%d", rc)

    client.on_connect = on_connect
    mqtt_user = os.environ.get("MQTT_USER", "")
    mqtt_pass = os.environ.get("MQTT_PASSWORD", "")
    if mqtt_user:
        client.username_pw_set(mqtt_user, mqtt_pass)
    client.connect_async(mqtt_host, mqtt_port, keepalive=30)
    client.loop_start()
    return client


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Start MQTT client on app startup, stop on shutdown."""
    mqtt_client = setup_mqtt()
    broadcast_task = asyncio.create_task(broadcast_loop())
    log.info("WebSocket bridge started")
    yield
    broadcast_task.cancel()
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    log.info("WebSocket bridge stopped")


app = FastAPI(title="Taktflow Telemetry Bridge", lifespan=lifespan)


@app.websocket("/ws/telemetry")
async def telemetry_ws(websocket: WebSocket):
    """WebSocket endpoint for live telemetry."""
    await websocket.accept()
    ws_clients.add(websocket)
    log.info("WebSocket client connected (%d total)", len(ws_clients))

    try:
        # Keep connection alive — client doesn't send data
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        pass
    finally:
        ws_clients.discard(websocket)
        log.info("WebSocket client disconnected (%d remaining)", len(ws_clients))


@app.get("/health")
async def health():
    """Health check endpoint."""
    return {
        "status": "ok",
        "clients": len(ws_clients),
        "uptime_sec": int(time.time() - state.start_time),
    }


async def broadcast_loop():
    """Broadcast telemetry snapshot to all WebSocket clients at 4Hz."""
    while True:
        if ws_clients:
            snapshot = state.to_snapshot()
            data = json.dumps(snapshot)
            disconnected = set()
            for ws in ws_clients:
                try:
                    await ws.send_text(data)
                except Exception:
                    disconnected.add(ws)
            ws_clients.difference_update(disconnected)

        await asyncio.sleep(0.25)  # 4Hz — smooth for visual dashboard


def main():
    port = int(os.environ.get("WS_PORT", "8080"))
    uvicorn.run(app, host="0.0.0.0", port=port, log_level="info")


if __name__ == "__main__":
    main()
