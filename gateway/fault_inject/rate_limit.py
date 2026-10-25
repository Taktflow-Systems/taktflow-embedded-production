"""In-memory sliding-window rate limiter and client identity for the fault API.

Single process only: state lives in this process and is lost on restart.
The clock is injectable so tests can move time without sleeping.
"""

import ipaddress
import threading
import time
from collections import deque
from typing import Callable, Iterable, NamedTuple, Optional

# Peers whose X-Forwarded-For header is believed (host Caddy on loopback).
TRUSTED_PROXY_PEERS = frozenset({"127.0.0.1"})

# Clients are grouped by /64 for IPv6 so rotating addresses in one prefix share a bucket.
IPV6_PREFIX_BITS = 64

_SWEEP_INTERVAL_SEC = 60.0


class Rule(NamedTuple):
    """At most `limit` hits per `window_sec` for one `client` within one `bucket`."""

    bucket: str
    client: str
    limit: int
    window_sec: float


def client_key(peer: Optional[str], forwarded_for: Optional[str]) -> str:
    """Identify the client behind a request.

    X-Forwarded-For counts only when the TCP peer is a trusted proxy; its
    first address is used, and only if it parses as an IP address.
    """
    peer = peer or "unknown"
    if peer in TRUSTED_PROXY_PEERS and forwarded_for:
        first = forwarded_for.split(",")[0].strip()
        try:
            addr = ipaddress.ip_address(first)
        except ValueError:
            return peer
        if isinstance(addr, ipaddress.IPv6Address):
            net = ipaddress.ip_network((addr, IPV6_PREFIX_BITS), strict=False)
            return str(net)
        return str(addr)
    return peer


class SlidingWindowLimiter:
    """Sliding-window counters keyed by (bucket, client)."""

    def __init__(self, clock: Callable[[], float] = time.monotonic) -> None:
        self._clock = clock
        self._lock = threading.Lock()
        self._hits: dict[tuple[str, str], tuple[deque, float]] = {}
        self._last_sweep = clock()

    def _expire(self, hits: deque, window_sec: float, now: float) -> None:
        while hits and hits[0] <= now - window_sec:
            hits.popleft()

    def _sweep(self, now: float) -> None:
        if now - self._last_sweep < _SWEEP_INTERVAL_SEC:
            return
        self._last_sweep = now
        for key in list(self._hits):
            hits, window_sec = self._hits[key]
            self._expire(hits, window_sec, now)
            if not hits:
                del self._hits[key]

    def _wait_locked(self, rules: Iterable[Rule], now: float) -> float:
        wait = 0.0
        for rule in rules:
            entry = self._hits.get((rule.bucket, rule.client))
            if entry is None:
                continue
            hits = entry[0]
            self._expire(hits, rule.window_sec, now)
            if len(hits) >= rule.limit:
                # The hit that frees a slot is the one `limit` places from the newest.
                free_at = hits[len(hits) - rule.limit] + rule.window_sec
                wait = max(wait, free_at - now)
        return wait

    def _record_locked(self, rules: Iterable[Rule], now: float) -> None:
        for rule in rules:
            key = (rule.bucket, rule.client)
            entry = self._hits.get(key)
            if entry is None:
                entry = (deque(), rule.window_sec)
                self._hits[key] = entry
            entry[0].append(now)

    def retry_after(self, rules: Iterable[Rule]) -> float:
        """Seconds until every rule admits a hit (0.0 = admitted now). Records nothing."""
        rules = list(rules)
        with self._lock:
            now = self._clock()
            self._sweep(now)
            return self._wait_locked(rules, now)

    def record(self, rules: Iterable[Rule]) -> None:
        """Count one hit against every rule."""
        rules = list(rules)
        with self._lock:
            self._record_locked(rules, self._clock())

    def try_acquire(self, rules: Iterable[Rule]) -> float:
        """Atomically admit and count one hit, or refuse and count nothing.

        Returns 0.0 when admitted, else the seconds to wait.
        """
        rules = list(rules)
        with self._lock:
            now = self._clock()
            self._sweep(now)
            wait = self._wait_locked(rules, now)
            if wait <= 0.0:
                self._record_locked(rules, now)
                return 0.0
            return wait
