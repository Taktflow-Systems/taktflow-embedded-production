# Plan: HIL Test Suite — Closed-Loop Plant-Sim Verification

**Status**: IN PROGRESS (Phases 1-3 DONE)
**Branch**: `feature/hil-test-suite`
**Traces**: TSR-035, TSR-038, TSR-046, GAP-6
**Prerequisite**: plan-hil-plant-sim (DONE — plant-sim running on Pi with can0)

## Problem

The plant-sim is running on the Pi, sending closed-loop physics on `can0` (0x600/0x601 virtual sensors, 0x100 vehicle state, 0x200/0x201 steering/brake status). The 3 simulated ECUs (BCM, ICU, TCU) consume CAN traffic and produce responses. But there is **no automated test suite** to verify HIL behavior against safety requirements.

SIL has `verdict_checker.py` + 18 YAML scenarios, but these are tied to Docker/vcan0 and the fault-inject REST API. HIL needs its own test runner that:
1. Operates on `can0` (physical CAN bus)
2. Uses MQTT for fault injection (no REST API on Pi)
3. Accounts for real-time timing (tighter than Docker SIL)
4. Maps every test case to TSR/SSR/SWR requirement IDs
5. Produces JUnit XML with requirement traceability (closes GAP-6)

## What Already Exists

| Component | Location | Reusable? |
|-----------|----------|-----------|
| SIL verdict checker | `test/sil/verdict_checker.py` | ~70% — CAN monitoring, verdict evaluation, JUnit XML |
| 18 SIL YAML scenarios | `test/sil/scenarios/` | Format reusable, timing/setup needs HIL adaptation |
| Dashboard test specs | `gateway/fault_inject/test_specs.py` | 8 scenarios with SG/ASIL/HE mapping |
| Plant-sim (5 models) | `gateway/plant_sim/` | Running on Pi — provides closed-loop environment |
| HIL launch script | `scripts/hil-plant-sim.sh` | Starts plant-sim on can0 |
| DBC file | `gateway/taktflow.dbc` | CAN signal definitions for decoding |

## Architecture

```
┌────────────────────────────┐     SSH / local
│  test/hil/hil_runner.py    │ ◄── runs on Pi or dev machine with CAN
│  (python-can on can0)      │     connected to can0 via USB-CAN
│                            │
│  Reads: YAML scenarios     │
│  Sends: CAN inject frames  │
│  Monitors: CAN responses   │
│  Injects: MQTT faults      │
│  Outputs: JUnit XML        │
└────────────┬───────────────┘
             │ can0
             ▼
┌────────────────────────────┐
│  Pi: plant-sim + BCM/ICU/TCU│
│  (running on can0)          │
└────────────────────────────┘
```

### Key Differences from SIL Verdict Checker

| Aspect | SIL | HIL |
|--------|-----|-----|
| CAN interface | `vcan0` (socketcan) | `can0` (socketcan) or USB-CAN |
| Fault injection | REST API (`localhost:5050`) | MQTT (`mosquitto_pub`) or direct CAN frame injection |
| Reset mechanism | Docker restart | Process restart via SSH or CAN-based soft reset |
| Timing tolerance | Loose (Docker jitter 10-50ms) | Tighter (Linux jitter 1-5ms) |
| ECUs under test | 7 Docker containers (all POSIX) | 3 POSIX (BCM/ICU/TCU) + plant-sim |
| Physical ECUs | None | Not yet (pending IoHwAb override plan) |
| Requirement tags | `verifies: [SWR-xxx]` | `verifies: [TSR-xxx, SSR-xxx, SWR-xxx]` with ASIL level |

## Testable Requirements on Current HIL Setup

The current HIL has **3 simulated ECUs + plant-sim** on `can0`. Physical STM32 ECUs are not connected to plant-sim yet (pending IoHwAb override). Requirements testable now:

### Category 1: Plant-Sim Closed-Loop Dynamics (7 tests)

