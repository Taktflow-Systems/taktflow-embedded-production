# Plan: FZC Brake Command Oscillation Detection

**Date**: 2026-03-02

## Context

The `brake_fault` scenario sends rapid 0%/100% Brake_Command (0x103) alternations to the FZC, but the system stays in RUN. Root cause: FZC has no conflicting-command detection — it only detects PWM feedback deviation (2% threshold, 3-cycle debounce) and command timeout (100ms). In SIL, simulated feedback follows commanded instantly, so feedback deviation never triggers. The scenario correctly sends on 0x103 (the real command channel), but the FZC lacks the intelligence to flag it as abnormal.

**Goal**: Add command oscillation detection to FZC's `Swc_Brake` so the full safety chain works:
scenario sends bad commands on 0x103 → FZC detects oscillation → FZC sends Brake_Fault (0x210) → CVC receives via BSW → EVT_BRAKE_FAULT → SAFE_STOP.

No scenario changes needed — 0x103 is the correct injection point.

## Design

**Detection algorithm**: Track the absolute change in brake command between consecutive cycles. If the change exceeds a threshold (e.g., 30%) for N consecutive cycles, it's an oscillation fault.

```
prev_cmd = Brake_PrevCmd (stored from last cycle)
delta = |brake_cmd - prev_cmd|
if delta > FZC_BRAKE_OSCILLATION_DELTA_THRESH:
    oscillation_counter++
else:
    oscillation_counter = 0
if oscillation_counter >= FZC_BRAKE_OSCILLATION_DEBOUNCE:
    new_fault = FZC_BRAKE_CMD_OSCILLATION
```

**Why this works**: Normal brake commands change gradually (driver modulation) or hold steady. A 30%+ jump every cycle for 4+ consecutive cycles is physically impossible from a human driver and indicates a faulty sender, bus attack, or software bug.

**Constants**:
- `FZC_BRAKE_OSCILLATION_DELTA_THRESH  30u` — 30% minimum jump to count as oscillation
- `FZC_BRAKE_OSCILLATION_DEBOUNCE       4u` — 4 consecutive large jumps = fault (40ms)

**New fault code**: `FZC_BRAKE_CMD_OSCILLATION  4u`
**New DTC**: `FZC_DTC_BRAKE_OSCILLATION  15u` (next available after `FZC_DTC_WATCHDOG_FAIL = 14u`)

## Phase 1: TDD — Write Tests First

**File**: `firmware/fzc/test/test_Swc_Brake_asild.c`

| Test | Setup | Assertion |
|------|-------|-----------|
| `test_Oscillation_no_fault_stable_command` | Send same brake_cmd for 10 cycles | fault = NO_FAULT |
| `test_Oscillation_no_fault_small_delta` | Alternate 40%/60% (delta=20 < 30) for 10 cycles | fault = NO_FAULT |
| `test_Oscillation_no_fault_single_jump` | One jump 0→100, then stable at 100 | fault = NO_FAULT (counter resets) |
| `test_Oscillation_fault_after_debounce` | Alternate 0%/100% for 4+ cycles | fault = FZC_BRAKE_CMD_OSCILLATION |
| `test_Oscillation_fault_triggers_auto_brake` | Oscillation detected | brake PWM forced to 100%, motor cutoff started |
| `test_Oscillation_DTC_reported` | Oscillation detected | Dem_ReportErrorStatus called with FZC_DTC_BRAKE_OSCILLATION |

## Phase 2: Implement Detection

### `firmware/fzc/include/Fzc_Cfg.h`

Add:
```c
/* Brake oscillation detection */
#define FZC_BRAKE_OSCILLATION_DELTA_THRESH  30u   /* 30% min jump per cycle */
#define FZC_BRAKE_OSCILLATION_DEBOUNCE       4u   /* 4 consecutive = fault */

/* Fault code */
#define FZC_BRAKE_CMD_OSCILLATION           4u

/* DTC */
#define FZC_DTC_BRAKE_OSCILLATION          15u   /* 0xD10400 */
```

### `firmware/fzc/src/Swc_Brake.c`

Add between timeout check (line ~203) and feedback check (line ~228):

1. Static variables: `static uint8 Brake_PrevCmd`, `static uint8 Brake_OscillationCounter`
2. Init both to 0 in `Swc_Brake_Init()`
3. Detection logic:
   - Compute delta = abs(brake_cmd - Brake_PrevCmd)
   - If delta > threshold → increment counter; else → reset counter
   - If counter >= debounce → `new_fault = FZC_BRAKE_CMD_OSCILLATION`
   - Store `Brake_PrevCmd = brake_cmd`
4. DTC reporting: Add `FZC_BRAKE_CMD_OSCILLATION` case in the DTC section (~line 326)

No changes to fault handling, auto-brake, motor cutoff, or CAN TX — the existing paths handle any non-zero fault code.

## Phase 3: Verify & Commit

1. `make -f Makefile.posix TARGET=fzc test` — all tests green
2. `make -f Makefile.posix TARGET=cvc test` — no regressions
3. Push → CI green (all workflows)
4. Write lesson learned (before commit per rule)
5. Deploy to VPS
6. Test: click `brake_fault` → CVC transitions to SAFE_STOP
7. Test: click `reset` → recovery to RUN

## Files Modified

| # | File | Change |
|---|------|--------|
| 1 | `firmware/fzc/test/test_Swc_Brake_asild.c` | 6 new oscillation tests (TDD first) |
| 2 | `firmware/fzc/include/Fzc_Cfg.h` | New constants, fault code, DTC |
| 3 | `firmware/fzc/src/Swc_Brake.c` | Oscillation detection logic + DTC case |
| 4 | `docs/reference/lessons-learned/infrastructure/PROCESS-simulated-relay-sil.md` | Lesson 13 |
