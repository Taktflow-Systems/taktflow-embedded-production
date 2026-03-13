"""MQTT subscriber that bridges DTC alerts to SAP QM quality notifications.

Listens on ``taktflow/alerts/dtc/#`` for incoming Diagnostic Trouble Codes,
maps them through the defect catalog, creates a notification via the local
SAP QM mock API, and publishes the resulting notification event to
``taktflow/sap/notification`` for the WebSocket bridge to pick up.
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional

import paho.mqtt.client as mqtt

from .database import create_notification
from .defect_catalog import ECU_PLANTS, map_dtc_to_defect

logger = logging.getLogger("sap_qm_mock.dtc_connector")

MQTT_HOST = os.environ.get("MQTT_HOST", "localhost")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))

SUBSCRIBE_TOPIC = "taktflow/alerts/dtc/#"
PUBLISH_TOPIC = "taktflow/sap/notification"


class DTCConnector:
    """MQTT-based connector that turns DTC alerts into SAP QM notifications."""

    def __init__(self) -> None:
        self._client: Optional[mqtt.Client] = None
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._running: bool = False

    # ------------------------------------------------------------------
    # MQTT callbacks (run on the paho network thread)
    # ------------------------------------------------------------------

    def _on_connect(
        self,
        client: mqtt.Client,
        userdata: Any,
        flags: Any,
        rc: int,
        properties: Any = None,
    ) -> None:
        if rc == 0:
            logger.info("MQTT connected to %s:%d", MQTT_HOST, MQTT_PORT)
            client.subscribe(SUBSCRIBE_TOPIC)
            logger.info("Subscribed to %s", SUBSCRIBE_TOPIC)
        else:
            logger.error("MQTT connection failed with code %d", rc)

    def _on_message(
        self,
        client: mqtt.Client,
        userdata: Any,
        msg: mqtt.MQTTMessage,
    ) -> None:
        """Handle an incoming DTC alert message."""
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
            logger.info(
                "DTC alert received on %s: %s", msg.topic, payload
            )
            # Schedule the async handler on the event loop
            if self._loop is not None and self._loop.is_running():
                asyncio.run_coroutine_threadsafe(
                    self._handle_dtc_alert(payload, client),
                    self._loop,
                )
        except (json.JSONDecodeError, UnicodeDecodeError) as exc:
            logger.warning("Invalid DTC alert payload: %s", exc)

    def _on_disconnect(
        self,
        client: mqtt.Client,
        userdata: Any,
        rc: int,
        properties: Any = None,
    ) -> None:
        if rc != 0:
            logger.warning("MQTT disconnected unexpectedly (rc=%d)", rc)

    # ------------------------------------------------------------------
    # Alert processing
    # ------------------------------------------------------------------

    async def _handle_dtc_alert(
        self, payload: dict[str, Any], client: mqtt.Client
    ) -> None:
        """Map a DTC alert to a SAP QM notification and publish the event."""
        # Field names match mqtt_publisher.py: "dtc", "ecu_source"
        dtc_code: str = payload.get("dtc") or payload.get("dtc_code", "")
        ecu_source: Optional[int] = payload.get("ecu_source") or payload.get("ecu_id")
        description: str = payload.get(
            "description", f"DTC alert: {dtc_code}"
        )

        defect = map_dtc_to_defect(dtc_code)
        if defect is None:
            logger.warning("Unknown DTC code %s — skipping", dtc_code)
            return

        plant = ECU_PLANTS.get(ecu_source) if ecu_source else None

        notification_data: dict[str, Any] = {
            "notification_type": "Q3",  # Internal notification
            "description": description,
            "material": payload.get("material"),
            "plant": plant,
            "defect_code": defect["code"],
            "dtc_code": dtc_code,
            "ecu_source": ecu_source,
            "priority": defect["priority"],
            "items": [
                {
                    "item_number": "0001",
                    "defect_code": defect["code"],
                    "defect_text": defect["text"],
                    "cause_code": dtc_code,
                    "cause_text": f"DTC {dtc_code} from ECU {ecu_source or 'unknown'}",
                }
            ],
        }

        record = await create_notification(notification_data)
        logger.info(
            "Created notification %s from DTC %s",
            record["notification_id"],
            dtc_code,
        )

        # Generate 8D report
        self._generate_8d_report(record, dtc_code, defect, plant)

        # Publish event for the WebSocket bridge
        event_payload = json.dumps(
            {
                "event": "notification_created",
                "notification_id": record["notification_id"],
                "dtc_code": dtc_code,
                "defect_code": defect["code"],
                "defect_text": defect["text"],
                "plant": plant,
                "priority": defect["priority"],
                "status": "OSNO",
            }
        )
        client.publish(PUBLISH_TOPIC, event_payload, qos=1)
        logger.info(
            "Published SAP event to %s for %s",
            PUBLISH_TOPIC,
            record["notification_id"],
        )

    # ------------------------------------------------------------------
    # 8D Report generation
    # ------------------------------------------------------------------

    @staticmethod
    def _generate_8d_report(
        record: dict[str, Any],
        dtc_code: str,
        defect: dict[str, Any],
        plant: str | None,
    ) -> None:
        """Generate an 8D report template as JSON when a Q-Meldung is created."""
        qm_number = record.get("notification_id", "UNKNOWN")
        report_dir = Path(__file__).resolve().parent.parent / "reports"
        report_dir.mkdir(parents=True, exist_ok=True)

        report = {
            "qm_number": qm_number,
            "dtc_code": dtc_code,
            "defect_code": defect.get("code", ""),
            "defect_text": defect.get("text", ""),
            "plant": plant or "N/A",
            "created_at": datetime.now(timezone.utc).isoformat(),
            "d1_team": "Taktflow Embedded — Functional Safety",
            "d2_problem_description": (
                f"DTC {dtc_code} detected: {defect.get('text', 'Unknown defect')}"
            ),
            "d3_containment_actions": [],
            "d4_root_cause_analysis": "",
            "d5_corrective_actions": [],
            "d6_verification": "",
            "d7_preventive_actions": [],
            "d8_closure": {"closed": False, "date": None},
        }

        report_path = report_dir / f"8D-{qm_number}.json"
        report_path.write_text(
            json.dumps(report, indent=2, ensure_ascii=False),
            encoding="utf-8",
        )
        logger.info("8D report generated: %s", report_path)

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    async def start(self) -> None:
        """Start the MQTT client with synchronous connect + retry.

        Uses blocking connect (not connect_async) to guarantee the
        subscription is active before any DTC alerts arrive.  This
        prevents the race where connect_async returns before the broker
        handshake completes, causing early messages to be lost.
        """
        self._loop = asyncio.get_running_loop()
        self._client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id="sap-qm-dtc-connector",
        )
        self._client.on_connect = self._on_connect  # type: ignore[assignment]
        self._client.on_message = self._on_message  # type: ignore[assignment]
        self._client.on_disconnect = self._on_disconnect  # type: ignore[assignment]

        for attempt in range(15):
            try:
                self._client.connect(MQTT_HOST, MQTT_PORT, keepalive=30)
                break
            except (ConnectionRefusedError, OSError) as exc:
                logger.warning(
                    "MQTT connect attempt %d failed: %s", attempt + 1, exc
                )
                await asyncio.sleep(1)
        else:
            logger.error("MQTT connection failed after 15 attempts")

        self._client.loop_start()
        self._client.subscribe(SUBSCRIBE_TOPIC, qos=1)
        self._running = True
        logger.info(
            "DTC connector started — subscribed to %s on %s:%d",
            SUBSCRIBE_TOPIC, MQTT_HOST, MQTT_PORT,
        )

    async def stop(self) -> None:
        """Stop the MQTT client gracefully."""
        if self._client is not None and self._running:
            self._client.loop_stop()
            self._client.disconnect()
            self._running = False
            logger.info("DTC connector stopped")
