# Lessons Learned — SIL Nightly CI Pipeline

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-26 to 2026-03-01
**Scope:** Building automated SIL integration tests that run nightly in CI
**Result:** 18+ SIL test scenarios, nightly CI pipeline, verdict checking

---

## 1. SIL Tests Are Integration Tests, Not Unit Tests

SIL tests boot all 7 ECU containers, run fault injection scenarios, and check system-level behavior. They test the interaction between ECUs, not individual functions.

**What SIL tests verify:**
- ECU-to-ECU CAN communication
- E2E protection (CRC, alive counter)
- Fault detection and state transitions
- DTC broadcast and logging
- Heartbeat timeout detection

**Lesson:** SIL tests complement unit tests — they catch integration issues that unit tests with mocks cannot.

---

## 2. Docker Compose restart vs stop+start

`docker compose restart <service>` keeps the container but restarts the process. `docker compose stop && docker compose start` recreates the container.

**Problem:** Restart doesn't re-read environment variables or volume mounts. Stop+start does.

**Fix:** For SIL tests, use `docker compose restart` for ECU reset scenarios, `docker compose up -d --force-recreate` for clean-state scenarios.

**Lesson:** Know the difference. Restart = fast but stateful. Recreate = slow but clean.

---

## 3. Com TX/RX Bridge for SIL

In SIL, the COM module's TX and RX signals must bridge to SocketCAN. The bridge reads COM TX buffers, packs them into CAN frames, and sends via SocketCAN. Incoming CAN frames are unpacked into COM RX buffers.

**Problem:** Without the bridge, ECUs compute internally but never communicate.

**Lesson:** The COM-to-CAN bridge is the most critical SIL infrastructure component. Build and test it before writing any SIL test scenarios.

---

## 4. Verdict Checker Needs All Step Actions

The SIL test runner uses step actions to inject faults and check responses:

| Step Action | What it does |
|-------------|-------------|
| `inject_overcurrent` | Set motor current above threshold |
| `check_motor_disabled` | Verify motor driver is off |
| `check_dtc_logged` | Verify DTC broadcast sent |
| `docker_restart` | Restart an ECU container |
| `wait_ms` | Wait for timing-dependent behavior |

**Problem:** Missing step actions caused silent test failures — the verdict checker skipped unknown actions instead of failing.

**Fix:** Unknown step actions now fail the test with a clear error message.

**Lesson:** Unknown actions must fail loudly, not be silently skipped. Fail-closed applies to test infrastructure too.

---

## 5. Gateway MQTT Topics Must Match

The MQTT gateway subscribes to specific topics. If the ECU firmware publishes to a slightly different topic (e.g., `taktflow/ecus/cvc/heartbeat` vs `taktflow/ecu/cvc/heartbeat`), messages are silently lost.

**Fix:** Topic names defined as constants in a shared header. Gateway and firmware use the same constants.

**Lesson:** MQTT topic names are stringly-typed APIs. Define them once, share the definition.

---

## 6. RTE Scheduler Priority Matters in SIL

On real hardware, the RTOS scheduler ensures safety-critical tasks run first. On POSIX, all tasks are threads with default priority.

**Problem:** Non-safety tasks (dashboard update, DTC broadcast) could starve safety tasks (heartbeat, E2E check) under CPU load.

**Fix:** Set `pthread_setschedparam` with `SCHED_FIFO` for safety-critical threads, `SCHED_OTHER` for QM threads.

**Lesson:** SIL without task priority is not representative of real behavior. Set priorities to match the RTOS configuration.

---

## 7. 18 SIL Failures in One Night

The first nightly run found 18 failures:
- 6 from COM TX/RX bridge not initialized
- 4 from missing step actions in verdict checker
- 3 from gateway topic mismatches
- 3 from DTC broadcast timing
- 2 from heartbeat counter wrap-around

**Fix strategy:** Group by root cause (same as unit test hardening), fix the infrastructure first (bridge, verdict checker), then the scenarios.

**Lesson:** First nightly run always fails badly. The infrastructure (bridge, checker, topics) must be solid before scenarios matter.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| SIL vs unit | SIL tests catch integration issues mocks cannot |
| restart vs recreate | Know the difference — stateful vs clean |
| COM bridge | Most critical SIL component — build first |
| Unknown actions | Fail loudly, never skip silently |
| MQTT topics | Define once, share the definition — stringly-typed = fragile |
| Task priority | Set POSIX thread priorities to match RTOS config |
| First nightly | Always fails badly — fix infrastructure before scenarios |
