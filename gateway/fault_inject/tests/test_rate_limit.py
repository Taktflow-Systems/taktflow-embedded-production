"""Rate limits and key handling of the fault injection API (RF-P1-11, D-12 amended).

Run from the repository root:
    python -m pytest gateway/fault_inject/tests -v
Needs the packages of gateway/fault_inject/requirements.txt and requirements-test.txt.
Test client addresses are built from integers, never written as literals.
"""

import ipaddress
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from fastapi.testclient import TestClient  # noqa: E402

from fault_inject import app as app_module  # noqa: E402
from fault_inject.rate_limit import Rule, SlidingWindowLimiter, client_key  # noqa: E402

LOOPBACK = "127.0.0.1"
FAKE_KEY = "test-operator-key"

# Every endpoint the public dashboard calls (TelemetryDashboard.tsx, TestRunner.tsx).
# (method, path, JSON body or None, sends X-Client-Id)
WEBSITE_CALLS = [
    ("POST", "/api/fault/control/acquire", {"client_id": "visitor"}, False),
    ("GET", "/api/fault/control/status", None, False),
    ("POST", "/api/fault/scenario/normal_drive", None, True),
    ("POST", "/api/fault/reset", None, True),
    ("POST", "/api/fault/control/release", {"client_id": "visitor"}, False),
    ("POST", "/api/fault/control/acquire", {"client_id": "visitor"}, False),
    ("GET", "/api/test/specs", None, False),
    ("POST", "/api/test/run", {"tests": None}, True),
    ("POST", "/api/test/stop", None, True),
]


def _v4(n: int) -> str:
    return str(ipaddress.IPv4Address(0xC6336400 + n))


def _v6(prefix: int, host: int) -> str:
    return str(ipaddress.IPv6Address((0x20010DB8 << 96) | (prefix << 64) | host))


class FakeClock:
    def __init__(self) -> None:
        self.now = 1000.0

    def __call__(self) -> float:
        return self.now

    def advance(self, seconds: float) -> None:
        self.now += seconds


class FakeRunner:
    """Stands in for DashboardTestRunner; start() can be told to refuse."""

    status = "idle"
    last_result = None
    _run_id = ""

    def __init__(self) -> None:
        self.starts = 0
        self.busy = False

    def start(self, tests):
        if self.busy:
            raise RuntimeError("already running")
        self.starts += 1
        return f"run-{self.starts}"

    def stop(self) -> bool:
        return True


@pytest.fixture
def clock():
    return FakeClock()


@pytest.fixture
def runner(monkeypatch):
    fake = FakeRunner()
    monkeypatch.setattr(app_module, "_test_runner", fake)
    return fake


@pytest.fixture(autouse=True)
def isolated_state(monkeypatch, clock):
    monkeypatch.setattr(app_module, "_limiter", SlidingWindowLimiter(clock=clock))
    monkeypatch.setattr(app_module, "FAULT_API_KEY", "")
    monkeypatch.setattr(app_module, "_test_runner", None)
    monkeypatch.setattr(app_module, "_mqtt_client", None)
    monkeypatch.setitem(app_module._control_lock, "client_id", None)
    monkeypatch.setitem(app_module._control_lock, "expires_at", 0.0)
    monkeypatch.setattr(
        app_module,
        "SCENARIOS",
        {"normal_drive": {"fn": lambda: "ok", "description": "test scenario"}},
    )
    monkeypatch.setattr(app_module, "reset_scenario", lambda: "reset ok")


def client(peer: str) -> TestClient:
    return TestClient(app_module.app, client=(peer, 50000))


def fault_post(c: TestClient, headers=None):
    # An unknown scenario answers 404 after the limit check, so no CAN or Docker is touched.
    return c.post("/api/fault/scenario/does-not-exist", headers=headers or {})


# ---------------------------------------------------------------------------
# /api/fault/*: 30 per minute per client
# ---------------------------------------------------------------------------

def test_31st_fault_request_in_a_minute_is_429(clock):
    c = client("peer-a")
    for i in range(30):
        assert fault_post(c).status_code == 404, f"request {i + 1}"
    resp = fault_post(c)
    assert resp.status_code == 429
    assert int(resp.headers["Retry-After"]) >= 1


def test_other_client_is_not_affected(clock):
    a, b = client("peer-a"), client("peer-b")
    for _ in range(30):
        fault_post(a)
    assert fault_post(a).status_code == 429
    assert fault_post(b).status_code == 404


def test_window_expiry_with_fake_clock(clock):
    c = client("peer-a")
    for _ in range(30):
        fault_post(c)
    assert fault_post(c).status_code == 429
    clock.advance(59.0)
    assert fault_post(c).status_code == 429
    clock.advance(1.0)
    assert fault_post(c).status_code == 404


