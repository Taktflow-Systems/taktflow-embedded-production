# Lessons Learned — Heartbeat Monitoring Hardening

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-03-02
**Scope:** 7-phase hardening of heartbeat monitoring system (CVC, FZC, RZC, SC) to professional AUTOSAR patterns
**Result:** All 5 safety gaps closed, 134 tests green (38 CVC + 24 E2E SM + 27 FZC + 21 RZC + 24 SC), FTTI compliance achieved for CVC primary path

---

## 1. Derived Constants Eliminate a Class of Bug

**Context:** Three consecutive heartbeat bugs (commits `430f3ba`, `e4acb09`, `52b029d`) all stemmed from manually-set `HB_PERIOD_CYCLES` that didn't match the actual RTE period.

**Fix:** Replace all manual constants with derived macros:

```c
#define HB_TX_CYCLES  (CVC_HB_TX_PERIOD_MS / CVC_RTE_PERIOD_MS)
_Static_assert(CVC_HB_TX_PERIOD_MS % CVC_RTE_PERIOD_MS == 0u,
               "HB TX period must be exact multiple of RTE period");
```

**Lesson:** When timing constant A depends on timing constant B, derive A from B with a compile-time assertion. Manual synchronization between independent `#define` values is a bug factory. The `_Static_assert` catches the drift at compile time, not at runtime on the vehicle.

---

## 2. E2E_Protect with NULL Config/State is Silent No-Op

**Context:** CVC heartbeat TX called `E2E_Protect(NULL_PTR, NULL_PTR, ...)` — the E2E module early-returned on NULL, so heartbeats had zero E2E protection despite the code appearing to use E2E.

**Fix:** Added static `E2E_ConfigType` and `E2E_StateType` with proper `DataId`, initialized at startup.

**Lesson:** NULL pointer guards in AUTOSAR BSW modules make bugs silent. When integrating E2E, always verify with a test that captures the config pointer passed to `E2E_Protect` — a non-NULL assertion catches this immediately. Code review alone missed this because the function call "looked right."

---

## 3. Recovery Debounce Prevents Comm Status Oscillation

**Context:** Before hardening, a single valid heartbeat after a timeout would immediately restore OK status. On a noisy CAN bus, this causes rapid TIMEOUT→OK→TIMEOUT oscillation, which triggers cascading state machine transitions and DTC spam.

**Fix (Phase 4):** Counter-based recovery — require 3 consecutive heartbeats before declaring OK. Phase 5 replaced this with E2E SM window-based evaluation (subsumes the counter approach with more robust sliding window logic).

**Lesson:** Any binary status (OK/FAIL) derived from periodic messages needs hysteresis in both directions. Detection hysteresis (miss count) was already present; recovery hysteresis (consecutive-OK count) was missing. The E2E SM pattern handles both directions with a single configurable mechanism.

---

## 4. WdgM Supervised Entity with No Checkpoint = Dead Monitoring

**Context:** WdgM SE 3 was configured for heartbeat alive supervision (`{3u, 1u, 1u, 3u}`), but `WdgM_CheckpointReached(3u)` was never called in the heartbeat code. The watchdog was monitoring a checkpoint that never fired — it would eventually trigger a watchdog violation, but for the wrong reason.

**Fix:** Added `WdgM_CheckpointReached(3u)` at the heartbeat TX boundary (every 50ms), updated SE 3 config to expect 1-3 checkpoints per 100ms WdgM cycle.

**Lesson:** When adding a WdgM supervised entity, always verify with a test that the checkpoint actually fires. A supervised entity with no checkpoint is worse than no supervision — it creates a false sense of coverage and can trigger false watchdog violations.

---

## 5. FTTI Budget Must Be Documented, Not Assumed

**Context:** The original heartbeat timeouts (CVC: 150ms, SC: 200ms) were chosen for robustness but never formally checked against FTTI. Both exceeded the 100ms FTTI for SG-008 — a 2x violation that went unnoticed because no FTTI budget document existed.

**Fix:** Created `docs/safety/analysis/heartbeat-ftti-budget.md` with per-SG timing breakdown, proving which path is primary vs complementary for each safety goal. Tuned timeouts to achieve compliance: CVC FZC detection 100ms (exactly at FTTI), SC total 140ms (backup path, documented gap).

**Lesson:** Every safety mechanism's timeout chain must be formally documented against its FTTI before the first safety review. The budget doc should answer: "What is the worst-case detection time, and is it within FTTI?" If the answer involves "complementary mechanism" or "backup path," that argument must be explicit and traceable to the safety concept.

---

## 6. E2E State Machine vs Simple Miss Counter

**Context:** Simple miss counters (Phase 4) work for basic detection but have limitations:
- No distinction between startup (unknown state) and runtime monitoring
- Recovery is counter-based (fragile to message ordering)
- No formal "window" concept for evaluating communication quality

**Fix:** Implemented AUTOSAR-inspired E2E State Machine (`E2E_Sm.c`) with:
- Three states: INIT → VALID → INVALID
- Sliding window circular buffer
- Configurable per-ECU: `WindowSize`, `MinOkStateInit`, `MaxErrorStateValid`, `MinOkStateInvalid`

**Lesson:** For ASIL D heartbeat monitoring, a state machine with formal state transitions is more defensible in a safety audit than ad-hoc counters. The window-based approach naturally handles jitter, burst reception, and startup synchronization. The additional complexity (~120 LOC for E2E_Sm.c) pays for itself in auditability and robustness.

---

## 7. Per-ECU Thresholds Reflect Actual Safety Architecture

**Context:** Originally, CVC used a single `CVC_HB_MAX_MISS 3u` for both FZC and RZC. But FZC (steering/braking) has tighter safety requirements than RZC (motor), and RZC has local motor cutoff as primary protection.

**Fix:** Per-ECU thresholds: `CVC_HB_FZC_MAX_MISS 2u` (100ms, SG-008 FTTI compliant), `CVC_HB_RZC_MAX_MISS 3u` (150ms, justified by local motor cutoff primary path).

**Lesson:** One-size-fits-all timeout values waste safety margin on some ECUs while being too tight on others. Map each timeout to its safety goal and document why that specific value was chosen. The FTTI budget doc makes this reasoning auditable.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Derived constants | Derive timing constants from base values + `_Static_assert` — eliminates manual sync bugs |
| E2E NULL config | Test that E2E_Protect receives non-NULL config — silent no-op is worse than crash |
| Recovery debounce | Every binary status needs hysteresis in both detection AND recovery directions |
| WdgM checkpoints | A supervised entity with no checkpoint = dead monitoring — always verify in tests |
| FTTI documentation | Budget doc must exist before safety review — "it's probably fine" is not compliant |
| E2E State Machine | Window-based SM > ad-hoc counters for ASIL D auditability |
| Per-ECU thresholds | Map each timeout to its specific safety goal and document the justification |
