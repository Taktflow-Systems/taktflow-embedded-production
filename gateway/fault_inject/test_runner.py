"""Dashboard E2E test runner — MQTT verdict monitor + sequential test executor.

Subscribes to taktflow/# MQTT topics, monitors vehicle state / DTCs / fault
flags, and publishes real-time progress to taktflow/test/progress.

Usage:
    runner = DashboardTestRunner(mqtt_client, trigger_fn)
    runner.start()          # starts in daemon thread
    runner.is_running       # check state
    runner.last_result      # last completed run result dict
"""

import json
import logging
import os
import threading
import time
import uuid

import paho.mqtt.client as paho_mqtt

from .test_specs import TEST_SPECS, TEST_SPECS_BY_ID, TestSpec, VerdictCheck

log = logging.getLogger("fault_inject")

# Vehicle state enum (matches ws_bridge)
VEHICLE_STATES = {0: "INIT", 1: "RUN", 2: "DEGRADED", 3: "LIMP", 4: "SAFE_STOP", 5: "SHUTDOWN"}

# Global hardening: settle after RUN before fault injection.
# Can be tuned per environment without code changes.
DEFAULT_POST_RUN_SETTLE_SEC = float(os.environ.get("TEST_POST_RUN_SETTLE_SEC", "10.5"))


