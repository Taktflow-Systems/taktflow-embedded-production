#!/usr/bin/env python3
"""Cloud connector — bridges local MQTT to AWS IoT Core.

Subscribes to vehicle telemetry, DTC, and alert topics on the local
Mosquitto broker and republishes them to AWS IoT Core over TLS+X.509.
Buffers up to 100 messages during cloud disconnects.
"""

from __future__ import annotations

import asyncio
import logging
import time

from cloud_connector.bridge import CloudBridge
from cloud_connector.health import (
    HEALTH_INTERVAL_S,
    HEALTH_TOPIC,
    build_health_payload,
)

logger = logging.getLogger(__name__)


async def health_loop(bridge: CloudBridge, start_time: float) -> None:
    """Publish health status to local MQTT every 30 seconds."""
    while True:
        await asyncio.sleep(HEALTH_INTERVAL_S)
        uptime = time.monotonic() - start_time
        payload = build_health_payload(
            aws_connected=bridge.aws_connected,
            msgs_sent=bridge.msgs_sent,
            msgs_buffered=bridge.msgs_buffered,
            uptime_s=uptime,
        )
        bridge._local_client.publish(HEALTH_TOPIC, payload, qos=0)
        logger.debug("Health: connected=%s, sent=%d, buffered=%d",
                      bridge.aws_connected, bridge.msgs_sent, bridge.msgs_buffered)


async def main() -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s — %(message)s",
    )

    bridge = CloudBridge()
    bridge.start()
    start_time = time.monotonic()

    logger.info("Cloud connector running")

    try:
        await health_loop(bridge, start_time)
    except asyncio.CancelledError:
        logger.info("Shutting down …")
    finally:
        bridge.stop()
        logger.info("Cloud connector stopped")


if __name__ == "__main__":
    asyncio.run(main())
