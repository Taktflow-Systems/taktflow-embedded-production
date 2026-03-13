"""Tests for the offline message buffer."""

import pytest

from cloud_connector.buffer import MessageBuffer


class TestMessageBuffer:
    """Unit tests for MessageBuffer (deque-backed offline queue)."""

    def test_enqueue_stores_message(self):
        buf = MessageBuffer(maxlen=10)
        buf.enqueue("vehicle/telemetry", b'{"rpm": 1500}')
        assert buf.pending == 1

    def test_drain_returns_all_messages_in_order(self):
        buf = MessageBuffer(maxlen=10)
        buf.enqueue("vehicle/telemetry", b"msg1")
        buf.enqueue("vehicle/dtc/new", b"msg2")
        buf.enqueue("vehicle/alerts", b"msg3")

        messages = buf.drain()
        assert len(messages) == 3
        assert messages[0] == ("vehicle/telemetry", b"msg1")
        assert messages[1] == ("vehicle/dtc/new", b"msg2")
        assert messages[2] == ("vehicle/alerts", b"msg3")

    def test_drain_clears_buffer(self):
        buf = MessageBuffer(maxlen=10)
        buf.enqueue("vehicle/telemetry", b"msg1")
        buf.drain()
        assert buf.pending == 0

    def test_maxlen_evicts_oldest(self):
        buf = MessageBuffer(maxlen=3)
        buf.enqueue("t", b"msg1")
        buf.enqueue("t", b"msg2")
        buf.enqueue("t", b"msg3")
        buf.enqueue("t", b"msg4")  # evicts msg1

        messages = buf.drain()
        assert len(messages) == 3
        assert messages[0] == ("t", b"msg2")

    def test_default_maxlen_is_100(self):
        buf = MessageBuffer()
        assert buf._queue.maxlen == 100

    def test_overflow_retains_newest(self):
        buf = MessageBuffer(maxlen=5)
        for i in range(10):
            buf.enqueue("t", f"msg{i}".encode())

        messages = buf.drain()
        assert len(messages) == 5
        # Should have msg5 through msg9 (newest 5)
        assert messages[0] == ("t", b"msg5")
        assert messages[4] == ("t", b"msg9")

    def test_empty_drain_returns_empty_list(self):
        buf = MessageBuffer(maxlen=10)
        assert buf.drain() == []
        assert buf.pending == 0
