"""Taktflow Fault Injection API — FastAPI server for triggering CAN
fault scenarios during demo.

Endpoints:
    POST /api/fault/scenario/{name}     — trigger a scenario by name
    POST /api/fault/reset               — reset all actuators to safe idle
    GET  /api/fault/scenarios           — list available scenarios
    GET  /api/fault/health             — health check
    POST /api/fault/control/acquire    — acquire 5-min controller lock
    POST /api/fault/control/release    — release controller lock early
    GET  /api/fault/control/status     — current lock state

Runs on FAULT_PORT (default 8091).
"""

import json
import logging
import os
import threading
import time

import paho.mqtt.client as paho_mqtt
import uvicorn
from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from .scenarios import (
    SCENARIOS, reset as reset_scenario, set_mqtt_client,
    _get_bus, _send, _brake_frame, _steer_frame,
    CAN_BRAKE_COMMAND, CAN_STEER_COMMAND,
)
from .test_runner import DashboardTestRunner

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [FAULT] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("fault_inject")

# API key for mutating endpoints (empty = no auth, dev mode)
FAULT_API_KEY = os.environ.get("FAULT_API_KEY", "")

# MQTT client for publishing reset/command messages
_mqtt_client: paho_mqtt.Client | None = None

# Idle command TX — paused during active fault scenarios
_idle_paused = False
IDLE_CMD_INTERVAL = float(os.environ.get("IDLE_CMD_INTERVAL", "0.05"))  # 50ms

# ---------------------------------------------------------------------------
# Controller lock — single in-memory lock for fault-inject control
# ---------------------------------------------------------------------------
LOCK_DURATION_SEC = int(os.environ.get("LOCK_DURATION_SEC", "900"))  # 15 min default

_control_lock = {
    "client_id": None,      # str | None — who holds it
    "expires_at": 0.0,      # unix timestamp
    "acquired_at": 0.0,     # when lock was taken
}
_lock_mu = threading.Lock()


class ClientIdBody(BaseModel):
    client_id: str


class TestRunBody(BaseModel):
    tests: list[str] | None = None


# Test runner instance (initialized on startup)
_test_runner: DashboardTestRunner | None = None


def _publish_lock_state() -> None:
    """Publish current lock state to MQTT with retain."""
    if _mqtt_client is None:
        return
    now = time.time()
    with _lock_mu:
        locked = (
            _control_lock["client_id"] is not None
            and now < _control_lock["expires_at"]
        )
        payload = {
            "locked": locked,
            "client_id": _control_lock["client_id"] or "",
            "remaining_sec": max(0, int(_control_lock["expires_at"] - now)) if locked else 0,
            "acquired_at": _control_lock["acquired_at"] if locked else 0.0,
        }
    _mqtt_client.publish(
        "taktflow/control/lock",
        json.dumps(payload),
        qos=0,
        retain=True,
    )


def _lock_watchdog() -> None:
    """Background thread: auto-expire lock and publish updated remaining_sec."""
    was_locked = False
    while True:
        time.sleep(1)
        with _lock_mu:
            is_locked = _control_lock["client_id"] is not None
            if is_locked and time.time() >= _control_lock["expires_at"]:
                log.info("Control lock expired for %s", _control_lock["client_id"])
                _control_lock["client_id"] = None
                _control_lock["expires_at"] = 0.0
                _control_lock["acquired_at"] = 0.0
                is_locked = False
        # Publish while locked, and one final time after expiry to clear retained msg
        if is_locked or was_locked:
            _publish_lock_state()
        was_locked = is_locked


