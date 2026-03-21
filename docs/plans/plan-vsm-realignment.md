# Plan: Vehicle State Machine Realignment to HARA

**Date:** 2026-03-21
**Status:** PENDING REVIEW
**Trigger:** VSM-HARA alignment audit found 2 critical defects + 1 severity misclassification
**Research:** Real automotive behavior confirmed via ISO 26262, Bosch/Continental/Infineon publications, Waymo/Cruise safety reports

---

## Findings from Audit + Research

### Fix 1: SC_KILL → SHUTDOWN (Critical)

**Current code:** INIT/RUN/DEGRADED + SC_KILL → SAFE_STOP
**Required:** Any state + SC_KILL → SHUTDOWN

**Why:** SC_KILL is an external override. The Safety Controller kills the power relay because the main ECU has already failed to reach safe state. ISO 26262 Part 4 Clause 7.4.4.5: the independent watchdog shall bring the system to safe state independently. Real SDV platforms (Waymo, Cruise): SC first signals SAFE_STOP, then kills relay after timeout. When the relay kills, the main ECU loses power — that's SHUTDOWN, not SAFE_STOP.

**Change:** 3 lines in transition table (Swc_VehicleState.c):
- Line 89: `INIT + SC_KILL → SHUTDOWN` (was SAFE_STOP)
- Line 109: `RUN + SC_KILL → SHUTDOWN` (was SAFE_STOP)
- Line 178: All other states + SC_KILL → SHUTDOWN

### Fix 2: MOTOR_CUTOFF severity reclassification (Major)

**Current code:** RUN + MOTOR_CUTOFF → SAFE_STOP
**Required:** RUN + MOTOR_CUTOFF → DEGRADED

**Why:** Loss of motor torque is **fail-silent** (motor stops providing torque). This is ASIL B, not ASIL D. The vehicle can still brake and steer — it just can't accelerate. Bosch 3-level degradation model classifies this as Level 1 (performance limitation). ISO 26262 explicitly allows DEGRADED as a safe state for loss-of-function faults.

Contrast with CREEP_FAULT (unintended torque) which is **fail-active** (ASIL D) → SAFE_STOP is correct for that.

**Change:** 1 line in transition table:
- `RUN + MOTOR_CUTOFF → DEGRADED` (was SAFE_STOP)

### Fix 3: NVM state persistence (Deferred)

**SSR-CVC-019 requires:** Persist vehicle state + fault bitmask to NVM. Prevent reset-washing.
**Research confirms:** Every production ASIL-rated ECU does this. AUTOSAR Dem confirmed/pending DTCs survive power cycles.

**Decision:** Defer to separate plan. Reason: NvM integration is a separate BSW module task, not a VSM transition table fix. The Dem module already stores DTCs — NvM backing is the gap. Create `plan-nvm-integration.md` later.

### Fix 4: TSR-035 documentation update

**Add to TSR-035 table:**
- CREEP_FAULT event (maps to HE-017, SG-001)
- Explicit severity classification: loss-of-function = DEGRADED, unintended-action = SAFE_STOP

### No-change items (confirmed correct by research)

| Item | Current | Research Says | Verdict |
|------|---------|--------------|---------|
| 30ms motor cutoff debounce | 3 × 10ms cycles | Industry standard 20-50ms | **Keep** |
| CREEP_FAULT → SAFE_STOP | In code | Correct (fail-active, ASIL D) | **Keep** |
| BRAKE_FAULT → SAFE_STOP | In code | Correct (ASIL D, loss of braking is critical) | **Keep** |
| STEERING_FAULT → SAFE_STOP | In code | Correct (ASIL D at our speed/scenario) | **Keep** |
| SAFE_STOP → INIT (recovery) | In code | Conservative (forces self-test), defensible | **Keep** |
| 5s recovery window | 3s unlatch + 2s clear | Matches TSR-035 | **Keep** |

---

## Implementation Plan

### Step 1: Write tests FIRST (TDD)

Add to `test/unit/bsw/test_Swc_VehicleState_asild.c`:
- `test_SC_KILL_from_INIT_goes_SHUTDOWN` — assert next state = SHUTDOWN
- `test_SC_KILL_from_RUN_goes_SHUTDOWN` — assert next state = SHUTDOWN
- `test_SC_KILL_from_DEGRADED_goes_SHUTDOWN` — assert next state = SHUTDOWN
- `test_SC_KILL_from_SAFE_STOP_goes_SHUTDOWN` — assert next state = SHUTDOWN
- `test_MOTOR_CUTOFF_from_RUN_goes_DEGRADED` — assert next state = DEGRADED (not SAFE_STOP)
- `test_CREEP_FAULT_from_RUN_goes_SAFE_STOP` — assert next state = SAFE_STOP (unchanged, regression guard)

### Step 2: Fix transition table

Change 4 entries in `Swc_VehicleState.c` transition table:
```
[INIT][SC_KILL]      = SHUTDOWN   (was SAFE_STOP)
[RUN][SC_KILL]       = SHUTDOWN   (was SAFE_STOP)
[DEGRADED][SC_KILL]  = SHUTDOWN   (was SAFE_STOP)
[SAFE_STOP][SC_KILL] = SHUTDOWN   (was SAFE_STOP)
[RUN][MOTOR_CUTOFF]  = DEGRADED   (was SAFE_STOP)
```

### Step 3: Verify tests pass

Run the VSM unit tests. All new + existing must pass.

### Step 4: Update requirements docs

- TSR-035: Add CREEP_FAULT event row, clarify severity column
- SSR-CVC-016: Add note on MOTOR_CUTOFF = DEGRADED (fail-silent) vs CREEP = SAFE_STOP (fail-active)

### Step 5: Update audit doc

Mark defects 1 and 2 as FIXED in `vsm-hara-alignment-audit.md`.

---

## Risk Assessment

| Change | Risk | Mitigation |
|--------|------|-----------|
| SC_KILL → SHUTDOWN | Low — SHUTDOWN is more conservative than SAFE_STOP | Existing SHUTDOWN entry actions (all safe defaults) already implemented in BswM |
| MOTOR_CUTOFF → DEGRADED | Medium — previously went to SAFE_STOP (safer), now DEGRADED (less restrictive) | DEGRADED still limits torque to 75%, applies brake on grade. If motor cutoff persists + additional fault, transitions to SAFE_STOP via dual-fault path |
| Tests added | None — regression protection | — |

---

## HITL Review Checklist

Before implementing, confirm:
- [ ] SC_KILL → SHUTDOWN is correct (not SC_KILL → SAFE_STOP → SHUTDOWN two-step)
- [ ] MOTOR_CUTOFF → DEGRADED is acceptable (vehicle can still brake/steer, just no power)
- [ ] NVM persistence deferred to separate plan (acceptable for bench)
- [ ] BRAKE_FAULT and STEERING_FAULT stay at SAFE_STOP (correct per ASIL D)
- [ ] 30ms debounce stays at 3 cycles (correct per industry standard)
