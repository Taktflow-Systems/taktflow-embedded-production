"""Normal Drive holds its pedal until reset or another scenario.

Run from the repository root:
    python -m pytest gateway/fault_inject/tests -v
"""

import os
import sys
import time as real_time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from fastapi.testclient import TestClient  # noqa: E402

from fault_inject import app as app_module  # noqa: E402
from fault_inject import pedal_udp  # noqa: E402


class _StopLoop(BaseException):
    """Escapes the idle loop's broad except clause."""


class _FakeTime:
    def __init__(self, ticks: int):
        self.ticks = ticks

    def time(self) -> float:
        return real_time.time()

    def sleep(self, _sec: float) -> None:
        self.ticks -= 1
        if self.ticks <= 0:
            raise _StopLoop


@pytest.fixture(autouse=True)
def isolated_state(monkeypatch):
    monkeypatch.setattr(app_module, "_drive_hold_pct", None)
    monkeypatch.setattr(app_module, "_idle_paused", False)
    monkeypatch.setattr(app_module, "_idle_paused_until", 0.0)
    monkeypatch.setattr(app_module, "FAULT_API_KEY", "")
    monkeypatch.setattr(app_module, "_test_runner", None)
    monkeypatch.setattr(app_module, "_mqtt_client", None)
    monkeypatch.setitem(app_module._control_lock, "client_id", None)
    monkeypatch.setitem(app_module._control_lock, "expires_at", 0.0)
    monkeypatch.setattr(
        app_module,
        "SCENARIOS",
        {
            "normal_drive": {"fn": lambda: "drive ok", "description": "d"},
            "overcurrent": {"fn": lambda: "oc ok", "description": "o"},
        },
    )
    monkeypatch.setattr(app_module, "reset_scenario", lambda: "reset ok")


@pytest.fixture
def client():
    return TestClient(app_module.app)


@pytest.fixture
def pedal(monkeypatch):
    sent = {"pct": [], "neutral": 0, "clear": 0}
    monkeypatch.setattr(pedal_udp, "pedal_pct_to_angle", lambda pct: pct)
    monkeypatch.setattr(pedal_udp, "send_pedal_override", lambda a: sent["pct"].append(a))
    monkeypatch.setattr(pedal_udp, "send_pedal_neutral",
                        lambda: sent.__setitem__("neutral", sent["neutral"] + 1))
    monkeypatch.setattr(pedal_udp, "clear_pedal_override",
                        lambda: sent.__setitem__("clear", sent["clear"] + 1))
    return sent


def _run_idle_loop(monkeypatch, ticks: int) -> None:
    monkeypatch.setattr(app_module, "time", _FakeTime(ticks))
    with pytest.raises(_StopLoop):
        app_module._idle_command_loop()


def test_normal_drive_sets_hold_without_post_pause(client):
    r = client.post("/api/fault/scenario/normal_drive")
    assert r.status_code == 200
    assert app_module._drive_hold_pct == 50.0
    assert app_module._idle_paused_until == 0.0


def test_other_scenario_clears_hold_and_pauses(client):
    client.post("/api/fault/scenario/normal_drive")
    client.post("/api/fault/scenario/overcurrent")
    assert app_module._drive_hold_pct is None
    assert app_module._idle_paused_until > real_time.time()


def test_reset_clears_hold(client):
    client.post("/api/fault/scenario/normal_drive")
    assert client.post("/api/fault/reset").status_code == 200
    assert app_module._drive_hold_pct is None


def test_failed_normal_drive_sets_no_hold(client, monkeypatch):
    def boom():
        raise RuntimeError("no bus")

    app_module.SCENARIOS["normal_drive"] = {"fn": boom, "description": "d"}
    assert client.post("/api/fault/scenario/normal_drive").status_code == 500
    assert app_module._drive_hold_pct is None


def test_runner_path_holds_and_runner_reset_releases():
    app_module._trigger_scenario("normal_drive")
    assert app_module._drive_hold_pct == 50.0
    assert app_module._reset_and_release_hold() == "reset ok"
    assert app_module._drive_hold_pct is None


def test_idle_loop_sends_held_pedal_even_under_control_lock(monkeypatch, pedal):
    monkeypatch.setattr(app_module, "_drive_hold_pct", 50.0)
    monkeypatch.setitem(app_module._control_lock, "client_id", "visitor")
    monkeypatch.setitem(app_module._control_lock, "expires_at", real_time.time() + 60)
    _run_idle_loop(monkeypatch, ticks=5)
    assert pedal["pct"] and set(pedal["pct"]) == {50.0}
    assert pedal["neutral"] == 0


def test_idle_loop_without_hold_stays_neutral(monkeypatch, pedal):
    _run_idle_loop(monkeypatch, ticks=5)
    assert pedal["pct"] == []
    assert pedal["neutral"] > 0


def test_idle_loop_pause_beats_hold(monkeypatch, pedal):
    monkeypatch.setattr(app_module, "_drive_hold_pct", 50.0)
    monkeypatch.setattr(app_module, "_idle_paused", True)
    _run_idle_loop(monkeypatch, ticks=5)
    assert pedal["pct"] == [] and pedal["neutral"] == 0