def _idle_command_loop() -> None:
    """Background thread: keep CAN bus handle alive and drive an idle
    cruise profile so the demo shows the vehicle moving.

    Profile (paused while a scenario is active or when the control
    lock is held by a remote client):
      phase A (2.0s): ramp pedal 0% -> IDLE_CRUISE_PCT
      phase B (6.0s): hold at IDLE_CRUISE_PCT  (~15-20 km/h)
      phase C (2.0s): ramp back to 0%
      phase D (2.0s): idle — vehicle coasts to stop
      repeat

    Driving both sensors through the SPI UDP override also stabilises
    the otherwise jittering PedalPosition1 that was tripping CVC's
    plausibility check every few seconds in idle.
    """
    from .pedal_udp import (
        send_pedal_override,
        clear_pedal_override,
        pedal_pct_to_angle,
    )

    # 60% duty maps to ~2400 RPM -> ~17 km/h with the ws_bridge
    # wheel/gear factor (0.15 m radius, 8:1). Tuned by observation.
    IDLE_CRUISE_PCT = float(os.environ.get("IDLE_CRUISE_PCT", "60"))
    PHASE_RAMP_UP_SEC   = 2.0
    PHASE_HOLD_SEC      = 6.0
    PHASE_RAMP_DOWN_SEC = 2.0
    PHASE_COAST_SEC     = 2.0
    TICK = 0.1  # 10 Hz pedal updates

    bus = None
    t_phase = 0.0
    phase = "ramp_up"

    def _send_pct(pct: float) -> None:
        try:
            send_pedal_override(pedal_pct_to_angle(pct))
        except Exception as exc:  # noqa: BLE001
            log.debug("Idle pedal send failed: %s", exc)

    while True:
        try:
            # Pause cruise while a scenario is running OR a remote user
            # holds control (so manual pedal inputs from the UI aren't
            # fought by this loop).
            now = time.time()
            with _lock_mu:
                remote_locked = (
                    _control_lock["client_id"] is not None
                    and now < _control_lock["expires_at"]
                    and _control_lock["client_id"] != "taktflow-fault-inject"
                )

            if _idle_paused or remote_locked:
                # Reset so we start a fresh cycle when cruise resumes.
                if bus is not None or phase != "ramp_up":
                    clear_pedal_override()
                    phase = "ramp_up"
                    t_phase = 0.0
                time.sleep(TICK)
                continue

            if bus is None:
                bus = _get_bus()
                log.info("Idle command loop: cruise profile started "
                         "(cruise_pct=%.0f)", IDLE_CRUISE_PCT)

            if phase == "ramp_up":
                pct = IDLE_CRUISE_PCT * min(1.0, t_phase / PHASE_RAMP_UP_SEC)
                _send_pct(pct)
                if t_phase >= PHASE_RAMP_UP_SEC:
                    phase, t_phase = "hold", 0.0
                    continue
            elif phase == "hold":
                _send_pct(IDLE_CRUISE_PCT)
                if t_phase >= PHASE_HOLD_SEC:
                    phase, t_phase = "ramp_down", 0.0
                    continue
            elif phase == "ramp_down":
                pct = IDLE_CRUISE_PCT * max(0.0, 1.0 - (t_phase / PHASE_RAMP_DOWN_SEC))
                _send_pct(pct)
                if t_phase >= PHASE_RAMP_DOWN_SEC:
                    clear_pedal_override()
                    phase, t_phase = "coast", 0.0
                    continue
            else:  # coast
                # No UDP traffic — SPI stub leaves both sensors at 0.
                if t_phase >= PHASE_COAST_SEC:
                    phase, t_phase = "ramp_up", 0.0
                    continue

            t_phase += TICK
        except Exception as exc:
            log.warning("Idle command loop error: %s", exc)
            bus = None
        time.sleep(TICK)


def _init_mqtt() -> paho_mqtt.Client:
    """Initialize MQTT client for fault-inject command publishing."""
    host = os.environ.get("MQTT_HOST", "localhost")
    port = int(os.environ.get("MQTT_PORT", "1883"))
    client = paho_mqtt.Client(
        paho_mqtt.CallbackAPIVersion.VERSION2,
        client_id="taktflow-fault-inject",
    )

    def _on_connect(client, userdata, flags, rc, properties=None):
        rc_val = rc.value if hasattr(rc, 'value') else rc
        if rc_val == 0:
            log.info("MQTT connected to %s:%d", host, port)
            # Re-subscribe test runner topics on reconnect
            if _test_runner is not None:
                _test_runner.on_mqtt_connect(client, userdata, flags, rc, properties)
        else:
            log.error("MQTT connect failed: rc=%s", rc)

    def _on_disconnect(client, userdata, flags, rc, properties=None):
        log.warning("MQTT disconnected (rc=%s) — will auto-reconnect", rc)

    client.on_connect = _on_connect
    client.on_disconnect = _on_disconnect
    mqtt_user = os.environ.get("MQTT_USER", "")
    mqtt_pass = os.environ.get("MQTT_PASSWORD", "")
    if mqtt_user:
        client.username_pw_set(mqtt_user, mqtt_pass)
    client.connect_async(host, port, keepalive=30)
    client.loop_start()
    log.info("MQTT client connecting to %s:%d", host, port)
    return client


