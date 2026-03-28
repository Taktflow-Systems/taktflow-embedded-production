"""Diagnostic harness for suspected SAFE_STOP failure paths.

Runs each suspected scenario in two phases:
1) immediate injection after RUN
2) post-grace injection after a settle delay (default: 10.5s)

This isolates whether failures are caused by timing gates (for example CVC
post-INIT grace) versus actual missing transition/fault signals.
"""

from __future__ import annotations

import argparse
import json
import os
import ssl
import sys
import threading
import time
import urllib.error
import urllib.request
import uuid
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

import paho.mqtt.client as paho_mqtt


@dataclass(frozen=True)
class PathSpec:
    id: str
    scenario: str
    observe_sec: float
    expected_dtc: int | None = None


@dataclass
class ProbeResult:
    phase: str
    path_id: str
    scenario: str
    expected_dtc: str | None
    reached_run: bool
    reached_safe_stop: bool
    first_safe_stop_ms: int | None
    expected_dtc_seen: bool | None
    first_expected_dtc_ms: int | None
    final_state: str
    final_state_code: int
    transitions: list[str]
    dtcs: list[str]
    overcurrent_seen: bool
    steer_fault_seen: bool
    brake_fault_seen: bool
    notes: list[str]


VEHICLE_STATES = {
    0: "INIT",
    1: "RUN",
    2: "DEGRADED",
    3: "LIMP",
    4: "SAFE_STOP",
    5: "SHUTDOWN",
}

SUSPECT_PATHS: list[PathSpec] = [
    PathSpec(id="overcurrent_path", scenario="overcurrent", observe_sec=8.0, expected_dtc=0xE301),
    PathSpec(id="steer_fault_path", scenario="steer_fault", observe_sec=8.0, expected_dtc=0xD001),
    PathSpec(id="brake_fault_path", scenario="brake_fault", observe_sec=8.0, expected_dtc=0xE202),
    PathSpec(id="motor_reversal_path", scenario="motor_reversal", observe_sec=8.0, expected_dtc=0xE301),
    PathSpec(id="creep_from_stop_path", scenario="creep_from_stop", observe_sec=8.0, expected_dtc=None),
]


class VerdictProbe:
    """Tracks MQTT telemetry signals relevant to SAFE_STOP diagnosis."""

    def __init__(self):
        self._lock = threading.Lock()
        self.window_start = time.time()
        self.vehicle_state = 0
        self.transitions: list[tuple[float, int]] = []
        self.dtcs: list[tuple[float, int]] = []
        self.overcurrent_seen = False
        self.steer_fault_seen = False
        self.brake_fault_seen = False

    def reset_window(self):
        with self._lock:
            self.window_start = time.time()
            self.transitions = []
            self.dtcs = []
            self.overcurrent_seen = False
            self.steer_fault_seen = False
            self.brake_fault_seen = False

    def on_message(self, topic: str, payload: str):
        now = time.time()
        with self._lock:
            if topic == "taktflow/can/Vehicle_State/VehicleState":
                new_state = _parse_int(payload)
                if new_state != self.vehicle_state:
                    self.vehicle_state = new_state
                    self.transitions.append((now, new_state))
            elif topic == "taktflow/can/DTC_Broadcast/DTC_Number":
                dtc = _parse_int(payload)
                if dtc:
                    self.dtcs.append((now, dtc))
            elif topic == "taktflow/can/Motor_Current/OvercurrentFlag":
                self.overcurrent_seen = self.overcurrent_seen or (_parse_int(payload) != 0)
            elif topic == "taktflow/can/Steering_Status/SteerFaultStatus":
                self.steer_fault_seen = self.steer_fault_seen or (_parse_int(payload) != 0)
            elif topic == "taktflow/can/Brake_Status/BrakeFaultStatus":
                self.brake_fault_seen = self.brake_fault_seen or (_parse_int(payload) != 0)
            elif topic.startswith("taktflow/alerts/dtc/"):
                try:
                    alert = json.loads(payload)
                    raw = alert.get("dtc", "0")
                    dtc = int(raw, 16) if isinstance(raw, str) and raw.startswith("0x") else int(raw)
                    if dtc:
                        self.dtcs.append((now, dtc))
                except (TypeError, ValueError, json.JSONDecodeError):
                    pass

    def wait_for_state(self, target: int, timeout_sec: float) -> bool:
        deadline = time.time() + timeout_sec
        while time.time() < deadline:
            with self._lock:
                if self.vehicle_state == target:
                    return True
            time.sleep(0.1)
        return False

    def snapshot(self) -> dict[str, Any]:
        with self._lock:
            return {
                "window_start": self.window_start,
                "vehicle_state": self.vehicle_state,
                "transitions": list(self.transitions),
                "dtcs": list(self.dtcs),
                "overcurrent_seen": self.overcurrent_seen,
                "steer_fault_seen": self.steer_fault_seen,
                "brake_fault_seen": self.brake_fault_seen,
            }