These verify that the plant-sim produces correct physics on CAN, and POSIX ECU sensor feeders consume them.

| Test ID | Requirement | ASIL | What to Verify |
|---------|-------------|------|----------------|
| HIL-001 | TSR-035, SSR-RZC-003 | B | Motor current on 0x301 reacts to torque commands on 0x101 |
| HIL-002 | TSR-038, SSR-RZC-006 | B | Motor temperature on 0x302 rises under sustained load |
| HIL-003 | TSR-046, SSR-RZC-001 | B | Battery voltage on 0x303 sags under motor load |
| HIL-004 | TSR-010, SSR-FZC-002 | D | Steering angle on 0x200 tracks command on 0x102 with rate limit |
| HIL-005 | TSR-015, SSR-FZC-007 | D | Brake position on 0x201 tracks command on 0x103 with rate limit |
| HIL-006 | TSR-018, SSR-FZC-012 | C | Lidar distance on 0x220 responds to simulated obstacles |
| HIL-007 | TSR-005, SSR-CVC-016 | D | Vehicle state machine transitions (INIT→RUN→DEGRADED→SAFE_STOP) visible on 0x100 |

### Category 2: Heartbeat & Liveness (5 tests)

| Test ID | Requirement | ASIL | What to Verify |
|---------|-------------|------|----------------|
| HIL-010 | TSR-025, SSR-CVC-011 | C | CVC heartbeat 0x010 present at 50ms ±10% |
| HIL-011 | TSR-026, SSR-FZC-018 | C | FZC heartbeat 0x011 present at 50ms ±10% |
| HIL-012 | TSR-027, SSR-RZC-011 | C | RZC heartbeat 0x012 present at 50ms ±10% |
| HIL-013 | SWR-ICU-009 | QM | ICU displays ECU health from heartbeats (check 0x360 body status) |
| HIL-014 | SWR-BCM-002 | QM | BCM receives vehicle state 0x100, timeout→SHUTDOWN behavior |

### Category 3: Simulated ECU Behavior (6 tests)

| Test ID | Requirement | ASIL | What to Verify |
|---------|-------------|------|----------------|
| HIL-020 | SWR-BCM-003/004 | QM | BCM headlight auto-on in RUN state, auto-off in SHUTDOWN |
| HIL-021 | SWR-BCM-006/007 | QM | BCM turn indicator 1.5 Hz, hazard lights override turns |
| HIL-022 | SWR-BCM-010 | QM | BCM body status 0x360 broadcasts at 100ms with alive counter |
| HIL-023 | SWR-ICU-002/003/004/005 | QM | ICU gauge CAN reception (speed, torque, temp, battery) |
| HIL-024 | SWR-TCU-002/004 | QM | TCU UDS session control + read DTC via 0x7DF |
| HIL-025 | SWR-TCU-005 | QM | TCU UDS Read DID (vehicle speed, battery voltage, state) |

### Category 4: Fault Injection via MQTT (5 tests, requires mosquitto)

| Test ID | Requirement | ASIL | What to Verify |
|---------|-------------|------|----------------|
| HIL-030 | TSR-007, SSR-RZC-004 | B | Overcurrent injection → RZC DTC 0xE301, motor cutoff |
| HIL-031 | TSR-011, SSR-FZC-002 | D | Steering fault injection → FZC DTC 0xD001, SAFE_STOP |
| HIL-032 | TSR-016, SSR-FZC-007 | D | Brake fault injection → FZC DTC 0xE202, SAFE_STOP |
| HIL-033 | TSR-046, SSR-RZC-001 | B | Battery undervoltage injection → DEGRADED/LIMP |
| HIL-034 | TSR-009, SSR-RZC-006 | B | Motor overtemp injection → DTC, derating on 0x301 |

### Category 5: E2E Protection & CAN Integrity (3 tests)

