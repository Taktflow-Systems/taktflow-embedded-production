"""Tests for the CloudBridge MQTT forwarder."""

import json
import time
from unittest.mock import MagicMock, patch

import pytest

from cloud_connector.bridge import CloudBridge, FORWARDED_TOPICS


class TestForwardedTopics:
    """Verify only the budget-safe topics are forwarded."""

    def test_vehicle_telemetry_is_forwarded(self):
        assert "vehicle/telemetry" in FORWARDED_TOPICS

    def test_vehicle_dtc_new_is_forwarded(self):
        assert "vehicle/dtc/new" in FORWARDED_TOPICS

    def test_vehicle_alerts_is_forwarded(self):
        assert "vehicle/alerts" in FORWARDED_TOPICS

    def test_individual_signals_not_forwarded(self):
        assert "taktflow/can/Motor_Current/Current_mA" not in FORWARDED_TOPICS

    def test_anomaly_score_not_forwarded(self):
        assert "taktflow/anomaly/score" not in FORWARDED_TOPICS


class TestCloudBridge:
    """Unit tests for the CloudBridge message forwarding logic."""

    def _make_bridge(self, aws_connected=True):
        """Create a bridge with mocked MQTT clients."""
        bridge = CloudBridge.__new__(CloudBridge)
        bridge._local_client = MagicMock()
        bridge._aws_client = MagicMock()
        bridge._aws_connected = aws_connected
        bridge._msgs_sent = 0
        bridge._buffer = MagicMock()
        bridge._buffer.pending = 0
        bridge._buffer.drain.return_value = []
        return bridge

    def test_forward_message_when_connected(self):
        bridge = self._make_bridge(aws_connected=True)
        bridge._on_local_message(
            bridge._local_client,
            None,
            MagicMock(topic="vehicle/telemetry", payload=b'{"rpm": 1500}'),
        )
        bridge._aws_client.publish.assert_called_once_with(
            "vehicle/telemetry", b'{"rpm": 1500}', qos=1
        )

    def test_buffer_message_when_disconnected(self):
        bridge = self._make_bridge(aws_connected=False)
        bridge._on_local_message(
            bridge._local_client,
            None,
            MagicMock(topic="vehicle/telemetry", payload=b'{"rpm": 1500}'),
        )
        bridge._buffer.enqueue.assert_called_once_with(
            "vehicle/telemetry", b'{"rpm": 1500}'
        )
        bridge._aws_client.publish.assert_not_called()

    def test_drain_buffer_on_reconnect(self):
        bridge = self._make_bridge(aws_connected=False)
        bridge._buffer.drain.return_value = [
            ("vehicle/telemetry", b"msg1"),
            ("vehicle/dtc/new", b"msg2"),
        ]
        bridge._buffer.pending = 2

        # Simulate AWS reconnect
        bridge._on_aws_connect(
            bridge._aws_client, None, None, 0, None
        )

        assert bridge._aws_connected is True
        assert bridge._aws_client.publish.call_count == 2

    def test_increment_msgs_sent_counter(self):
        bridge = self._make_bridge(aws_connected=True)
        bridge._on_local_message(
            bridge._local_client,
            None,
            MagicMock(topic="vehicle/telemetry", payload=b"msg"),
        )
        assert bridge._msgs_sent == 1