class FaultApiClient:
    def __init__(self, base_url: str, client_id: str, insecure_tls: bool = False):
        self.base_url = base_url.rstrip("/")
        self.client_id = client_id
        self.ssl_context = None
        if insecure_tls:
            self.ssl_context = ssl._create_unverified_context()

    def acquire(self):
        self._request("POST", "/api/fault/control/acquire", {"client_id": self.client_id})

    def release(self):
        self._request("POST", "/api/fault/control/release", {"client_id": self.client_id})

    def reset(self):
        self._request("POST", "/api/fault/reset", None, add_client_header=True, timeout=180)

    def trigger(self, scenario: str):
        self._request("POST", f"/api/fault/scenario/{scenario}", None, add_client_header=True, timeout=60)

    def _request(
        self,
        method: str,
        path: str,
        body: dict[str, Any] | None,
        add_client_header: bool = False,
        timeout: int = 30,
    ) -> dict[str, Any]:
        url = f"{self.base_url}{path}"
        data = None if body is None else json.dumps(body).encode("utf-8")
        headers = {"Content-Type": "application/json"}
        api_key = os.environ.get("FAULT_API_KEY", "")
        if api_key:
            headers["X-Api-Key"] = api_key
        if add_client_header:
            headers["X-Client-Id"] = self.client_id
        req = urllib.request.Request(url, data=data, method=method, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=timeout, context=self.ssl_context) as resp:
                payload = resp.read().decode("utf-8") if resp.length != 0 else "{}"
                return json.loads(payload) if payload else {}
        except urllib.error.HTTPError as exc:
            detail = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"{method} {path} failed: HTTP {exc.code} {detail}") from exc
        except urllib.error.URLError as exc:
            raise RuntimeError(f"{method} {path} failed: {exc}") from exc


def _parse_int(val: str) -> int:
    try:
        return int(float(val))
    except (ValueError, TypeError):
        return 0


def _dtc_hex(value: int | None) -> str | None:
    return None if value is None else f"0x{value:04X}"


def _build_probe_result(spec: PathSpec, phase: str, snap: dict[str, Any], reached_run: bool) -> ProbeResult:
    transitions = snap["transitions"]
    dtcs = snap["dtcs"]
    start = snap["window_start"]

    safe_hits = [(ts, state) for ts, state in transitions if state == 4]
    reached_safe_stop = len(safe_hits) > 0
    first_safe_stop_ms = None
    if safe_hits:
        first_safe_stop_ms = int((safe_hits[0][0] - start) * 1000)

    expected_dtc_seen = None
    first_expected_dtc_ms = None
    if spec.expected_dtc is not None:
        exp = [(ts, dtc) for ts, dtc in dtcs if dtc == spec.expected_dtc]
        expected_dtc_seen = len(exp) > 0
        if exp:
            first_expected_dtc_ms = int((exp[0][0] - start) * 1000)

    final_state_code = snap["vehicle_state"]
    final_state = VEHICLE_STATES.get(final_state_code, f"state_{final_state_code}")
    transition_names = [VEHICLE_STATES.get(st, f"state_{st}") for _, st in transitions]
    dtc_names = [f"0x{dtc:04X}" for _, dtc in dtcs]

    notes: list[str] = []
    if not reached_run:
        notes.append("RUN not reached after reset")
    if expected_dtc_seen is True and not reached_safe_stop:
        notes.append("DTC observed but SAFE_STOP not reached")
    if expected_dtc_seen is False and spec.expected_dtc is not None:
        notes.append("Expected DTC missing")

    return ProbeResult(
        phase=phase,
        path_id=spec.id,
        scenario=spec.scenario,
        expected_dtc=_dtc_hex(spec.expected_dtc),
        reached_run=reached_run,
        reached_safe_stop=reached_safe_stop,
        first_safe_stop_ms=first_safe_stop_ms,
        expected_dtc_seen=expected_dtc_seen,
        first_expected_dtc_ms=first_expected_dtc_ms,
        final_state=final_state,
        final_state_code=final_state_code,
        transitions=transition_names,
        dtcs=dtc_names,
        overcurrent_seen=snap["overcurrent_seen"],
        steer_fault_seen=snap["steer_fault_seen"],
        brake_fault_seen=snap["brake_fault_seen"],
        notes=notes,
    )


