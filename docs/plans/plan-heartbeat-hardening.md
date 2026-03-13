# Plan: Heartbeat Monitoring Hardening — Professional AUTOSAR Patterns

## Context

The heartbeat system has had 3 bugs in rapid succession (commits `430f3ba`, `e4acb09`, `52b029d`), all stemming from the same root causes: manually-set timing constants, no compile-time validation, and no formal FTTI budget linking timeout values to safety goals. The current implementation works but has gaps vs. professional AUTOSAR practice:

- **HB_PERIOD_CYCLES** hardcoded independently from RTE period — caused 500ms and 250ms heartbeat bugs
- **No FTTI budget**: 150ms CVC timeout and 200ms SC timeout exceed most FTTIs (SG-001/SG-004: 50ms, SG-008: 100ms)
- **No E2E State Machine**: Simple 3-miss counter instead of window-based INIT/VALID/INVALID evaluation
- **No WdgM checkpoint**: SE 3 configured for heartbeat but `WdgM_CheckpointReached(3)` never called
- **CVC E2E_Protect called with NULL config/state**: Bypasses E2E entirely on CVC heartbeat TX
- **No recovery debounce**: Single heartbeat after timeout → immediate OK (oscillation risk)
- **Tests only on CVC RX path**: FZC/RZC have TX-only tests, no jitter/timing/startup tests

## Status

| Phase | Name | Status |
|-------|------|--------|
| 1 | Derived Constants + Compile-Time Safety | DONE |
| 2 | FTTI Budget Documentation + Timeout Tuning | DONE |
| 3 | WdgM Integration + E2E Fix | DONE |
| 4 | Recovery Debounce + DTC Hardening | DONE |
| 5 | E2E State Machine | DONE |
| 6 | Comprehensive Test Coverage | DONE |
| 7 | Safety Documentation + Traceability | DONE |

---

## Phase 1: Derived Constants + Compile-Time Safety

**Goal**: Eliminate the class of bug where HB_PERIOD_CYCLES doesn't match the actual RTE period.

1. Add `<ECU>_RTE_PERIOD_MS` to each config header:
   - `firmware/cvc/include/Cvc_Cfg.h`: `#define CVC_RTE_PERIOD_MS 10u`
   - `firmware/fzc/include/Fzc_Cfg.h`: `#define FZC_RTE_PERIOD_MS 10u`
   - `firmware/rzc/include/Rzc_Cfg.h`: `#define RZC_RTE_PERIOD_MS 50u`

2. Replace manual `HB_PERIOD_CYCLES` with derived macro + `_Static_assert`:
   ```c
   #define HB_TX_CYCLES  (CVC_HB_TX_PERIOD_MS / CVC_RTE_PERIOD_MS)
   _Static_assert(CVC_HB_TX_PERIOD_MS % CVC_RTE_PERIOD_MS == 0u,
                  "HB TX period must be exact multiple of RTE period");
   ```

3. Replace magic signal IDs `9u`/`11u` in CVC heartbeat RX with named constants

**Files**: `Cvc_Cfg.h`, `Fzc_Cfg.h`, `Rzc_Cfg.h`, all 3 `Swc_Heartbeat.c`
**DONE**: All HB_PERIOD_CYCLES derived, `_Static_assert` present, no magic numbers, `make test` green

---

## Phase 2: FTTI Budget Documentation + Timeout Tuning

**Goal**: Formally prove heartbeat timeouts vs. FTTI, tune where needed.

1. Create `docs/safety/analysis/heartbeat-ftti-budget.md` — per-SG timing breakdown:
   - Document that heartbeat is **complementary** (not primary) for SG-001/SG-003/SG-004 — primary detection is local (pedal plausibility @ 10ms, steering @ 10ms)
   - Document that heartbeat IS primary for SG-008 (system-level SC monitoring) — must comply with 100ms FTTI

2. Add per-ECU timeout thresholds in CVC:
   - `CVC_HB_FZC_MAX_MISS 2u` (100ms — steering/braking ECU, tighter FTTI)
   - `CVC_HB_RZC_MAX_MISS 3u` (150ms — motor ECU, uses local cutoff as primary)

3. Reduce SC timeout: `SC_HB_TIMEOUT_TICKS` 15→10 (100ms), shorten confirmation 50ms→30ms

**Files**: New `heartbeat-ftti-budget.md`, `Cvc_Cfg.h`, `sc_cfg.h`, `Swc_Heartbeat.c` (CVC), `sc_heartbeat.c`
**DONE**: FTTI budget doc complete, SC within 100ms FTTI, per-ECU thresholds, tests verify detection times

---

## Phase 3: WdgM Integration + E2E Fix

**Goal**: Make WdgM alive supervision actually work for heartbeat, fix CVC E2E_Protect NULL call.

1. Add `WdgM_CheckpointReached(3u)` in CVC `Swc_Heartbeat_MainFunction` at TX boundary (every 50ms = 2 calls per 100ms WdgM cycle)
2. Update WdgM SE 3 config from `{3u, 1u, 1u, 3u}` to `{3u, 1u, 3u, 3u}` (expect 1–3 checkpoints per 100ms)
3. Fix CVC heartbeat E2E_Protect: add static `E2E_ConfigType`/`E2E_StateType` with `CVC_E2E_HEARTBEAT_DATA_ID`, replace NULL call

**Files**: `Swc_Heartbeat.c` (CVC), `main.c` (CVC — WdgM config), `test_Swc_Heartbeat_asilc.c`
**DONE**: WdgM checkpoint fires, SE 3 config matches, E2E_Protect uses real config, tests verify

---

