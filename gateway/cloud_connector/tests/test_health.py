"""Tests for the health status publisher."""

import json
from unittest.mock import MagicMock

import pytest

from cloud_connector.health import build_health_payload, HEALTH_TOPIC


class TestHealthPayload:
    """Verify health status JSON structure."""

    def test_payload_has_required_fields(self):
        payload = build_health_payload(
            aws_connected=True,
            msgs_sent=42,
            msgs_buffered=3,
            uptime_s=123.4,
        )
        data = json.loads(payload)
        assert data["connected"] is True
        assert data["msgs_sent"] == 42
        assert data["msgs_buffered"] == 3
        assert data["uptime_s"] == 123.4
        assert "ts" in data

    def test_payload_disconnected_state(self):
        payload = build_health_payload(
            aws_connected=False,
            msgs_sent=0,
            msgs_buffered=100,
            uptime_s=0.0,
        )
        data = json.loads(payload)
        assert data["connected"] is False
        assert data["msgs_buffered"] == 100

    def test_health_topic(self):
        assert HEALTH_TOPIC == "taktflow/cloud/status"
