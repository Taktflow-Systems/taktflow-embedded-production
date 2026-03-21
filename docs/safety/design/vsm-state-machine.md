# Vehicle State Machine — Design Specification

**Document ID:** VSM-DESIGN-001
**Version:** 2.0 (post-HARA realignment)
**Date:** 2026-03-21
**ASIL:** D
**Traces to:** TSR-035, SSR-CVC-010 through SSR-CVC-013

---

## State Diagram

```
                    ┌──────────┐
                    │   INIT   │ ← Power-on entry
                    │  (st=0)  │
                    └────┬─────┘
                         │
              ┌──────────┼──────────┐
              │          │          │
        SELF_TEST_PASS   │    SELF_TEST_FAIL
        (heartbeats OK)  │          │
              │          │          ▼
              ▼      SC_KILL    ┌──────────┐
         ┌─────────┐    │      │SAFE_STOP │
         │   RUN   │    │      │  (st=4)  │──── VEHICLE_STOPPED ──→ SHUTDOWN
         │  (st=1) │    │      └──────────┘
         └────┬────┘    │           ▲
              │         │           │
    ┌─────────┼─────────┼───────────┤
    │         │         │           │
    │  PEDAL_SINGLE     │     PEDAL_DUAL
    │  BATTERY_WARN     │     CAN_TIMEOUT_*
    │  MOTOR_CUTOFF     │     ESTOP
    │         │         │     BRAKE_FAULT
    │         ▼         │     STEERING_FAULT
    │  ┌──────────┐     │     CREEP_FAULT
    │  │ DEGRADED │     │           │
    │  │  (st=2)  │─────┤           │
    │  └────┬─────┘     │           │
    │       │           │           │
    │  BATTERY_CRIT     │           │
    │       │           │           │
    │       ▼           │           │
    │  ┌──────────┐     │           │
    │  │   LIMP   │─────┤           │
    │  │  (st=3)  │     │           │
    │  └──────────┘     │           │
    │                   │           │
    │         SC_KILL from ANY state │
    │              │                │
    │              ▼                │
    │       ┌──────────┐            │
    └──────→│ SHUTDOWN │←───────────┘
            │  (st=5)  │  (terminal)
            └──────────┘
```

## States

| State | Value | Description | Entry Actions | Safe State? |
|-------|-------|-------------|---------------|-------------|
| **INIT** | 0 | Boot, self-test, waiting for heartbeats | Run self-test, OLED "INIT" | No (transitional) |
| **RUN** | 1 | Nominal operation | Enable all actuators, torque limit 100% | No (operational) |
| **DEGRADED** | 2 | Minor fault, reduced capability | Torque limit 75%, warn driver | Yes (reduced) |
| **LIMP** | 3 | Battery/thermal issue, very limited | Torque limit 30%, speed limit 20 km/h | Yes (reduced) |
| **SAFE_STOP** | 4 | Critical fault, stopping vehicle | Torque 0%, motor cutoff, auto-brake, OLED "FAULT" | Yes (active stop) |
| **SHUTDOWN** | 5 | Terminal state, all safe defaults | All actuators off, NVM write, wait for power-off | Yes (terminal) |

## Events and Transitions

| Event | ID | From → To | ASIL | HARA Mapping |
|-------|-----|-----------|------|-------------|
| SELF_TEST_PASS | 0 | INIT → RUN | D | — |
| SELF_TEST_FAIL | 1 | INIT → SAFE_STOP | D | — |
| PEDAL_FAULT_SINGLE | 2 | RUN → DEGRADED | D | HE-001 (SG-001) |
| PEDAL_FAULT_DUAL | 3 | RUN/DEGRADED → SAFE_STOP | D | HE-001 (SG-001) |
| CAN_TIMEOUT_SINGLE | 4 | RUN/DEGRADED → SAFE_STOP | D | HE-012 (SG-008) |
| CAN_TIMEOUT_DUAL | 5 | RUN/DEGRADED/LIMP → SAFE_STOP | D | HE-012 (SG-008) |
| ESTOP | 6 | RUN/DEGRADED/LIMP → SAFE_STOP | B | HE-020 (SG-008) |
| **SC_KILL** | 7 | **ANY → SHUTDOWN** | D | HE-012 (SG-008) |
| FAULT_CLEARED | 8 | DEGRADED/LIMP → RUN | D | — |
| CAN_RESTORED | 9 | LIMP → DEGRADED | D | — |
| VEHICLE_STOPPED | 10 | SAFE_STOP → SHUTDOWN | D | — |
| MOTOR_CUTOFF | 11 | **RUN → DEGRADED** (fail-silent) | B | HE-002 (SG-001) |
| BRAKE_FAULT | 12 | RUN/DEGRADED → SAFE_STOP | D | HE-005 (SG-004) |
| STEERING_FAULT | 13 | RUN/DEGRADED → SAFE_STOP | D | HE-004 (SG-003) |
| BATTERY_WARN | 14 | RUN → DEGRADED | A | HE-010 (SG-006) |
| BATTERY_CRIT | 15 | RUN/DEGRADED → LIMP | A | HE-010 (SG-006) |
| CREEP_FAULT | 16 | RUN → SAFE_STOP | D | HE-017 (SG-001) |

