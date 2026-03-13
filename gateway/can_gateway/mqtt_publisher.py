"""MQTT publisher — publishes decoded CAN signals to MQTT topics.

Topic structure:
    taktflow/can/{MessageName}/{SignalName}  -> value
    taktflow/telemetry/stats/can_msgs_per_sec -> rate
    taktflow/alerts/dtc/{dtc_code}           -> { json }
"""

import json
import logging
import time
from datetime import datetime, timezone

import paho.mqtt.client as mqtt

log = logging.getLogger("can_gateway.mqtt")

TOPIC_PREFIX = "taktflow"

# DTC-related signals from DTC_Broadcast (0x500 = 1280)
DTC_MSG_NAME = "DTC_Broadcast"


class MqttPublisher:
    """Publishes decoded CAN signals to MQTT broker."""

    def __init__(self, host: str = "localhost", port: int = 1883):
        self.host = host
        self.port = port
        self._client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id="can-gateway",
        )
        self._connected = False

        # Latest signal values for aggregated telemetry
        self._latest: dict[str, float | str] = {}
        self._last_publish_time: dict[str, float] = {}
        self._last_telemetry_time = time.monotonic()

        # Stats
        self._msg_count = 0
        self._last_stats_time = time.monotonic()
        self._msgs_per_sec = 0.0

        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect

    def _on_connect(self, client, userdata, flags, rc, properties=None):
        if rc == 0:
            self._connected = True
            log.info("Connected to MQTT broker at %s:%d", self.host, self.port)
        else:
            log.error("MQTT connect failed: rc=%d", rc)

    def _on_disconnect(self, client, userdata, flags, rc, properties=None):
        self._connected = False
        log.warning("Disconnected from MQTT broker (rc=%d)", rc)

    def connect(self):
        """Connect to the MQTT broker with auto-reconnect."""
        self._client.connect_async(self.host, self.port, keepalive=30)
        self._client.loop_start()

    def stop(self):
        """Disconnect and stop the MQTT loop."""
        self._client.loop_stop()
        self._client.disconnect()

    # E2E header signals — change every frame, not useful for dashboard
    _E2E_SIGNALS = frozenset({"E2E_DataID", "E2E_AliveCounter", "E2E_CRC8"})

    # Minimum interval (seconds) between MQTT publishes per signal.
    # CAN frames arrive every 10ms but dashboard only needs ~5 Hz updates.
    _MIN_PUBLISH_INTERVAL = 0.2  # 200ms = 5 Hz max per signal

    def publish_signals(self, msg_name: str, signals: dict):
        """Publish each signal as a separate MQTT topic (rate-limited)."""
        if not self._connected:
            return

        now = time.monotonic()
        for signal_name, value in signals.items():
            if signal_name in self._E2E_SIGNALS:
                continue

            key = f"{msg_name}/{signal_name}"

            # Rate-limit: skip if same signal published recently
            last_time = self._last_publish_time.get(key, 0.0)
            if (now - last_time) < self._MIN_PUBLISH_INTERVAL:
                self._latest[key] = value
                continue

            topic = f"{TOPIC_PREFIX}/can/{msg_name}/{signal_name}"
            payload = str(value) if not isinstance(value, (dict, list)) else json.dumps(value)
            self._client.publish(topic, payload, qos=0, retain=True)

            self._latest[key] = value
            self._last_publish_time[key] = now

        self._msg_count += 1

        # Check for DTC broadcast — publish as alert
        if msg_name == DTC_MSG_NAME:
            self._publish_dtc_alert(signals)

    def publish_can_error(self, error_info: dict):
        """Publish a structured CAN error-frame event."""
        if not self._connected:
            return

        payload = {
            "ts": time.time(),
            **error_info,
        }
        self._client.publish(
            f"{TOPIC_PREFIX}/can/error",
            json.dumps(payload),
            qos=1,
            retain=True,
        )

    def _publish_dtc_alert(self, signals: dict):
        """Publish a DTC alert with structured JSON."""
        dtc_number = signals.get("DTC_Number", 0)
        if dtc_number == 0:
            return

        alert = {
            "dtc": f"0x{int(dtc_number):06X}",
            "status": int(signals.get("DTC_Status", 0)),
            "ecu_source": int(signals.get("ECU_Source", 0)),
            "occurrence": int(signals.get("OccurrenceCount", 0)),
            "freeze_frame": [
                int(signals.get("FreezeFrame0", 0)),
                int(signals.get("FreezeFrame1", 0)),
                int(signals.get("FreezeFrame2", 0)),
            ],
            "ts": time.time(),
        }
        topic = f"{TOPIC_PREFIX}/alerts/dtc/{alert['dtc']}"
        alert_json = json.dumps(alert)
        self._client.publish(topic, alert_json, qos=1)

        # Also publish to vehicle/dtc/new for xIL verdict checking
        self._client.publish("vehicle/dtc/new", alert_json, qos=1)

    def publish_telemetry_aggregate(self):
        """Publish aggregated telemetry to vehicle/telemetry (5s interval)."""
        now = time.monotonic()
        if (now - self._last_telemetry_time) < 5.0:
            return
        self._last_telemetry_time = now

        if not self._connected:
            return

        # Map internal signal keys to telemetry JSON fields
        voltage_mv = self._latest.get("Battery_Status/BatteryVoltage_mV", 0)
        try:
            voltage_v = float(voltage_mv) / 1000.0
        except (TypeError, ValueError):
            voltage_v = 0.0

        payload = {
            "motor_speed_rpm": self._latest.get(
                "Motor_Status/MotorSpeed_RPM", 0
            ),
            "battery_voltage_v": round(voltage_v, 2),
            "vehicle_state": self._latest.get(
                "Vehicle_State/State", "UNKNOWN"
            ),
            "timestamp": datetime.now(timezone.utc).isoformat(),
        }
        self._client.publish(
            "vehicle/telemetry", json.dumps(payload), qos=1, retain=True
        )

    def publish_stats(self):
        """Publish CAN message rate stats. Called periodically."""
        now = time.monotonic()
        elapsed = now - self._last_stats_time
        if elapsed >= 1.0:
            self._msgs_per_sec = self._msg_count / elapsed
            self._msg_count = 0
            self._last_stats_time = now

            if self._connected:
                self._client.publish(
                    f"{TOPIC_PREFIX}/telemetry/stats/can_msgs_per_sec",
                    f"{self._msgs_per_sec:.0f}",
                    qos=0,
                    retain=True,
                )

        # Also publish aggregated telemetry
        self.publish_telemetry_aggregate()

    @property
    def msgs_per_sec(self) -> float:
        return self._msgs_per_sec