def test_window_slides_instead_of_resetting(clock):
    c = client("peer-a")
    for _ in range(15):
        fault_post(c)
    clock.advance(30.0)
    for _ in range(15):
        fault_post(c)
    assert fault_post(c).status_code == 429
    clock.advance(30.0)  # the first 15 leave the window, the second 15 stay
    for _ in range(15):
        assert fault_post(c).status_code == 404
    assert fault_post(c).status_code == 429


def test_retry_after_matches_the_oldest_hit(clock):
    c = client("peer-a")
    fault_post(c)
    clock.advance(10.0)
    for _ in range(29):
        fault_post(c)
    resp = fault_post(c)
    assert resp.status_code == 429
    assert resp.headers["Retry-After"] == "50"


def test_stop_shares_the_fault_limit(clock, runner):
    c = client("peer-a")
    for _ in range(30):
        assert c.post("/api/test/stop").status_code == 200
    assert c.post("/api/test/stop").status_code == 429
    assert fault_post(c).status_code == 429


def test_read_endpoints_are_never_limited(clock):
    c = client("peer-a")
    for _ in range(100):
        assert c.get("/api/fault/control/status").status_code == 200
        assert c.get("/api/fault/scenarios").status_code == 200
        assert c.get("/api/fault/health").status_code == 200
        assert c.get("/api/test/specs").status_code == 200
        assert c.get("/api/test/status").status_code == 200
        assert c.get("/api/test/result").status_code == 200


# ---------------------------------------------------------------------------
# Client identity
# ---------------------------------------------------------------------------

def test_forwarded_for_is_ignored_from_a_non_loopback_peer(clock):
    c = client("peer-a")
    for i in range(30):
        fault_post(c, {"X-Forwarded-For": _v4(i)})
    assert fault_post(c, {"X-Forwarded-For": _v4(200)}).status_code == 429


def test_forwarded_for_is_used_from_the_loopback_proxy(clock):
    proxy = client(LOOPBACK)
    first, second = _v4(1), _v4(2)
    for _ in range(30):
        assert fault_post(proxy, {"X-Forwarded-For": first}).status_code == 404
    assert fault_post(proxy, {"X-Forwarded-For": first}).status_code == 429
    assert fault_post(proxy, {"X-Forwarded-For": second}).status_code == 404


def test_only_the_first_forwarded_address_counts(clock):
    proxy = client(LOOPBACK)
    first = _v4(1)
    for i in range(30):
        fault_post(proxy, {"X-Forwarded-For": f"{first}, {_v4(100 + i)}"})
    assert fault_post(proxy, {"X-Forwarded-For": f"{first}, {_v4(250)}"}).status_code == 429


def test_client_key_rules():
    assert client_key("peer-a", _v4(1)) == "peer-a"
    assert client_key(LOOPBACK, None) == LOOPBACK
    assert client_key(LOOPBACK, "") == LOOPBACK
    assert client_key(LOOPBACK, _v4(1)) == _v4(1)
    assert client_key(LOOPBACK, f" {_v4(1)} , {_v4(2)}") == _v4(1)
    assert client_key(LOOPBACK, "not-an-address") == LOOPBACK
    assert client_key(None, None) == "unknown"


def test_ipv6_clients_are_grouped_by_64_bit_prefix():
    one = client_key(LOOPBACK, _v6(7, 1))
    two = client_key(LOOPBACK, _v6(7, 2))
    other = client_key(LOOPBACK, _v6(8, 1))
    assert one == two
    assert one != other


# ---------------------------------------------------------------------------
# /api/test/run: 3 per 10 minutes per client, 20 per hour in total
# ---------------------------------------------------------------------------

def run_post(c: TestClient):
    return c.post("/api/test/run", json={"tests": None}, headers={"X-Client-Id": "visitor"})


def test_run_per_client_limit(clock, runner):
    c = client("peer-a")
    for _ in range(3):
        assert run_post(c).status_code == 200
    resp = run_post(c)
    assert resp.status_code == 429
    assert int(resp.headers["Retry-After"]) >= 1
    assert runner.starts == 3
    assert run_post(client("peer-b")).status_code == 200


def test_run_per_client_window_expires(clock, runner):
    c = client("peer-a")
    for _ in range(3):
        run_post(c)
    assert run_post(c).status_code == 429
    clock.advance(599.0)
    assert run_post(c).status_code == 429
    clock.advance(1.0)
    assert run_post(c).status_code == 200


def test_run_global_hourly_limit(clock, runner):
    # Seven clients with three runs each: the 21st run is refused by the global limit.
    results = []
    for n in range(7):
        c = client(f"peer-{n}")
        for _ in range(3):
            results.append(run_post(c).status_code)
    assert results[:20] == [200] * 20
    assert results[20] == 429
    assert run_post(client("peer-new")).status_code == 429
    clock.advance(3600.0)
    assert run_post(client("peer-new")).status_code == 200