## Key Design Decisions

### 1. SC_KILL → SHUTDOWN (not SAFE_STOP)

**Decision:** SC_KILL transitions directly to SHUTDOWN from any state.
**Rationale:** The Safety Controller kills the power relay when the main ECU has failed to reach safe state. This is an external override — the main ECU is no longer in control. ISO 26262-4 Clause 7.4.4.5: independent safety mechanism operates independently. Waymo/Cruise: SC kills = instant power loss = SHUTDOWN.
**Implication:** SHUTDOWN entry actions must execute quickly (NVM write + actuator safe defaults).

### 2. MOTOR_CUTOFF → DEGRADED (not SAFE_STOP)

**Decision:** Loss of motor torque is a DEGRADED transition, not SAFE_STOP.
**Rationale:** Loss of torque is fail-silent (ASIL B). The vehicle can still brake and steer. Bosch 3-level degradation model: Level 1 (performance limitation). ISO 26262 allows DEGRADED as safe state for loss-of-function faults.
**Contrast:** CREEP_FAULT (unintended torque) is fail-active (ASIL D) → SAFE_STOP.

### 3. Fault Confirmation (30ms / 3 cycles)

**Decision:** Motor/brake/steering faults require 3 consecutive 10ms confirmations.
**Rationale:** Industry standard 20-50ms debounce. Infineon AURIX reference: 3 cycles at 10ms. Within ASIL D FTTI budget (50-100ms).
**Source:** Bosch SAE 2016-01-0127, Infineon AP32393.

### 4. SAFE_STOP Recovery → INIT (not RUN)

**Decision:** Recovery from SAFE_STOP goes to INIT (forces self-test), not directly to RUN.
**Rationale:** Conservative approach. After a critical fault + stop, the system should verify all subsystems before resuming operation. 5-second clear window (3s unlatch + 2s confirm).

### 5. NVM State Persistence (NOT YET IMPLEMENTED)

**Decision:** Deferred. SSR-CVC-019 requires NVM persistence to prevent reset-washing.
**Status:** Dem module stores DTCs. NvM backing for vehicle state not implemented.
**Risk:** Power cycle clears fault state. Acceptable for bench, not for production.

## Timing Budget

| Path | Detection | Confirmation | Reaction | Total | FTTI |
|------|-----------|-------------|----------|-------|------|
| Pedal dual fault → SAFE_STOP | 10ms | 30ms | 5ms | 45ms | 50ms (SG-001) |
| Creep fault → SAFE_STOP | 10ms | 30ms | 5ms | 45ms | 50ms (SG-001) |
| Brake fault → SAFE_STOP | 10ms | 30ms | 5ms | 45ms | 50ms (SG-004) |
| Steering fault → SAFE_STOP | 10ms | 30ms | 5ms | 45ms | 100ms (SG-003) |
| CAN timeout → SAFE_STOP | 500ms | — | 10ms | 510ms | — |
| SC_KILL → SHUTDOWN | — | — | <5ms | <5ms | 100ms (SG-008) |

## Implementation Reference

**Source file:** `firmware/ecu/cvc/src/Swc_VehicleState.c`
**Transition table:** Lines 79-199 (6×17 static array)
**Event derivation:** `Swc_VehicleState_MainFunction` (10ms cyclic, lines 474-884)
**Fault confirmation:** `Swc_VehicleState_ConfirmFault` (lines 404-461)

## Known Issues

1. **16 pre-existing test failures** in `test_Swc_VehicleState_asild.c` — stale mock signal IDs and CAN_TIMEOUT severity expectation mismatch. Need investigation.
2. **NVM persistence not implemented** — SSR-CVC-019 deferred.
3. **CAN_TIMEOUT_SINGLE goes to SAFE_STOP** — some tests expect LIMP. Need to verify against HARA whether single-zone CAN loss is minor (LIMP) or critical (SAFE_STOP).
