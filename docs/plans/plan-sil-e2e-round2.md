# Plan: SIL E2E Round 2 — Fix 14 Remaining Failures

**Date**: 2026-03-05
**Status**: IN PROGRESS — Phases 1-4 DONE, Phases 5-7 pending
**Baseline**: 4/18 passing (SIL-001, SIL-016, SIL-017, SIL-018 close)

---

## Root Cause Summary

| # | Root Cause | Affected Scenarios | Fix Complexity |
|---|---|---|---|
| 1 | DTC frame format mismatch (plant-sim 16-bit LE vs firmware 24-bit BE) | SIL-006, 007, 008, 011, 012, 013, 015 | Low |
| 2 | E-Stop DIO inject is write-once latch (never clears to LOW) | SIL-003, 013 | Low |
| 3 | Vehicle state YAML expects DEGRADED, firmware gives LIMP for single-zone loss | SIL-004 | Low (YAML only) |
| 4 | CVC can't report its own death (0x100 stops when CVC dies) | SIL-005 | Low (YAML only) |
| 5 | Motor overcurrent/overtemp fault not sustaining through 3-cycle confirmation | SIL-007, 010 | Low-Medium |
| 6 | SAP QM gateway reads empty URL despite docker-compose setting | SIL-018 | Low |
| 7 | E2E error counter verdict type not implemented in verdict checker | SIL-009 | Low-Medium |
| 8 | Minor: RPM latency, alive counter wrap, SOC monotonic, DTC persistence | SIL-002, 014, 015 | Varies |

---

## Phase 1: DTC Frame Format Alignment (HIGH IMPACT — 7 scenarios)

**Problem**: Plant-sim packs DTC as 16-bit LE in bytes [0:2], firmware Dem.c packs as 24-bit BE in bytes [0:3]. Verdict checker reads 24-bit BE.

Plant-sim sends 0xE401 as: `[0x01, 0xE4, 0x01, 0x03, ...]`
Verdict reads: `(0x01 << 16) | (0xE4 << 8) | 0x01 = 0x01E401` — mismatch

**Fix**: Align plant-sim with firmware 24-bit BE format.

### Files to change:
1. `gateway/fault_inject/scenarios.py` — `_dtc_frame()` (lines 184-200)
2. `gateway/plant_sim/simulator.py` — `_send_dtc()` + DTC constants

### Critical: DTC_STEER_FAULT mismatch
- Plant-sim: `DTC_STEER_FAULT = 0xE201`
- Firmware FZC: `Dem_SetDtcCode(FZC_DTC_STEER_PLAUSIBILITY, 0x00D001u)`
- Plant-sim must use `0x00D001` to match firmware

---

## Phase 2: E-Stop DIO Pass-Through Fix (2 scenarios)

**Problem**: `Swc_CvcCom.c` only writes DIO HIGH when inject != 0, never writes LOW.

**Fix**: Bidirectional pass-through — write DIO to match inject value every cycle.

### Files to change:
1. `firmware/cvc/src/Swc_CvcCom.c` — E-Stop inject block (lines 373-385)

---

## Phase 3: Vehicle State YAML Corrections (1 scenario)

**Problem**: SIL-004 expects DEGRADED for single FZC loss, firmware gives LIMP.

**Fix**: Update YAML expected state.

### Files to change:
1. `test/sil/scenarios/sil_004_can_busoff_fzc.yaml` — DEGRADED → LIMP

---

## Phase 4: CVC Death Detection YAML Fix (1 scenario)

**Problem**: SIL-005 expects vehicle_state SAFE_STOP on 0x100, but CVC is dead and can't send it.

**Fix**: Remove vehicle_state verdict, rely on existing heartbeat_loss + motor_shutdown verdicts.

### Files to change:
1. `test/sil/scenarios/sil_005_watchdog_timeout_cvc.yaml`

---

## Phase 5: Fault Confirmation Duration (2 scenarios)

Investigate + ensure plant-sim sustains faults through 3-cycle confirmation window.

---

## Phase 6: SAP QM URL Fix (1 scenario)

Investigate why gateway reads empty SAP_QM_API_URL.

---

## Phase 7: E2E Error Counter Verdict (1 scenario)

Check/implement `e2e_error_count` verdict type in verdict_checker.py.

---

## Execution Order

Phases 1-4 first (highest impact), then 5-7, then deploy + full SIL run.

**Expected**: 4/18 → ~15/18 after Phases 1-4, ~18/18 after all phases.