app = FastAPI(
    title="Taktflow Fault Injection API",
    description="Trigger CAN fault scenarios for the Taktflow embedded demo.",
    version="1.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "https://taktflow-systems.com",
        "https://www.taktflow-systems.com",
        "http://localhost:3000",
    ],
    allow_credentials=True,
    allow_methods=["GET", "POST"],
    allow_headers=["*"],
)


def _trigger_scenario(name: str):
    """Trigger a scenario by name (used by test runner)."""
    entry = SCENARIOS.get(name)
    if entry:
        entry["fn"]()


@app.on_event("startup")
def _on_startup():
    global _mqtt_client, _test_runner
    _mqtt_client = _init_mqtt()
    set_mqtt_client(_mqtt_client)
    # Always publish initial unlocked state to clear any stale retained lock payload.
    _publish_lock_state()
    # Initialize test runner
    _test_runner = DashboardTestRunner(_mqtt_client, _trigger_scenario, reset_scenario)
    # Start lock watchdog (daemon thread — dies with process)
    t = threading.Thread(target=_lock_watchdog, daemon=True)
    t.start()
    log.info("Control lock watchdog started (duration=%ds)", LOCK_DURATION_SEC)
    # Start idle command loop (virtual pedal ECU — keeps FZC alive)
    t2 = threading.Thread(target=_idle_command_loop, daemon=True)
    t2.start()
    log.info("Idle command loop started (interval=%.0fms)", IDLE_CMD_INTERVAL * 1000)


# ---------------------------------------------------------------------------
# Control lock endpoints
# ---------------------------------------------------------------------------

@app.post("/api/fault/control/acquire")
def acquire_control(body: ClientIdBody, request: Request):
    """Acquire 5-min control lock. Returns 409 if already held by someone else."""
    _check_api_key(request)
    now = time.time()
    with _lock_mu:
        # Check if lock is already held (and not expired)
        if (
            _control_lock["client_id"] is not None
            and _control_lock["client_id"] != body.client_id
            and now < _control_lock["expires_at"]
        ):
            remaining = int(_control_lock["expires_at"] - now)
            raise HTTPException(
                status_code=409,
                detail=f"Another user has control",
                headers={"X-Remaining-Sec": str(remaining)},
            )
        _control_lock["client_id"] = body.client_id
        _control_lock["expires_at"] = now + LOCK_DURATION_SEC
        _control_lock["acquired_at"] = now
    log.info("Control acquired by %s for %ds", body.client_id, LOCK_DURATION_SEC)
    _publish_lock_state()
    return {
        "locked": True,
        "client_id": body.client_id,
        "remaining_sec": LOCK_DURATION_SEC,
    }


@app.post("/api/fault/control/release")
def release_control(body: ClientIdBody, request: Request):
    """Release control lock early. Returns 403 if not the holder."""
    _check_api_key(request)
    with _lock_mu:
        if _control_lock["client_id"] != body.client_id:
            raise HTTPException(status_code=403, detail="Not the lock holder")
        _control_lock["client_id"] = None
        _control_lock["expires_at"] = 0.0
        _control_lock["acquired_at"] = 0.0
    log.info("Control released by %s", body.client_id)
    _publish_lock_state()
    return {"locked": False}


@app.get("/api/fault/control/status")
def control_status():
    """Return current lock state."""
    now = time.time()
    with _lock_mu:
        locked = (
            _control_lock["client_id"] is not None
            and now < _control_lock["expires_at"]
        )
        return {
            "locked": locked,
            "client_id": _control_lock["client_id"] or "" if locked else "",
            "remaining_sec": max(0, int(_control_lock["expires_at"] - now)) if locked else 0,
        }


# ---------------------------------------------------------------------------
# API key guard
# ---------------------------------------------------------------------------

def _check_api_key(request: Request) -> None:
    """Reject mutating requests without a valid API key (if configured)."""
    if not FAULT_API_KEY:
        return  # No key configured — dev mode, allow all
    key = request.headers.get("X-Api-Key", "")
    if key != FAULT_API_KEY:
        raise HTTPException(status_code=401, detail="Invalid or missing API key")


# ---------------------------------------------------------------------------
# Lock guard helper
# ---------------------------------------------------------------------------

def _check_control_lock(request: Request) -> None:
    """If a lock is active and the requester is not the holder, reject with 403."""
    now = time.time()
    with _lock_mu:
        if _control_lock["client_id"] is None or now >= _control_lock["expires_at"]:
            return  # No active lock — allow
        caller = request.headers.get("X-Client-Id", "")
        if caller == _control_lock["client_id"]:
            return  # Caller is the lock holder — allow
        remaining = int(_control_lock["expires_at"] - now)
    raise HTTPException(
        status_code=403,
        detail="Another user has control",
        headers={"X-Remaining-Sec": str(remaining)},
    )