## Phase 4: Recovery Debounce + DTC Hardening

**Goal**: Prevent comm status oscillation on noisy bus.

1. Add `CVC_HB_RECOVERY_THRESHOLD 3u` — require 3 consecutive heartbeats before declaring OK
2. Add `fzc_recovery_count`/`rzc_recovery_count` state variables; reset on miss during recovery
3. DTC PASSED only after recovery threshold met
4. Document SC latched behavior with safety rationale in code comments
5. Harden SC confirmation window: require 3 consecutive HBs to cancel (not just 1)

**Files**: `Cvc_Cfg.h`, `Swc_Heartbeat.c` (CVC), `sc_heartbeat.c`, `sc_cfg.h`, both test files
**DONE**: Recovery requires 3 HBs, no single-message oscillation, DTC debounced

---

## Phase 5: E2E State Machine (Professional Pattern)

**Goal**: Replace simple miss counter with proper AUTOSAR-inspired E2E SM.

1. New shared module `firmware/shared/bsw/services/E2E_Sm.h` + `E2E_Sm.c`:
   - States: `E2E_SM_INIT` → `E2E_SM_VALID` → `E2E_SM_INVALID`
   - Sliding window evaluator (circular buffer of OK/FAIL results)
   - Configurable: `WindowSize`, `MinOkStateInit`, `MaxErrorStateValid`, `MinOkStateInvalid`
   - Typical config for 50ms heartbeat: WindowSize=6, MinOkStateInit=3, MaxErrorStateValid=3, MinOkStateInvalid=5

2. Integrate into CVC heartbeat RX: feed E2E check results → SM → comm status
3. Subsumes Phase 4's recovery debounce (window-based recovery replaces counter-based)

**Files**: New `E2E_Sm.h`/`E2E_Sm.c`/`test_E2E_Sm.c`, updated `Swc_Heartbeat.c` (CVC), `Cvc_Cfg.h`
**DONE**: E2E SM with 3 states, window evaluation, CVC uses SM, SIL demo works

---

## Phase 6: Comprehensive Test Coverage

**Goal**: ASIL D test coverage across all heartbeat modules.

| ECU | Current | New Tests | Target |
|-----|---------|-----------|--------|
| CVC | 23 | +12 (jitter, burst, E2E SM, startup race) | 35+ |
| FZC | 20 (TX) | +5 (derived constants, bus-off recovery) | 25+ |
| RZC | 16 (TX) | +6 (derived constants, naming fixes) | 22+ |
| SC | exists | +8 (confirmation edges, per-ECU independence) | expanded |
| E2E SM | 0 | +15 (state transitions, window, edge cases) | 15+ |

Key new categories: jitter injection (±5ms), burst reception, startup synchronization, alive counter rollover (15→0), repeated message detection

**Files**: All `test_Swc_Heartbeat_asilc.c`, new `test_E2E_Sm.c`
**DONE**: All targets met, `make test` green, all tests have `@verifies` tags

---

## Phase 7: Safety Documentation + Traceability

**Goal**: Close all documentation gaps.

1. Update FSC SM-019 (`docs/safety/concept/functional-safety-concept.md`): new timeouts, E2E SM, close FTTI Note 3
2. Update safety goals timing table with actual detection times
3. All new code: `@safety_req` / `@traces_to` tags
4. All new tests: `@verifies` tags
5. New `docs/reference/lessons-learned/infrastructure/PROCESS-heartbeat-hardening.md`

**Files**: `functional-safety-concept.md`, `heartbeat-ftti-budget.md` (finalize), new `PROCESS-heartbeat-hardening.md`
**DONE**: FSC SM-019 updated (E2E SM, per-ECU thresholds, timing table 205ms→140ms, Note 3 closed), FTTI budget traceability updated (E2E SM refs), lessons learned documented (7 key lessons)

---

## Phase Dependencies

```
Phase 1 (Derived Constants)           ← Do first
    ↓
Phase 2 (FTTI Budget) ──────┐
Phase 3 (WdgM + E2E Fix) ──┤        ← Can run in parallel
    ↓                        ↓
Phase 4 (Recovery Debounce)           ← Needs Phase 2 timeout values
    ↓
Phase 5 (E2E State Machine)          ← Subsumes Phase 4 recovery
    ↓
Phase 6 (Test Coverage)              ← After all code changes
    ↓
Phase 7 (Documentation)              ← Final
```

## Key Files

| File | Phases | Role |
|------|--------|------|
| `firmware/cvc/src/Swc_Heartbeat.c` | 1–5 | Core heartbeat TX+RX — most changes |
| `firmware/cvc/include/Cvc_Cfg.h` | 1,2,4,5 | All config constants |
| `firmware/sc/src/sc_heartbeat.c` | 2,4 | SC timeout tuning |
| `firmware/shared/bsw/services/E2E_Sm.c` | 5 | New E2E State Machine |
| `firmware/cvc/test/test_Swc_Heartbeat_asilc.c` | 1–6 | Primary test file |
| `docs/safety/analysis/heartbeat-ftti-budget.md` | 2,7 | New FTTI analysis |
| `docs/safety/concept/functional-safety-concept.md` | 7 | FSC SM-019 update |

## Verification

Per phase: `make test` green, SIL demo stable (no flickering, recovery works after fault injection)

Final:
- Grep for magic numbers: `grep -rn '[0-9]u' firmware/*/src/Swc_Heartbeat.c` — only named constants
- FTTI budget doc reviewed (HITL)
- FSC SM-019 timing matches implementation
- All tests have `@verifies` tags: `grep -c '@verifies' firmware/*/test/test_*Heartbeat*`