| Test ID | Requirement | ASIL | What to Verify |
|---------|-------------|------|----------------|
| HIL-040 | TSR-022, SSR-CVC-008 | D | E2E CRC-8 + alive counter present on 0x100 (Vehicle_State) |
| HIL-041 | TSR-023, SSR-CVC-010 | D | Corrupted CAN frame → receiver rejects (inject bad CRC on can0) |
| HIL-042 | TSR-024, SSR-FZC-015 | D | Alive counter freeze → receiver falls back to safe default |

**Total: 26 test cases covering 28 TSRs, 24 SSRs, 15 SWRs**

## Phases

| # | Phase | Status |
|---|-------|--------|
| 1 | Create HIL test runner (`test/hil/hil_runner.py`) | DONE |
| 2 | Write YAML scenario files (`test/hil/scenarios/`) | DONE |
| 3 | Create CAN keyword helpers (`test/hil/can_helpers.py`) | DONE |
| 4 | Run on Pi and validate all 26 tests | PENDING |
| 5 | Add to CI as manual trigger workflow | DONE |

---

### Phase 1: Create HIL test runner

**What**: Fork `test/sil/verdict_checker.py` into `test/hil/hil_runner.py` with these changes:

1. Default CAN channel = `can0` (env `HIL_CAN_CHANNEL`)
2. Replace REST fault injection with MQTT publish (`paho-mqtt`)
3. Add `inject_can` action for direct CAN frame injection (corrupt E2E, simulate missing heartbeat)
4. Add requirement tag extraction from YAML (`verifies:` field includes TSR/SSR/SWR IDs)
5. JUnit XML output with requirement IDs in test case properties
6. Support `--interface` flag for USB-CAN (`slcan`, `socketcan`)
7. Configurable timing tolerances (HIL tighter than SIL)

**Files**:

| File | Change |
|------|--------|
| `test/hil/hil_runner.py` | NEW — HIL verdict checker |
| `test/hil/__init__.py` | NEW — package init |
| `test/hil/requirements.txt` | NEW — python-can, paho-mqtt, junit-xml, pyyaml |

---

### Phase 2: Write YAML scenario files

**What**: Create 26 YAML scenario files following the SIL format, adapted for HIL.

**Format** (same structure as SIL, extended with requirement traceability):

```yaml
name: "Motor Current Closed-Loop Response"
id: "HIL-001"
description: >
  Verify motor current on CAN 0x301 reacts to torque commands on 0x101.
  Plant-sim computes motor physics, sends virtual sensors on 0x601,
  RZC sensor feeder injects into MCAL, RZC SWC reads and transmits 0x301.
verifies:
  - "TSR-035"
  - "SSR-RZC-003"
asil: "B"
aspice: "SWE.6"
iso_reference: "ISO 26262 Part 4, Section 7 — HSI Verification"
timeout_sec: 30

setup:
  - action: wait_state
    state: RUN
    timeout: 20
    description: "Wait for system to reach RUN (plant-sim + ECUs running)"

steps:
  - action: inject_can
    can_id: 0x101
    data: [0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00]
    description: "Send 50% torque forward command"
  - action: wait
    seconds: 3
    description: "Allow closed-loop to stabilize (plant-sim → 0x601 → RZC → 0x301)"

verdicts:
  - type: can_message
    can_id: 0x301
    field: motor_current
    min_value: 1000
    max_value: 15000
    description: "Motor current (0x301) shows non-zero value reacting to 50% torque"

  - type: can_message
    can_id: 0x601
    description: "Virtual sensor frame 0x601 present (plant-sim → RZC feeder)"

teardown:
  - action: inject_can
    can_id: 0x101
    data: [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    description: "Zero torque command"
```

**Files**: 26 YAML files in `test/hil/scenarios/` named `hil_001_*.yaml` through `hil_042_*.yaml`

---

### Phase 3: Create CAN keyword helpers

**What**: Utility module for common CAN operations used by the test runner.

