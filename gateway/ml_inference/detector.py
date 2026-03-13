#!/usr/bin/env python3
"""
MQTT-based Isolation Forest anomaly detector for CAN telemetry.

Subscribes to CAN signal topics, maintains a 1-second rolling window
(10 samples at 10 Hz), computes features every second, runs inference,
and publishes an anomaly score (0 = normal, 1 = anomalous).

If score > 0.7 a soft DTC alert is published on the DTC topic.
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import time
from collections import deque
from pathlib import Path

import joblib
import numpy as np
import paho.mqtt.client as mqtt

from ml_inference.train_anomaly import (
    MODEL_PATH,
    SCALER_PATH,
    save_model,
    train_model,
)

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))

WINDOW_SIZE = 10  # 10 samples @ 10 Hz = 1-second window
INFERENCE_INTERVAL_S = 1.0
DTC_SCORE_THRESHOLD = 0.7
DTC_CODE = "0xE601"

# MQTT topics — subscriptions
TOPIC_CURRENT = "taktflow/can/Motor_Current/Current_mA"
TOPIC_TEMP = "taktflow/can/Motor_Temperature/WindingTemp1_C"
TOPIC_RPM = "taktflow/can/Motor_Status/MotorSpeed_RPM"
TOPIC_VOLTAGE = "taktflow/can/Battery_Status/BatteryVoltage_mV"

# MQTT topics — publications
TOPIC_SCORE = "taktflow/anomaly/score"
TOPIC_DTC = f"taktflow/alerts/dtc/{DTC_CODE}"

TOPIC_RESET = "taktflow/command/reset"

SUBSCRIPTIONS = [TOPIC_CURRENT, TOPIC_TEMP, TOPIC_RPM, TOPIC_VOLTAGE, TOPIC_RESET]


# ---------------------------------------------------------------------------
# Rolling buffers
# ---------------------------------------------------------------------------
class SensorBuffers:
    """Thread-safe(ish) ring buffers for the four CAN signals."""

    def __init__(self, maxlen: int = WINDOW_SIZE) -> None:
        self.current: deque[float] = deque(maxlen=maxlen)
        self.temp: deque[float] = deque(maxlen=maxlen)
        self.rpm: deque[float] = deque(maxlen=maxlen)
        self.voltage: deque[float] = deque(maxlen=maxlen)

    def has_data(self) -> bool:
        """Return True if at least one sample exists in the current buffer."""
        return len(self.current) > 0

    def compute_features(self) -> np.ndarray | None:
        """Compute the 5-feature vector from the rolling window.

        Returns None if the current buffer is empty (no data yet).
        Features:
            [motor_current_mean, motor_current_std, motor_temp, rpm, battery_voltage]
        """
        if not self.has_data():
            return None

        current_arr = np.array(self.current)
        motor_current_mean = float(np.mean(current_arr))
        motor_current_std = float(np.std(current_arr)) if len(current_arr) > 1 else 5.0

        # For temp / rpm / voltage use the latest value, falling back to a
        # safe default if no message has arrived yet.
        motor_temp = self.temp[-1] if self.temp else 40.0
        rpm_val = self.rpm[-1] if self.rpm else 0.0
        voltage_val = self.voltage[-1] if self.voltage else 12500.0

        return np.array([[
            motor_current_mean,
            motor_current_std,
            motor_temp,
            rpm_val,
            voltage_val,
        ]])


# ---------------------------------------------------------------------------
# Model loading (with auto-train fallback)
# ---------------------------------------------------------------------------
def load_or_train_model():
    """Load persisted model + scaler, or train from scratch if missing."""
    if MODEL_PATH.exists() and SCALER_PATH.exists():
        logger.info("Loading model from %s", MODEL_PATH)
        model = joblib.load(MODEL_PATH)
        scaler = joblib.load(SCALER_PATH)
    else:
        logger.warning("Model files not found — training on synthetic data …")
        model, scaler = train_model()
        save_model(model, scaler)
    return model, scaler


# ---------------------------------------------------------------------------
# Score conversion
# ---------------------------------------------------------------------------
def raw_score_to_anomaly(raw_score: float) -> float:
    """Map IsolationForest decision_function output to 0–1 range.

    IsolationForest.decision_function returns negative values for anomalies
    and positive for inliers.  We map roughly:
        +0.05 (clearly normal)  →  0.0
        -0.17 (clearly anomalous) →  0.7  (DTC threshold)
        -0.26 (extreme)          →  1.0
    and clamp to [0, 1].
    """
    normalized = 0.15 - (raw_score / 0.30)
    return float(np.clip(normalized, 0.0, 1.0))


# ---------------------------------------------------------------------------
# MQTT callbacks
# ---------------------------------------------------------------------------
def make_on_connect(buffers: SensorBuffers):
    """Factory for the on_connect callback."""

    def on_connect(client: mqtt.Client, userdata, flags, reason_code, properties=None):
        if reason_code == 0:
            logger.info("Connected to MQTT broker at %s:%s", MQTT_HOST, MQTT_PORT)
            for topic in SUBSCRIPTIONS:
                client.subscribe(topic, qos=1)
                logger.info("  subscribed → %s", topic)
        else:
            logger.error("MQTT connect failed: reason_code=%s", reason_code)

    return on_connect


def make_on_message(buffers: SensorBuffers, reset_flag: dict):
    """Factory for the on_message callback."""

    def on_message(client: mqtt.Client, userdata, msg: mqtt.MQTTMessage):
        # Handle reset command
        if msg.topic == TOPIC_RESET:
            logger.info("Reset command received — clearing buffers")
            buffers.current.clear()
            buffers.temp.clear()
            buffers.rpm.clear()
            buffers.voltage.clear()
            reset_flag["pending"] = True
            return

        try:
            value = float(msg.payload.decode("utf-8"))
        except (ValueError, UnicodeDecodeError):
            # Try JSON payload: {"value": ...}
            try:
                payload = json.loads(msg.payload)
                value = float(payload.get("value", payload))
            except Exception:
                logger.debug("Unparseable payload on %s: %r", msg.topic, msg.payload)
                return

        if msg.topic == TOPIC_CURRENT:
            buffers.current.append(value)
        elif msg.topic == TOPIC_TEMP:
            buffers.temp.append(value)
        elif msg.topic == TOPIC_RPM:
            buffers.rpm.append(value)
        elif msg.topic == TOPIC_VOLTAGE:
            buffers.voltage.append(value)

    return on_message


# ---------------------------------------------------------------------------
# Inference loop
# ---------------------------------------------------------------------------
async def inference_loop(client: mqtt.Client, buffers: SensorBuffers,
                        model, scaler, reset_flag: dict):
    """Run inference every INFERENCE_INTERVAL_S seconds."""
    logger.info("Inference loop started (interval=%.1fs)", INFERENCE_INTERVAL_S)

    while True:
        await asyncio.sleep(INFERENCE_INTERVAL_S)

        # Handle pending reset — publish score 0.0 immediately
        if reset_flag.get("pending"):
            reset_flag["pending"] = False
            score_payload = json.dumps({
                "score": 0.0,
                "raw": 0.0,
                "ts": time.time(),
                "features": {},
            })
            client.publish(TOPIC_SCORE, score_payload, qos=0)
            logger.info("Reset: anomaly score cleared to 0.0")
            continue

        features = buffers.compute_features()
        if features is None:
            continue

        try:
            features_scaled = scaler.transform(features)
            raw_scores = model.decision_function(features_scaled)
            raw_score = float(raw_scores[0])
            anomaly_score = raw_score_to_anomaly(raw_score)
        except Exception:
            logger.exception("Inference error")
            continue

        # Build feature dict for frontend display
        feat = features[0]
        feature_dict = {
            "current_mean": round(float(feat[0]), 1),
            "current_std": round(float(feat[1]), 1),
            "temp": round(float(feat[2]), 1),
            "rpm": round(float(feat[3]), 0),
            "voltage": round(float(feat[4]), 0),
        }

        # Publish anomaly score with features
        score_payload = json.dumps({
            "score": round(anomaly_score, 4),
            "raw": round(raw_score, 6),
            "ts": time.time(),
            "features": feature_dict,
        })
        client.publish(TOPIC_SCORE, score_payload, qos=0)
        logger.debug("anomaly score=%.4f  raw=%.6f", anomaly_score, raw_score)

        # Publish to vehicle/alerts for xIL verdict checking
        alert_payload = json.dumps({
            "anomaly_score": round(anomaly_score, 4),
            "anomaly_type": "motor_current",
            "features": feature_dict,
            "ts": time.time(),
        })
        client.publish("vehicle/alerts", alert_payload, qos=1)

        # DTC alert if above threshold
        if anomaly_score > DTC_SCORE_THRESHOLD:
            dtc_payload = json.dumps({
                "dtc": DTC_CODE,
                "type": "ML_ANOMALY",
                "score": round(anomaly_score, 4),
                "severity": "SOFT",
                "ts": time.time(),
            })
            client.publish(TOPIC_DTC, dtc_payload, qos=1)
            logger.warning(
                "DTC %s triggered — anomaly score %.4f > %.2f",
                DTC_CODE, anomaly_score, DTC_SCORE_THRESHOLD,
            )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
async def main():
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s — %(message)s",
    )

    # Load / train model
    model, scaler = load_or_train_model()
    logger.info("Model ready.")

    # Sensor buffers
    buffers = SensorBuffers(maxlen=WINDOW_SIZE)
    reset_flag: dict = {"pending": False}

    # MQTT client (paho-mqtt v2 API)
    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id="taktflow-ml-detector",
    )
    client.on_connect = make_on_connect(buffers)
    client.on_message = make_on_message(buffers, reset_flag)

    # Connect (non-blocking loop)
    logger.info("Connecting to MQTT broker %s:%s …", MQTT_HOST, MQTT_PORT)
    client.connect_async(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_start()

    try:
        await inference_loop(client, buffers, model, scaler, reset_flag)
    except asyncio.CancelledError:
        logger.info("Shutting down …")
    finally:
        client.loop_stop()
        client.disconnect()
        logger.info("Disconnected from MQTT broker.")


if __name__ == "__main__":
    asyncio.run(main())
