"""Offline message buffer for cloud connector.

Queues messages when AWS IoT Core is unreachable.
Drains on reconnect (FIFO order, oldest first).
Bounded to 100 messages (SWR-GW-001) to cap memory on Pi.
"""

from __future__ import annotations

from collections import deque


class MessageBuffer:
    """Thread-safe(ish) bounded FIFO buffer for MQTT messages."""

    def __init__(self, maxlen: int = 100) -> None:
        self._queue: deque[tuple[str, bytes]] = deque(maxlen=maxlen)

    @property
    def pending(self) -> int:
        return len(self._queue)

    def enqueue(self, topic: str, payload: bytes) -> None:
        """Add a message. Oldest evicted if buffer full."""
        self._queue.append((topic, payload))

    def drain(self) -> list[tuple[str, bytes]]:
        """Return all buffered messages and clear the queue."""
        messages = list(self._queue)
        self._queue.clear()
        return messages