**Functions**:
- `send_can(bus, can_id, data)` — send a single CAN frame
- `wait_for_message(bus, can_id, timeout)` — block until CAN ID seen
- `check_heartbeat(bus, can_id, period_ms, tolerance_pct, duration_sec)` — verify periodic message
- `check_e2e(msg, crc_poly, alive_pos)` — validate E2E CRC-8 and alive counter
- `decode_signal(msg, dbc, signal_name)` — decode CAN signal using DBC
- `inject_mqtt_fault(host, topic, payload)` — publish fault injection via MQTT
- `measure_period(bus, can_id, sample_count)` — measure message period statistics

**Files**:

| File | Change |
|------|--------|
| `test/hil/can_helpers.py` | NEW — CAN utility functions |

---

### Phase 4: Run on Pi and validate

**What**: Deploy test suite to Pi, run all 26 scenarios, fix any failures.

**Steps**:
1. `rsync` test/hil/ to Pi
2. Start plant-sim + 3 POSIX ECUs
3. `python3 -m hil_runner --scenarios test/hil/scenarios/ --output results/`
4. Review JUnit XML — all 26 tests should pass
5. Fix timing tolerances or scenario definitions as needed

**Expected results**:
- Category 1-3 (18 tests): Should pass without MQTT
- Category 4 (5 tests): Requires mosquitto on Pi (Phase 4 of plant-sim plan)
- Category 5 (3 tests): Should pass with CAN frame injection

---

### Phase 5: CI workflow

**What**: Add `.github/workflows/hil-test.yml` as a manual-trigger workflow.

HIL tests can't run in GitHub Actions (needs physical CAN bus). The workflow:
1. Triggers manually (`workflow_dispatch`) or via SSH to Pi
2. Runs the test suite on Pi
3. Uploads JUnit XML as artifact
4. Parses requirement coverage from results

**Files**:

| File | Change |
|------|--------|
| `.github/workflows/hil-test.yml` | NEW — manual HIL test workflow |

---

## Files Modified

| File | Change |
|------|--------|
| `test/hil/hil_runner.py` | NEW — HIL test runner (forked from SIL verdict_checker) |
| `test/hil/__init__.py` | NEW — package init |
| `test/hil/can_helpers.py` | NEW — CAN utility functions |
| `test/hil/requirements.txt` | NEW — dependencies |
| `test/hil/scenarios/*.yaml` | NEW — 26 scenario files |
| `.github/workflows/hil-test.yml` | NEW — CI workflow (manual) |

**Total**: ~30 new files, 0 modifications to existing code.

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Timing differences SIL vs HIL | HIL tolerances set tighter (±10%) but still account for Linux jitter. Configurable via env var. |
| MQTT not available for fault injection | Category 4 tests skip gracefully with MQTT_UNAVAILABLE verdict. Core tests (Cat 1-3, 5) work without MQTT. |
| USB-CAN latency affects measurements | Use socketcan on Pi directly (not USB-CAN from dev machine). USB-CAN adds 1-5ms latency. |
| Plant-sim in SAFE_STOP (no SC on bus) | Tests expect SAFE_STOP as initial state when SC is absent. Test setup accounts for this. |
| Physical ECUs not tested | Clearly marked as "HIL-POSIX" scope. Physical ECU tests deferred to IoHwAb override plan. |

## Requirement Coverage Summary

| Category | Tests | TSRs | SSRs | SWRs | ASIL Range |
|----------|-------|------|------|------|------------|
| Closed-loop dynamics | 7 | 7 | 7 | 0 | B-D |
| Heartbeat & liveness | 5 | 3 | 3 | 2 | QM-C |
| Simulated ECU behavior | 6 | 0 | 0 | 15 | QM |
| Fault injection (MQTT) | 5 | 5 | 5 | 0 | B-D |
| E2E & CAN integrity | 3 | 3 | 3 | 0 | D |
| **Total** | **26** | **18** | **18** | **17** | **QM-D** |

This covers **35% of TSRs** (18/51) and **22% of SSRs** (18/81) that are testable without physical ECU hardware. The remaining requirements need either physical STM32 sensor input (IoHwAb override) or SC on the bus.
