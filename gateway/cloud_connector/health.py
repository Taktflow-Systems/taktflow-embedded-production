"""Health status publisher for cloud connector.

Publishes connection status to local MQTT every 30 seconds
so the dashboard can show cloud connection state.
"""

from __future__ import annotations

import json
import time

HEALTH_TOPIC = "taktflow/cloud/status"
HEALTH_INTERVAL_S = 30.0


def build_health_payload(
    aws_connected: bool,
    msgs_sent: int,
    msgs_buffered: int,
    uptime_s: float,
) -> str:
    """Build JSON health status payload."""
    return json.dumps({
        "connected": aws_connected,
        "msgs_sent": msgs_sent,
        "msgs_buffered": msgs_buffered,
        "uptime_s": round(uptime_s, 1),
        "ts": time.time(),
    })
