# Plan: Creep Guard — SG-001 / HE-017

**Safety Goal**: SG-001 — Prevent unintended vehicle motion from motor fault
**Hazardous Event**: HE-017 — Vehicle moves without driver intent (ASIL D)
**Safety Mechanism**: SM-024 — SC standstill torque cross-plausibility
**Branch**: `fix/sil-bugfixes`

## Background

HE-017 was identified as a V-model traceability gap: no safety goal, FSR, TSR, or
SSR covered unintended vehicle motion from a hardware motor fault (BTS7960 FET
gate-source short).  The original plan proposed CVC-level detection, but
architectural analysis showed the SC is the correct location: a FET short bypasses
all software-layer torque controls, and only the SC (independent MCU with kill
relay) can react.

## Phase 1: SC Creep Guard (SSR-SC-018) — DONE

**Implemented**: SC standstill torque cross-plausibility in `sc_plausibility.c`.

### Design (as implemented)

SC monitors two CAN signals:
- **Torque**: Vehicle_State (0x100) byte 4 = TorquePct (from CVC)
- **Current**: Motor_Current (0x301) bytes 2-3 = MotorCurrent_mA (from RZC)

If `torque == 0` AND `current > 500 mA` for 2 consecutive SC main cycles (20ms):
- Kill relay → de-energize (SM-005)
- Vehicle transitions to SAFE_STOP
- DTC 0xE312 logged

### Files Modified

| File | Change |
|------|--------|
| `firmware/sc/include/sc_cfg.h` | `SC_CREEP_CURRENT_THRESH`, `SC_CREEP_DEBOUNCE_CYCLES`, `SC_KILL_REASON_CREEP_GUARD` |
| `firmware/sc/include/sc_plausibility.h` | `SC_CreepGuard_Check()`, `SC_Plausibility_IsCreepFaulted()` |
| `firmware/sc/src/sc_plausibility.c` | Creep guard implementation |
| `firmware/sc/src/sc_relay.c` | Trigger (b2): creep guard kill |
| `firmware/sc/src/sc_main.c` | Step 3a: call `SC_CreepGuard_Check()` |
| `firmware/sc/test/test_sc_relay_asild.c` | Mock + test for creep guard trigger |

## Phase 2: Test Validation — DONE

**Results**: 8/8 E2E tests pass (322s total). Creep guard fires at 712ms.

- Plant-sim injects 1000mA motor current (BTS7960 FET short simulation)
- No pedal input — vehicle at standstill with torque=0
- SC detects torque=0 AND current>500mA → kill relay → SAFE_STOP

## Phase 3: Safety Documentation — DONE

V-model traceability chain completed:
- **HE-017** → **SG-001** (Prevent unintended vehicle motion from motor fault)
- **SG-001** → **FSR-026** (Standstill Motor Current Cross-Plausibility)
- **FSR-026** → **TSR-052** (SC Standstill Motor Current Cross-Plausibility)
- **TSR-052** → **SSR-SC-018** (SC Standstill Torque Cross-Plausibility Monitor)
- SSR-SC-018 implemented in `sc_plausibility.c`, tested in unit + E2E

Lesson learned recorded in `docs/lessons-learned/web/ui-safety.md`.

## Status

| Phase | Status | Notes |
|-------|--------|-------|
| 1     | DONE | SC creep guard (SSR-SC-018) implemented |
| 2     | DONE | 8/8 E2E tests pass |
| 3     | DONE | Full V-model traceability chain |
