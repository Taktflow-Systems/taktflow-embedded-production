"""Cloud bridge — forwards local MQTT topics to AWS IoT Core.

Subscribes to local Mosquitto broker, republishes to AWS IoT Core
over TLS 1.2 with X.509 client certificate authentication.
Buffers up to 100 messages during AWS disconnects (SWR-GW-001).
"""

from __future__ import annotations

import logging
import os
import ssl
from pathlib import Path

import paho.mqtt.client as mqtt

from cloud_connector.buffer import MessageBuffer

logger = logging.getLogger(__name__)

# Topics forwarded to cloud (budget-safe: ~520K msgs/month within free tier)
FORWARDED_TOPICS: list[str] = [
    "vehicle/telemetry",   # aggregated, 1 msg per 5s
    "vehicle/dtc/new",     # on-event only
    "vehicle/alerts",      # on-event only (ML anomaly alerts)
]

# Configuration from environment
MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
AWS_IOT_ENDPOINT = os.getenv("AWS_IOT_ENDPOINT", "")
AWS_IOT_PORT = int(os.getenv("AWS_IOT_PORT", "8883"))
AWS_CERT_DIR = os.getenv("AWS_CERT_DIR", "/certs")
DEVICE_ID = os.getenv("DEVICE_ID", "taktflow-pi-001")


class CloudBridge:
    """Unidirectional bridge: local Mosquitto → AWS IoT Core."""

    def __init__(self) -> None:
        self._buffer = MessageBuffer(maxlen=100)
        self._aws_connected = False
        self._msgs_sent = 0

        # Local MQTT client (subscribe to Mosquitto)
        self._local_client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id="cloud-connector-local",
        )
        self._local_client.on_connect = self._on_local_connect
        self._local_client.on_message = self._on_local_message

        # AWS IoT Core MQTT client (publish)
        self._aws_client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=DEVICE_ID,
        )
        self._aws_client.on_connect = self._on_aws_connect
        self._aws_client.on_disconnect = self._on_aws_disconnect

        # TLS for AWS IoT Core
        if AWS_IOT_ENDPOINT:
            cert_dir = Path(AWS_CERT_DIR)
            self._aws_client.tls_set(
                ca_certs=str(cert_dir / "root-CA.pem"),
                certfile=str(cert_dir / "device.cert.pem"),
                keyfile=str(cert_dir / "device.private.key"),
                tls_version=ssl.PROTOCOL_TLSv1_2,
            )
            # Exponential backoff: 1s, 2s, 4s, ... max 60s (SWR-GW-001)
            self._aws_client.reconnect_delay_set(
                min_delay=1, max_delay=60
            )

    # -- Properties for health reporting --

    @property
    def aws_connected(self) -> bool:
        return self._aws_connected

    @property
    def msgs_sent(self) -> int:
        return self._msgs_sent

    @property
    def msgs_buffered(self) -> int:
        return self._buffer.pending

    # -- Local MQTT callbacks --

    def _on_local_connect(self, client, userdata, flags, reason_code, properties=None):
        if reason_code == 0:
            logger.info("Connected to local MQTT at %s:%s", MQTT_HOST, MQTT_PORT)
            for topic in FORWARDED_TOPICS:
                client.subscribe(topic, qos=1)
                logger.info("  subscribed → %s", topic)
        else:
            logger.error("Local MQTT connect failed: rc=%s", reason_code)

    def _on_local_message(self, client, userdata, msg):
        """Forward received local message to AWS (or buffer if disconnected)."""
        if self._aws_connected:
            self._aws_client.publish(msg.topic, msg.payload, qos=1)
            self._msgs_sent += 1
        else:
            self._buffer.enqueue(msg.topic, msg.payload)
            logger.debug("Buffered %s (%d pending)", msg.topic, self._buffer.pending)

    # -- AWS IoT Core callbacks --

    def _on_aws_connect(self, client, userdata, flags, reason_code, properties=None):
        if reason_code == 0:
            self._aws_connected = True
            logger.info("Connected to AWS IoT Core at %s:%s", AWS_IOT_ENDPOINT, AWS_IOT_PORT)
            # Drain offline buffer
            buffered = self._buffer.drain()
            if buffered:
                logger.info("Draining %d buffered messages", len(buffered))
                for topic, payload in buffered:
                    client.publish(topic, payload, qos=1)
                    self._msgs_sent += 1
        else:
            logger.error("AWS IoT connect failed: rc=%s", reason_code)

    def _on_aws_disconnect(self, client, userdata, flags, reason_code, properties=None):
        self._aws_connected = False
        logger.warning("Disconnected from AWS IoT Core (rc=%s)", reason_code)

    # -- Lifecycle --

    def start(self) -> None:
        """Connect both MQTT clients and start network loops."""
        mqtt_user = os.environ.get("MQTT_USER", "")
        mqtt_pass = os.environ.get("MQTT_PASSWORD", "")
        if mqtt_user:
            self._local_client.username_pw_set(mqtt_user, mqtt_pass)
        logger.info("Connecting to local MQTT %s:%s", MQTT_HOST, MQTT_PORT)
        self._local_client.connect_async(MQTT_HOST, MQTT_PORT, keepalive=60)
        self._local_client.loop_start()

        if AWS_IOT_ENDPOINT:
            logger.info("Connecting to AWS IoT Core %s:%s", AWS_IOT_ENDPOINT, AWS_IOT_PORT)
            self._aws_client.connect_async(AWS_IOT_ENDPOINT, AWS_IOT_PORT, keepalive=60)
            self._aws_client.loop_start()
        else:
            logger.warning("AWS_IOT_ENDPOINT not set — running in local-only mode")

    def stop(self) -> None:
        """Disconnect both MQTT clients."""
        self._local_client.loop_stop()
        self._local_client.disconnect()
        if AWS_IOT_ENDPOINT:
            self._aws_client.loop_stop()
            self._aws_client.disconnect()
        logger.info("Cloud bridge stopped (sent %d messages total)", self._msgs_sent)