def run_path_probe(
    api: FaultApiClient,
    probe: VerdictProbe,
    spec: PathSpec,
    phase: str,
    settle_sec: float,
    run_wait_sec: float,
) -> ProbeResult:
    api.reset()
    reached_run = probe.wait_for_state(1, timeout_sec=run_wait_sec)
    if reached_run and settle_sec > 0:
        time.sleep(settle_sec)

    probe.reset_window()
    api.trigger(spec.scenario)
    time.sleep(spec.observe_sec)
    snap = probe.snapshot()
    return _build_probe_result(spec, phase, snap, reached_run)


def connect_probe(mqtt_host: str, mqtt_port: int, monitor: VerdictProbe) -> paho_mqtt.Client:
    client = paho_mqtt.Client(paho_mqtt.CallbackAPIVersion.VERSION2, client_id=f"path-harness-{uuid.uuid4().hex[:8]}")

    def _on_connect(cl, userdata, flags, rc, properties=None):
        rc_val = rc.value if hasattr(rc, "value") else rc
        if rc_val != 0:
            raise RuntimeError(f"MQTT connect failed: rc={rc_val}")
        cl.subscribe("taktflow/can/#", qos=0)
        cl.subscribe("taktflow/alerts/dtc/#", qos=0)

    def _on_message(cl, userdata, msg, properties=None):
        topic = msg.topic
        payload = msg.payload.decode("utf-8", errors="replace")
        monitor.on_message(topic, payload)

    client.on_connect = _on_connect
    client.on_message = _on_message
    mqtt_user = os.environ.get("MQTT_USER", "")
    mqtt_pass = os.environ.get("MQTT_PASSWORD", "")
    if mqtt_user:
        client.username_pw_set(mqtt_user, mqtt_pass)
    client.connect(mqtt_host, mqtt_port, keepalive=30)
    client.loop_start()
    return client


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run diagnostic harness for suspected SAFE_STOP paths.")
    parser.add_argument("--base-url", default="https://sil.taktflow-systems.com", help="Fault API base URL")
    parser.add_argument("--mqtt-host", default="sil.taktflow-systems.com", help="MQTT host")
    parser.add_argument("--mqtt-port", type=int, default=1883, help="MQTT port")
    parser.add_argument("--client-id", default=f"path-harness-{uuid.uuid4().hex[:8]}", help="Fault API client ID")
    parser.add_argument("--post-grace-sec", type=float, default=10.5, help="Settle delay for post-grace phase")
    parser.add_argument("--run-wait-sec", type=float, default=45.0, help="Timeout waiting for RUN after reset")
    parser.add_argument("--phase", choices=["both", "immediate", "post-grace"], default="both")
    parser.add_argument("--insecure-tls", action="store_true", help="Disable TLS verification for HTTPS")
    parser.add_argument(
        "--output",
        default=str(Path("tmp") / f"path-harness-{int(time.time())}.json"),
        help="JSON output path",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    monitor = VerdictProbe()
    mqtt_client = connect_probe(args.mqtt_host, args.mqtt_port, monitor)
    api = FaultApiClient(args.base_url, args.client_id, insecure_tls=args.insecure_tls)

    phases: list[tuple[str, float]] = []
    if args.phase in ("both", "immediate"):
        phases.append(("immediate", 0.0))
    if args.phase in ("both", "post-grace"):
        phases.append(("post-grace", args.post_grace_sec))

    results: list[ProbeResult] = []
    started_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

    try:
        api.acquire()
        # Let subscriptions warm up.
        time.sleep(1.0)

        for phase_name, settle_sec in phases:
            for spec in SUSPECT_PATHS:
                res = run_path_probe(api, monitor, spec, phase_name, settle_sec, args.run_wait_sec)
                results.append(res)
                status = "PASS" if res.reached_safe_stop else "FAIL"
                print(
                    f"[{phase_name}] {spec.id}: {status} "
                    f"(dtc={res.expected_dtc_seen}, safe_stop_ms={res.first_safe_stop_ms}, "
                    f"final={res.final_state})"
                )
    finally:
        try:
            api.release()
        except Exception:
            pass
        mqtt_client.loop_stop()
        mqtt_client.disconnect()

    payload = {
        "started_at": started_at,
        "base_url": args.base_url,
        "mqtt_host": args.mqtt_host,
        "mqtt_port": args.mqtt_port,
        "client_id": args.client_id,
        "phase": args.phase,
        "post_grace_sec": args.post_grace_sec,
        "paths": [asdict(r) for r in results],
    }

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"Wrote report: {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