# ---------------------------------------------------------------------------
# Fault scenario endpoints
# ---------------------------------------------------------------------------

@app.post("/api/fault/scenario/{name}")
def trigger_scenario(name: str, request: Request):
    """Trigger a fault scenario by name."""
    _check_api_key(request)
    _check_control_lock(request)
    entry = SCENARIOS.get(name)
    if entry is None:
        raise HTTPException(
            status_code=404,
            detail=f"Unknown scenario '{name}'.  "
                   f"Available: {', '.join(SCENARIOS.keys())}",
        )
    global _idle_paused
    _idle_paused = True
    log.info("Triggering scenario: %s (idle commands paused)", name)
    try:
        result = entry["fn"]()
    except Exception as exc:
        log.error("Scenario '%s' failed: %s", name, exc)
        raise HTTPException(
            status_code=500,
            detail=f"Scenario '{name}' failed: {exc}",
        ) from exc
    finally:
        _idle_paused = False
    log.info("Scenario '%s' complete: %s (idle commands resumed)", name, result)
    return {"scenario": name, "result": result}


@app.post("/api/fault/reset")
def reset_all(request: Request):
    """Power-cycle reset: restart ECU containers to clear all latched faults."""
    _check_api_key(request)
    _check_control_lock(request)
    global _idle_paused
    log.info("Power-cycle reset initiated")
    try:
        result = reset_scenario()
    except Exception as exc:
        log.error("Reset failed: %s", exc)
        raise HTTPException(
            status_code=500,
            detail=f"Reset failed: {exc}",
        ) from exc
    _idle_paused = False
    log.info("Reset complete: %s (idle commands resumed)", result)
    return {"result": result}


@app.get("/api/fault/scenarios")
def list_scenarios():
    """List all available fault scenarios with descriptions."""
    return {
        "scenarios": {
            name: entry["description"]
            for name, entry in SCENARIOS.items()
        }
    }


@app.get("/api/fault/health")
def health_check():
    """Health check endpoint."""
    return {
        "status": "ok",
        "service": "fault_inject",
        "can_channel": os.environ.get("CAN_CHANNEL", "vcan0"),
    }


# ---------------------------------------------------------------------------
# E2E test suite endpoints
# ---------------------------------------------------------------------------

@app.post("/api/test/run")
def start_test_run(body: TestRunBody, request: Request):
    """Start E2E test suite. Requires control lock."""
    _check_api_key(request)
    _check_control_lock(request)
    if _test_runner is None:
        raise HTTPException(status_code=503, detail="Test runner not initialized")
    try:
        run_id = _test_runner.start(body.tests)
    except RuntimeError:
        raise HTTPException(status_code=409, detail="Test run already in progress")
    return {"run_id": run_id, "state": "running"}


@app.post("/api/test/stop")
def stop_test_run(request: Request):
    """Stop the running test suite after the current scenario."""
    _check_api_key(request)
    _check_control_lock(request)
    if _test_runner is None:
        raise HTTPException(status_code=503, detail="Test runner not initialized")
    if not _test_runner.stop():
        raise HTTPException(status_code=409, detail="No test run in progress")
    return {"state": "stopping"}


@app.get("/api/test/specs")
def list_test_specs():
    """List all available E2E test specs for UI selection."""
    from .test_specs import TEST_SPECS
    return {
        "specs": [
            {
                "id": s.id,
                "label": s.label,
                "sg": s.sg,
                "asil": s.asil,
                "he": s.he,
                "description": s.description,
            }
            for s in TEST_SPECS
        ]
    }


@app.get("/api/test/status")
def test_status():
    """Current test run state."""
    if _test_runner is None:
        return {"state": "idle"}
    return {"state": _test_runner.status, "run_id": _test_runner._run_id or ""}


@app.get("/api/test/result")
def test_result():
    """Last completed test run result."""
    if _test_runner is None or _test_runner.last_result is None:
        return {"state": "idle", "results": []}
    return _test_runner.last_result


def main():
    port = int(os.environ.get("FAULT_PORT", "8091"))
    log.info("Starting fault injection API on port %d", port)
    uvicorn.run(
        "fault_inject.app:app",
        host="127.0.0.1",
        port=port,
        log_level="info",
    )


if __name__ == "__main__":
    main()
