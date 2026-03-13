# Lessons Learned — BSW Test-Driven Development

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-21 to 2026-02-22 (Phases 5-6)
**Scope:** Building 16 BSW modules and 7 ECU application layers using strict TDD
**Result:** 195 tests (Phase 5) + 88 tests (Phase 6) = 283 tests, all passing

---

## 1. Test-First Enforcement Hook

A Git hook (`test-first.sh`) blocks writes to any `firmware/*/src/*.c` file if the corresponding `test_*.c` file doesn't exist.

**Effect:** Forces the developer (human or AI) to think about the API before implementing it. The test stub defines the contract.

**Lesson:** Process rules without enforcement are suggestions. Hooks make them hard constraints.

---

## 2. AUTOSAR BSW Layer Ordering

The BSW must be built bottom-up:
```
MCAL (CAN, SPI, ADC, PWM, Dio, Gpt)
  → ECUAL (CanIf, PduR, IoHwAb)
    → Services (Com, Dcm, Dem, WdgM, BswM, E2E)
      → RTE
```

Each layer only depends on layers below it. Violating this creates circular dependencies.

**Lesson:** Build and test bottom-up. Don't start with RTE and work down — start with MCAL and work up.

---

## 3. E2E as Standalone Module

The E2E (End-to-End) protection module was designed with zero AUTOSAR dependencies — pure C functions taking byte arrays and returning CRC/counter results.

**Why:** E2E is used by every ECU including the Safety Controller (which has no AUTOSAR BSW). Making it standalone means one implementation, tested once, used everywhere.

**Lesson:** Cross-cutting concerns should be dependency-free. If it's used by everything, it should depend on nothing.

---

## 4. Table-Driven State Machine for VehicleState

The CVC's vehicle state machine uses a transition table (array of `{current_state, event, next_state, action}` structs) instead of nested switch/case.

**Benefits:**
- All valid transitions visible in one table
- Invalid transitions return an error (no default fall-through)
- Table is testable: iterate all entries and verify
- Adding a state = adding rows, not restructuring logic

**Lesson:** Table-driven state machines are easier to test, review, and modify than switch/case state machines.

---

## 5. Dual Sensor Processing Pattern

The pedal module reads two sensors, compares them for plausibility, and only uses the average if they agree.

```
Read sensor 1 → Read sensor 2 → |diff| < threshold?
  YES → average → torque mapping
  NO  → fault → torque = 0
```

**Test pattern:** Test with (a) both sensors agreeing, (b) sensors disagreeing by exactly threshold, (c) sensors disagreeing by threshold + 1, (d) one sensor failed/timeout.

**Lesson:** Dual-sensor plausibility is the fundamental ASIL D pattern. Test the boundary (threshold ± 1), not just "agree" and "disagree".

---

## 6. Unity Framework with Source Inclusion

The test pattern for every module:
```c
// test_Module.c
#include "unity.h"
// Define mocks BEFORE including source
static uint8_t mock_can_data[8];
Std_ReturnType Com_SendSignal(uint16_t id, const void* data) {
    memcpy(mock_can_data, data, 8);
    return E_OK;
}
// Include the source under test
#include "Module.c"
// Tests
void test_Module_Init(void) { ... }
```

**Why:** No Makefile linking complexity. Each test is one `gcc` command. Static functions are directly testable.

**Lesson:** Source inclusion is the embedded TDD standard. It's not a hack — it's the pattern.

---

## 7. Phase-Based Execution Maintains Momentum

Phase 5 (BSW): 16 modules, 195 tests, completed in one session.
Phase 6 (CVC): 7 modules, 88 tests, completed in one session.

Each phase had a status table with checkboxes. Checking off items provides visible progress.

**Lesson:** For large implementation tasks, phase-based execution with visible progress tracking keeps momentum and prevents scope creep.

---

## 8. Config Files Are Not Testable Code

`firmware/*/cfg/*.c` files (RTE port mappings, COM signal configurations) are static configuration data, not executable logic. They don't need unit tests.

**Lesson:** Test code, not config. Config is verified by integration tests that use the config to run.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Test-first hook | Enforcement > convention; hooks make rules hard constraints |
| Layer ordering | Build bottom-up: MCAL → ECUAL → Services → RTE |
| Standalone modules | Cross-cutting concerns should depend on nothing |
| State machines | Table-driven > switch/case for testability and review |
| Dual sensors | Test the boundary (threshold ± 1), not just agree/disagree |
| Source inclusion | Standard embedded TDD pattern, not a hack |
| Phase execution | Visible progress (checkboxes) maintains momentum |
| Config vs code | Test code, not config; integration tests verify config |