def test_refused_or_rejected_runs_do_not_count(clock, runner):
    c = client("peer-a")
    runner.busy = True
    for _ in range(5):
        assert run_post(c).status_code == 409
    runner.busy = False
    for _ in range(3):
        assert run_post(c).status_code == 200
    assert run_post(c).status_code == 429


def test_run_still_needs_the_controller_lock(clock, runner):
    holder = client("peer-a")
    assert holder.post("/api/fault/control/acquire", json={"client_id": "owner"}).status_code == 200
    intruder = client("peer-b")
    resp = intruder.post("/api/test/run", json={"tests": None}, headers={"X-Client-Id": "visitor"})
    assert resp.status_code == 403
    ok = holder.post("/api/test/run", json={"tests": None}, headers={"X-Client-Id": "owner"})
    assert ok.status_code == 200


# ---------------------------------------------------------------------------
# Operator key bypass
# ---------------------------------------------------------------------------

def test_valid_key_bypasses_the_fault_limit(clock, monkeypatch):
    monkeypatch.setattr(app_module, "FAULT_API_KEY", FAKE_KEY)
    c = client("peer-a")
    for _ in range(100):
        assert fault_post(c, {"X-Api-Key": FAKE_KEY}).status_code == 404


def test_valid_key_bypasses_the_run_limits(clock, runner, monkeypatch):
    monkeypatch.setattr(app_module, "FAULT_API_KEY", FAKE_KEY)
    c = client("peer-a")
    for _ in range(25):
        resp = c.post(
            "/api/test/run",
            json={"tests": None},
            headers={"X-Api-Key": FAKE_KEY, "X-Client-Id": "operator"},
        )
        assert resp.status_code == 200


def test_wrong_or_missing_key_is_limited_not_rejected(clock, monkeypatch):
    monkeypatch.setattr(app_module, "FAULT_API_KEY", FAKE_KEY)
    c = client("peer-a")
    for _ in range(30):
        assert fault_post(c, {"X-Api-Key": "wrong"}).status_code == 404
    assert fault_post(c, {"X-Api-Key": "wrong"}).status_code == 429
    assert fault_post(c).status_code == 429


def test_empty_header_does_not_match_when_no_key_is_configured(clock):
    c = client("peer-a")
    for _ in range(30):
        fault_post(c, {"X-Api-Key": ""})
    assert fault_post(c, {"X-Api-Key": ""}).status_code == 429


def test_key_never_changes_the_lock_rules(clock, runner, monkeypatch):
    monkeypatch.setattr(app_module, "FAULT_API_KEY", FAKE_KEY)
    holder = client("peer-a")
    holder.post("/api/fault/control/acquire", json={"client_id": "owner"})
    resp = client("peer-b").post(
        "/api/test/run", json={"tests": None}, headers={"X-Api-Key": FAKE_KEY, "X-Client-Id": "other"}
    )
    assert resp.status_code == 403


# ---------------------------------------------------------------------------
# Website endpoints work without a key while FAULT_API_KEY is set
# ---------------------------------------------------------------------------

def test_every_website_endpoint_works_without_a_key(clock, runner, monkeypatch):
    monkeypatch.setattr(app_module, "FAULT_API_KEY", FAKE_KEY)
    c = client(LOOPBACK)
    acquired = c.post(
        "/api/fault/control/acquire",
        json={"client_id": "visitor"},
        headers={"X-Forwarded-For": _v4(7)},
    )
    assert acquired.status_code == 200
    # The visitor now holds the controller lock; the remaining calls come from the same visitor.
    for method, path, body, with_client in WEBSITE_CALLS[1:]:
        headers = {"X-Forwarded-For": _v4(7)}
        if with_client:
            headers["X-Client-Id"] = "visitor"
        resp = c.request(method, path, json=body, headers=headers)
        assert "X-Api-Key" not in headers
        assert resp.status_code == 200, f"{method} {path} -> {resp.status_code} {resp.text}"


def test_limiter_unit_rules_are_independent(clock):
    limiter = SlidingWindowLimiter(clock=clock)
    rule_a = Rule("b", "a", 2, 10.0)
    rule_b = Rule("b", "b", 2, 10.0)
    assert limiter.try_acquire([rule_a]) == 0.0
    assert limiter.try_acquire([rule_a]) == 0.0
    assert limiter.try_acquire([rule_a]) == pytest.approx(10.0)
    assert limiter.try_acquire([rule_b]) == 0.0
    # A refused combined request records nothing for the rule that would have admitted it.
    assert limiter.try_acquire([rule_b, rule_a]) > 0.0
    assert limiter.try_acquire([rule_b]) == 0.0
    assert limiter.try_acquire([rule_b]) > 0.0


def test_limiter_forgets_idle_clients(clock):
    limiter = SlidingWindowLimiter(clock=clock)
    for n in range(50):
        limiter.try_acquire([Rule("b", f"c{n}", 1, 5.0)])
    clock.advance(120.0)
    limiter.retry_after([Rule("b", "x", 1, 5.0)])
    assert limiter._hits == {}
