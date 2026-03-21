# Development Discipline — MANDATORY

These rules are non-negotiable. They exist because every violation cost hours of debugging.

## 1. Never Hand-Write What Codegen Generates

**Rule:** ECU `main.c` must use `extern` to reference generated config structs. NEVER create `static` local copies of CanIf, PduR, Com, Rte, Dcm, E2E config tables.

**Check before commit:** `grep -rn "static.*ConfigType.*config" firmware/ecu/*/src/main.c` must return ZERO matches for CanIf, PduR, Com config types. Only platform configs (Can_ConfigType, Spi, Adc, Pwm, IoHwAb) and new module configs (CanSM, FiM, Xcp) are allowed as local statics.

**Why:** Hand-written configs drift from generated ones. CVC had 13 PduR entries vs 33 generated. XCP routing was silently broken for an entire session.

## 2. TDD — Tests Before Code

**Rule:** For any non-trivial change (>3 lines), write the test expectation FIRST, see it fail, then fix the code.

**Never adjust test assertions to match broken behavior.** If a test fails, the code is wrong — not the test. The only exception: test reads the wrong byte offset (test bug, not behavior change).

**Why:** TDD caught 2 CanSM bugs (Init null, L1 counter reset) that would have shipped silently.

## 3. Incremental Layer Verification

**Rule:** Follow the 6-layer verification plan. Each layer must pass 100% before starting the next.

```
Layer 1: Unit test (mocked dependencies)
Layer 2: Module test (real module, mocked neighbors)
Layer 3: BSW integration (real stack, mocked CAN driver)
Layer 4: Single ECU (POSIX binary on vcan)
Layer 5: Multi-ECU (2+ binaries on vcan)
Layer 6: Full system (Docker SIL)
```

**Never skip layers.** "It probably works" is not verification.

**Why:** Layer 3 missed the vcan loopback bug. Layer 4 caught it. If we had jumped to Layer 6, the bug would have been buried in Docker complexity.

## 4. Clean Test Environment

**Rule:** Before running integration tests (Layer 4+), always:
```bash
sudo killall -9 cvc_posix fzc_posix rzc_posix 2>/dev/null
```

**Why:** 19 zombie CVC processes on vcan0 inflated frame rates by 13× and wasted 2 hours of debugging.

## 5. Top-Down: HARA → Code

**Rule:** Every code change must trace to a requirement. The chain is:
```
HARA → Safety Goal → TSR → SSR → DBC → ARXML → Codegen → Generated Config → BSW Init → Scheduler → SWC
```

Before changing a transition table, VSM behavior, or fault reaction: check the HARA and TSR first. Research real automotive practice (ISO 26262, Bosch, Waymo) before deciding.

**Why:** The VSM had SC_KILL→SAFE_STOP for months. HARA says SHUTDOWN. Research confirmed it. The fix was 4 lines. Finding it required reading the full requirement chain.

## 6. Plan Before Code

**Rule:** For tasks with 3+ steps, write a plan doc to `docs/plans/` and get approval before coding. Update the plan after each phase.

**Why:** Without a plan, work drifts. With a plan, each step is traceable and reviewable.

## 7. Commit Generated Files Separately

**Rule:** Commit generated configs (`firmware/ecu/*/cfg/`) in a separate commit from hand-written code. Label with `chore(codegen):`.

**Why:** Makes code review possible — reviewer can skip generated file diffs and focus on real changes.

## 8. Kill Debug Traces Before Merge

**Rule:** SIL_DIAG fprintf traces are for investigation only. Remove them before merging to main. Use volatile debug counters (`g_dbg_*`) for permanent instrumentation.

**Why:** Debug traces slow down the binary, change timing behavior, and pollute stderr in production SIL.