class MQTTVerdictMonitor:
    """Subscribes to taktflow/# and tracks vehicle state, DTCs, and fault flags.

    The monitor maintains rolling state that the test runner polls during the
    observe window to evaluate verdict checks.
    """

    def __init__(self):
        self._lock = threading.Lock()
        self.vehicle_state: int = 0
        self.dtcs_seen: set[int] = set()
        self.steer_fault: int = 0
        self.brake_fault: int = 0
        self.motor_faults: int = 0
        self.motor_overcurrent: int = 0
        self.anomaly_score: float = 0.0
        self.motor_rpm: int = 0
        self.torque_request: int = 0
        # Timestamps for verdicts
        self._state_change_ts: dict[int, float] = {}
        self._dtc_ts: dict[int, float] = {}
        self._fault_ts: dict[str, float] = {}

    def reset(self):
        """Clear all tracked state for a new scenario."""
        with self._lock:
            self.vehicle_state = 0
            self.dtcs_seen.clear()
            self.steer_fault = 0
            self.brake_fault = 0
            self.motor_faults = 0
            self.motor_overcurrent = 0
            self.anomaly_score = 0.0
            self.motor_rpm = 0
            self.torque_request = 0
            self._state_change_ts.clear()
            self._dtc_ts.clear()
            self._fault_ts.clear()

    def on_mqtt_message(self, topic: str, payload: str):
        """Process an MQTT message and update tracked state."""
        with self._lock:
            self._process(topic, payload)

    def _process(self, topic: str, payload: str):
        now = time.time()

        if topic.startswith("taktflow/can/"):
            parts = topic.split("/")
            if len(parts) < 4:
                return
            msg_name, sig_name = parts[2], parts[3]
            # Strip message prefix from signal name (CAN gateway publishes
            # "Vehicle_State_Mode" but ws_bridge strips to "Mode").
            prefix = msg_name + "_"
            if sig_name.startswith(prefix):
                sig_name = sig_name[len(prefix):]

            if msg_name == "Vehicle_State" and sig_name in ("Mode", "VehicleState"):
                new_state = self._int(payload)
                if new_state != self.vehicle_state:
                    self.vehicle_state = new_state
                    self._state_change_ts[new_state] = now

            elif msg_name == "DTC_Broadcast" and sig_name == "DTC_Number":
                dtc = self._int(payload)
                if dtc:
                    self.dtcs_seen.add(dtc)
                    self._dtc_ts[dtc] = now

            elif msg_name == "Steering_Status" and sig_name == "SteerFaultStatus":
                val = self._int(payload)
                if val and not self.steer_fault:
                    self._fault_ts["steer_fault"] = now
                self.steer_fault = val

            elif msg_name == "Brake_Status" and sig_name == "BrakeFaultStatus":
                val = self._int(payload)
                if val and not self.brake_fault:
                    self._fault_ts["brake_fault"] = now
                self.brake_fault = val

            elif msg_name == "Motor_Status" and sig_name == "MotorFaultStatus":
                self.motor_faults = self._int(payload)

            elif msg_name == "Motor_Current" and sig_name == "OvercurrentFlag":
                self.motor_overcurrent = self._int(payload)

            elif msg_name == "Motor_Status" and sig_name == "MotorSpeed_RPM":
                self.motor_rpm = self._int(payload)

            elif msg_name == "Torque_Request" and sig_name == "TorqueRequest":
                self.torque_request = self._int(payload)

        elif topic.startswith("taktflow/anomaly/") and topic.endswith("/score"):
            try:
                data = json.loads(payload)
                self.anomaly_score = float(data.get("score", 0))
            except (json.JSONDecodeError, TypeError, ValueError):
                pass

        elif topic.startswith("taktflow/alerts/dtc/"):
            try:
                alert = json.loads(payload)
                dtc_str = alert.get("dtc", "0")
                dtc_num = int(dtc_str, 16) if isinstance(dtc_str, str) and dtc_str.startswith("0x") else int(dtc_str)
                if dtc_num:
                    self.dtcs_seen.add(dtc_num)
                    self._dtc_ts[dtc_num] = now
            except (json.JSONDecodeError, TypeError, ValueError):
                pass

    def check_verdict(self, check: VerdictCheck, start_time: float) -> dict:
        """Evaluate a single verdict check against current state.

        Returns a dict with keys: description, expected, observed, passed, elapsed_ms
        """
        with self._lock:
            return self._evaluate(check, start_time)

    def _evaluate(self, check: VerdictCheck, start_time: float) -> dict:
        result = {
            "description": check.description,
            "expected": check.expected,
            "observed": "",
            "passed": False,
            "elapsed_ms": 0,
        }

        if check.check_type == "vehicle_state":
            expected_values = check.value if isinstance(check.value, list) else [check.value]
            for val in expected_values:
                if val in self._state_change_ts:
                    elapsed = (self._state_change_ts[val] - start_time) * 1000
                    state_name = VEHICLE_STATES.get(val, f"state_{val}")
                    result["observed"] = f"{state_name} at t={elapsed:.0f}ms"
                    result["passed"] = elapsed <= check.timeout_ms
                    result["elapsed_ms"] = int(elapsed)
                    return result
            # Not seen yet
            current_name = VEHICLE_STATES.get(self.vehicle_state, f"state_{self.vehicle_state}")
            result["observed"] = f"Current state: {current_name} (expected not reached)"
            return result

        elif check.check_type == "dtc":
            dtc_code = check.value
            if dtc_code in self.dtcs_seen:
                elapsed = (self._dtc_ts.get(dtc_code, time.time()) - start_time) * 1000
                result["observed"] = f"DTC 0x{dtc_code:04X} at t={elapsed:.0f}ms"
                result["passed"] = elapsed <= check.timeout_ms
                result["elapsed_ms"] = int(elapsed)
            else:
                result["observed"] = f"DTC 0x{dtc_code:04X} not received"
            return result

        elif check.check_type == "fault_flag":
            field_name = check.field
            current_val = getattr(self, field_name, 0)
            if current_val == check.value:
                elapsed_ts = self._fault_ts.get(field_name)
                elapsed = ((elapsed_ts - start_time) * 1000) if elapsed_ts else 0
                result["observed"] = f"{field_name}={current_val} at t={elapsed:.0f}ms"
                result["passed"] = True
                result["elapsed_ms"] = int(elapsed)
            else:
                result["observed"] = f"{field_name}={current_val} (expected {check.value})"
            return result

        elif check.check_type == "state_stays":
            expected_values = check.value if isinstance(check.value, list) else [check.value]
            current_name = VEHICLE_STATES.get(self.vehicle_state, f"state_{self.vehicle_state}")
            unexpected = [
                (st, ts) for st, ts in self._state_change_ts.items()
                if st not in expected_values
            ]
            if unexpected:
                worst = min(unexpected, key=lambda x: x[1])
                bad_name = VEHICLE_STATES.get(worst[0], f"state_{worst[0]}")
                elapsed = (worst[1] - start_time) * 1000
                result["observed"] = f"Transitioned to {bad_name} at t={elapsed:.0f}ms"
                return result
            if self.vehicle_state in expected_values:
                result["observed"] = f"Stayed in {current_name} for full observe window"
                result["passed"] = True
            else:
                result["observed"] = f"Current state: {current_name}"
            return result

        elif check.check_type == "motor_stop":
            if self.motor_rpm == 0:
                result["observed"] = "Motor RPM = 0 (torque_req={})".format(self.torque_request)
                result["passed"] = True
            else:
                result["observed"] = "Motor RPM = {} (torque_req={})".format(
                    self.motor_rpm, self.torque_request)
            return result

        elif check.check_type == "torque_zero":
            if self.torque_request == 0:
                result["observed"] = "TorqueRequest = 0 (RPM={})".format(self.motor_rpm)
                result["passed"] = True
            else:
                result["observed"] = "TorqueRequest = {} (RPM={})".format(
                    self.torque_request, self.motor_rpm)
            return result

        result["observed"] = f"Unknown check type: {check.check_type}"
        return result

    @staticmethod
    def _int(val: str) -> int:
        try:
            return int(float(val))
        except (ValueError, TypeError):
            return 0


