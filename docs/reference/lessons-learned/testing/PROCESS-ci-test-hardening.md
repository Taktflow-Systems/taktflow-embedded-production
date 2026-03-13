# Lessons Learned — CI Test Hardening

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-22 to 2026-02-28
**Scope:** Getting 1,000+ unit tests to pass on CI (host x86_64) after developing on target (ARM32)
**Result:** 8 rounds of CI fixes, 99+ failures resolved

---

## 1. LP64 vs ILP32: uint32 Is Not unsigned int on 64-bit

The firmware defines `uint32` as `unsigned long` (AUTOSAR Platform_Types). On ARM32 (ILP32), `unsigned long` = 32 bits. On x86_64 Linux CI (LP64), `unsigned long` = 64 bits.

This causes `printf` format warnings (`%u` expects `unsigned int`, not `unsigned long`) and `TEST_ASSERT_EQUAL` size mismatches.

**Fix:** Use `uint32_t` from `<stdint.h>` in test code, or cast explicitly.

**Lesson:** AUTOSAR platform types assume 32-bit targets. Host-based unit tests need a compatibility shim or explicit casts.

---

## 2. Source Inclusion vs Linking for Unity Tests

Embedded test pattern: `#include "module_under_test.c"` directly in the test file, rather than compiling and linking separately.

**Why it works:**
- No linker — test file is self-contained
- Can test static functions
- Mock dependencies by defining them before the include
- CI just needs `gcc test_module.c -o test_module && ./test_module`

**Pitfall:** If two test files include the same source, symbols conflict. Each test file must be compiled independently.

**Lesson:** Source inclusion is the standard embedded TDD pattern. Embrace it — it eliminates linker configuration entirely.

---

## 3. Header Guard Collisions Across Test Files

Multiple test files defining `#define DEM_H` or `#define E2E_H` to mock those modules. If test A and test B both define `DEM_H` with different mock implementations, compilation in a combined build fails.

**Fix:** Each test file uses source inclusion and is compiled independently. Never combine test files into a single build unit.

---

## 4. Mock State Must Be Cleared in setUp

Unity's `setUp()` runs before each test. If mock global state (call counts, last arguments) isn't cleared, tests pass individually but fail when run together.

**Pattern:**
```c
void setUp(void) {
    memset(&mock_com_state, 0, sizeof(mock_com_state));
    // Reset ALL mock globals
}
```

**Lesson:** Every mock variable must be zeroed in setUp. No exceptions.

---

## 5. lcov --remove Patterns Need Exact Match

`lcov --remove` uses glob patterns. `*/test/*` removes test files. But `*/test_*.c` does not match `firmware/cvc/test/test_Swc_Pedal.c` if the pattern is anchored wrong.

**Fix:** Use `'*/test/*'` (directory match) not `'*test_*'` (filename match).

---

## 6. AUTOSAR API Confusion: 2-param vs 3-param

AUTOSAR `Com_SendSignal` takes 2 parameters: `(SignalId, DataPtr)`. Some code added a 3rd length parameter (common in non-AUTOSAR APIs). This compiled locally but failed on stricter CI flags.

**Lesson:** Always check the AUTOSAR API signature. The signal length is defined in configuration, not passed at runtime.

---

## 7. Type Signedness Mismatches Surface on Host

Steering angle is `sint16` (signed) but `IoHwAb_ReadSteeringAngle` returned `uint16*`. On ARM32, the implicit cast worked. On x86_64 with `-Werror`, it fails.

**Lesson:** Host builds with `-Wall -Werror` catch type safety issues that target builds silently accept. This is a feature, not a bug.

---

## 8. Batch Fix Strategy for 99+ Failures

When CI showed 99 test failures after the first integration:

1. **Group by error type**, not by file — most failures share a root cause
2. **Fix the most common error first** — often fixes 30+ tests at once
3. **Re-run after each group fix** — the count drops dramatically
4. **Don't fix one-by-one** — look for patterns

99 failures → 52 → 23 → 5 → 0 in 4 rounds.

**Lesson:** Batch by root cause, not by file.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| LP64 vs ILP32 | AUTOSAR platform types assume 32-bit; shim for host tests |
| Source inclusion | Standard embedded TDD pattern; eliminates linker config |
| Mock cleanup | Zero ALL mock state in setUp() — no exceptions |
| API signatures | Check AUTOSAR spec, not intuition |
| Host as lint | `-Wall -Werror` on host catches real bugs hidden on target |
| Batch strategy | Group 99 failures by root cause, fix in 4 rounds not 99 |