class DashboardTestRunner:
    """Runs test scenarios sequentially in a daemon thread.

    Publishes progress to taktflow/test/progress (retain=False) and
    final result to taktflow/test/result (retain=True).
    """

    def __init__(self, mqtt_client: paho_mqtt.Client, trigger_fn, reset_fn):
        """
        Args:
            mqtt_client: MQTT client for subscribing and publishing
            trigger_fn: callable(scenario_name) -> triggers a fault scenario
            reset_fn: callable() -> resets all actuators
        """
        self._mqtt = mqtt_client
        self._trigger = trigger_fn
        self._reset = reset_fn
        self._monitor = MQTTVerdictMonitor()
        self._running = False
        self._stop_requested = False
        self._lock = threading.Lock()
        self._run_id: str = ""
        self.last_result: dict | None = None

        # Register per-topic callbacks (persistent across reconnects)
        self._mqtt.message_callback_add("taktflow/can/#", self._on_message)
        self._mqtt.message_callback_add("taktflow/anomaly/#", self._on_message)
        self._mqtt.message_callback_add("taktflow/alerts/#", self._on_message)

        # Subscribe (also re-subscribe on reconnect via on_connect)
        self._subscribe()

    def _subscribe(self):
        """Subscribe to MQTT topics for verdict monitoring."""
        self._mqtt.subscribe("taktflow/can/#", qos=0)
        self._mqtt.subscribe("taktflow/anomaly/#", qos=0)
        self._mqtt.subscribe("taktflow/alerts/#", qos=0)
        log.info("Test runner subscribed to MQTT verdict topics")

    def on_mqtt_connect(self, client, userdata, flags, rc, properties=None):
        """Re-subscribe on reconnect (called from app.py on_connect)."""
        if rc == 0 or (hasattr(rc, 'value') and rc.value == 0):
            log.info("Test runner: MQTT connected — re-subscribing")
            self._subscribe()

    def _on_message(self, client, userdata, msg, properties=None):
        """Forward MQTT messages to the verdict monitor.

        Accepts optional properties arg for paho-mqtt v2.1+ VERSION2 compat.
        """
        topic = msg.topic
        payload = msg.payload.decode("utf-8", errors="replace")
        self._monitor.on_mqtt_message(topic, payload)

    @property
    def is_running(self) -> bool:
        return self._running

    @property
    def status(self) -> str:
        if self._running:
            return "running"
        if self.last_result:
            return "complete"
        return "idle"

    def start(self, test_ids: list[str] | None = None) -> str:
        """Start a test run in a daemon thread.

        Args:
            test_ids: optional list of scenario IDs to run (defaults to all 8)

        Returns:
            run_id string

        Raises:
            RuntimeError if already running
        """
        with self._lock:
            if self._running:
                raise RuntimeError("Test run already in progress")
            self._running = True

        self._run_id = str(uuid.uuid4())[:8]

        if test_ids:
            specs = [TEST_SPECS_BY_ID[tid] for tid in test_ids if tid in TEST_SPECS_BY_ID]
        else:
            specs = list(TEST_SPECS)

        thread = threading.Thread(
            target=self._run_suite,
            args=(specs, self._run_id),
            daemon=True,
        )
        self._stop_requested = False
        thread.start()
        return self._run_id

    def stop(self) -> bool:
        """Request the running suite to stop after the current scenario."""
        if not self._running:
            return False
        self._stop_requested = True
        log.info("[TEST %s] Stop requested", self._run_id)
        return True

    def _wait_for_run(self, run_id: str, timeout: float = 90.0,
                      stable_sec: float = 8.0) -> bool:
        """Poll MQTT verdict monitor until vehicle_state == 1 (RUN) and stable.

        Requires CVC to stay in RUN for ``stable_sec`` consecutive seconds
        before returning True.  This handles the SC relay-kill transient that
        can briefly push CVC to SAFE_STOP during Docker container startup.

        Timeouts increased for VPS reliability:
        - timeout: 90s (was 60s) — container restart on shared-CPU VPS can
          take 15-30s, leaving less margin for the 3s INIT→RUN + stable_sec.
        - stable_sec: 8s (was 5s) — VPS scheduling jitter can cause brief
          state flickers; 8s confirms genuine stability.

        Returns True if stable RUN was reached within timeout, False otherwise.
        """
        deadline = time.time() + timeout
        run_since: float | None = None
        while time.time() < deadline:
            if self._stop_requested:
                return False
            if self._monitor.vehicle_state == 1:
                if run_since is None:
                    run_since = time.time()
                    log.info("[TEST %s] CVC reached RUN, waiting %.0fs for stability",
                             run_id, stable_sec)
                elif time.time() - run_since >= stable_sec:
                    log.info("[TEST %s] CVC stable in RUN for %.0fs", run_id, stable_sec)
                    return True
            else:
                if run_since is not None:
                    log.info("[TEST %s] CVC left RUN (state=%d/%s), resetting stability timer",
                             run_id, self._monitor.vehicle_state,
                             VEHICLE_STATES.get(self._monitor.vehicle_state, "UNKNOWN"))
                run_since = None
            time.sleep(0.5)
        log.warning("[TEST %s] CVC did not reach stable RUN within %.0fs (state=%d/%s)",
                    run_id, timeout, self._monitor.vehicle_state,
                    VEHICLE_STATES.get(self._monitor.vehicle_state, "UNKNOWN"))
        return False

    def _run_suite(self, specs: list[TestSpec], run_id: str):
        """Execute the test suite (runs in daemon thread)."""
        start_time = time.time()
        results: list[dict] = []

        try:
            for idx, spec in enumerate(specs):
                if self._stop_requested:
                    log.info("[TEST %s] Stopped by user after %d scenarios", run_id, idx)
                    break

                self._publish_progress(run_id, len(specs), idx, spec, "preparing", results, start_time)

                # Prep: restart containers, THEN clear monitor state, THEN wait for RUN.
                # Monitor reset AFTER container restart to clear stale VehicleState
                # messages from the previous boot cycle (CVC sends state=1 while
                # zone controllers restart, then CVC itself restarts last).
                log.info("[TEST %s] Resetting for scenario: %s", run_id, spec.id)
                self._reset()
                self._monitor.reset()

                if not self._wait_for_run(run_id):
                    # CVC didn't reach RUN — skip this test
                    log.warning("[TEST %s] Skipping %s — CVC not in RUN", run_id, spec.id)
                    results.append({
                        "id": spec.id,
                        "label": spec.label,
                        "sg": spec.sg,
                        "asil": spec.asil,
                        "he": spec.he,
                        "description": spec.description,
                        "injection": spec.injection,
                        "passed": False,
                        "duration_sec": 0,
                        "verdicts": [{"description": "CVC did not reach RUN state",
                                      "expected": "vehicle_state == RUN",
                                      "observed": f"vehicle_state == {VEHICLE_STATES.get(self._monitor.vehicle_state, 'UNKNOWN')}",
                                      "passed": False, "elapsed_ms": 0}],
                    })
                    self._publish_progress(run_id, len(specs), idx + 1, spec, "done", results, start_time)
                    continue

                if spec.prep and spec.prep != "reset":
                    self._trigger(spec.prep)
                    time.sleep(1.5)  # allow prep scenario to take effect

                # Some scenarios rely on ConfirmFault paths in CVC.
                # In SIL, CVC suppresses these for a post-INIT grace period.
                # Harden all scenarios by applying a global minimum settle.
                settle_sec = spec.post_run_settle_sec
                if settle_sec < DEFAULT_POST_RUN_SETTLE_SEC:
                    settle_sec = DEFAULT_POST_RUN_SETTLE_SEC
                if settle_sec > 0.0:
                    log.info("[TEST %s] Settling %.1fs after RUN before %s",
                             run_id, settle_sec, spec.id)
                    settle_deadline = time.time() + settle_sec
                    while time.time() < settle_deadline:
                        if self._stop_requested:
                            break
                        time.sleep(0.2)

                # Clear monitor state before injection
                self._monitor.reset()

                # Inject fault
                self._publish_progress(run_id, len(specs), idx, spec, "injecting", results, start_time)
                log.info("[TEST %s] Injecting: %s", run_id, spec.scenario)
                inject_time = time.time()
                self._trigger(spec.scenario)

                # Observe window — poll verdicts
                self._publish_progress(run_id, len(specs), idx, spec, "observing", results, start_time)
                observe_end = inject_time + spec.observe_sec
                while time.time() < observe_end:
                    if self._stop_requested:
                        break
                    time.sleep(0.2)  # poll at 5Hz

                # Evaluate verdicts
                verdict_results = []
                for check in spec.verdicts:
                    v = self._monitor.check_verdict(check, inject_time)
                    log.info("[TEST %s] Verdict '%s': passed=%s observed='%s'",
                             run_id, check.check_type, v["passed"], v["observed"])
                    verdict_results.append(v)

                scenario_passed = all(v["passed"] for v in verdict_results)
                duration = time.time() - inject_time

                scenario_result = {
                    "id": spec.id,
                    "label": spec.label,
                    "sg": spec.sg,
                    "asil": spec.asil,
                    "he": spec.he,
                    "description": spec.description,
                    "injection": spec.injection,
                    "passed": scenario_passed,
                    "duration_sec": round(duration, 1),
                    "verdicts": verdict_results,
                }
                results.append(scenario_result)

                self._publish_progress(run_id, len(specs), idx + 1, spec, "done", results, start_time)
                log.info("[TEST %s] %s: %s (%.1fs)", run_id, spec.id,
                         "PASS" if scenario_passed else "FAIL", duration)

                # Brief pause between scenarios
                if idx < len(specs) - 1:
                    time.sleep(1.0)

        except Exception as exc:
            log.error("[TEST %s] Suite aborted: %s", run_id, exc)
        finally:
            # Reset after suite
            try:
                self._reset()
            except Exception:
                pass

            total_duration = time.time() - start_time
            passed = sum(1 for r in results if r["passed"])
            failed = len(results) - passed

            final_result = {
                "state": "complete",
                "run_id": run_id,
                "total": len(specs),
                "current_index": len(specs),
                "current_id": "",
                "current_label": "",
                "current_phase": "complete",
                "elapsed_sec": round(total_duration, 1),
                "results": results,
                "summary": {
                    "passed": passed,
                    "failed": failed,
                    "total": len(results),
                    "duration_sec": round(total_duration, 1),
                    "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                },
            }

            self.last_result = final_result
            # Clear progress topic so frontend stops showing "running"
            self._mqtt.publish(
                "taktflow/test/progress",
                json.dumps({"state": "complete", "run_id": run_id}),
                qos=0,
                retain=False,
            )
            self._publish_result(final_result)

            with self._lock:
                self._running = False

            log.info("[TEST %s] Suite complete: %d/%d passed in %.1fs",
                     run_id, passed, len(results), total_duration)

    def _publish_progress(self, run_id: str, total: int, current_idx: int,
                          spec: TestSpec, phase: str, results: list[dict],
                          suite_start: float):
        """Publish progress message to MQTT."""
        payload = {
            "state": "running",
            "run_id": run_id,
            "total": total,
            "current_index": current_idx,
            "current_id": spec.id,
            "current_label": spec.label,
            "current_phase": phase,
            "elapsed_sec": round(time.time() - suite_start, 1),
            "results": list(results),  # copy
        }
        self._mqtt.publish(
            "taktflow/test/progress",
            json.dumps(payload),
            qos=0,
            retain=False,
        )

    def _publish_result(self, result: dict):
        """Publish final result to MQTT with retain."""
        self._mqtt.publish(
            "taktflow/test/result",
            json.dumps(result),
            qos=0,
            retain=True,
        )
