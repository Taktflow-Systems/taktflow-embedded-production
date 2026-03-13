## Human-in-the-Loop (HITL) Comment Lock

`HITL` means human-reviewer-owned comment content.

**Marker standard (code-friendly):**
- Markdown: `<!-- HITL-LOCK START:<id> -->` ... `<!-- HITL-LOCK END:<id> -->`
- C/C++/Java/JS/TS: `// HITL-LOCK START:<id>` ... `// HITL-LOCK END:<id>`
- Python/Shell/YAML/TOML: `# HITL-LOCK START:<id>` ... `# HITL-LOCK END:<id>`

**Rules:**
- AI must never edit, reformat, move, or delete text inside any `HITL-LOCK` block.
- Append-only: AI may add new comments/changes only; prior HITL comments stay unchanged.
- If a locked comment needs revision, add a new note outside the lock or ask the human reviewer to unlock it.

# Tool Qualification Record: Unity Test Framework

| Field | Value |
|-------|-------|
| Document ID | TQ-002 |
| Tool | Unity Test Framework |
| Version | 2.6.0 (vendored) |
| Supplier | ThrowTheSwitch.org (Mark VanderVoord, open-source) |
| Classification | TI2, TD1 --> TCL1 |
| ASIL Context | ASIL D |
| Qualification Method | (1a) Increased confidence from use + cross-validation via expected-fail tests |
| Date | 2026-02-24 |
| Author | Taktflow Systems |
| Status | Active |

## 1. Tool Description

Unity is a lightweight, portable unit test framework for C. It provides assertion macros (`TEST_ASSERT_EQUAL`, `TEST_ASSERT_TRUE`, `TEST_ASSERT_EQUAL_HEX8`, `TEST_ASSERT_FLOAT_WITHIN`, etc.) and a test runner infrastructure (`UNITY_BEGIN`, `RUN_TEST`, `UNITY_END`) for organizing and executing test cases.

Unity is vendored (committed directly to the repository) at `firmware/shared/bsw/test/unity/`. The vendored copy is a single-header, single-source implementation (~2,500 lines of C) with no external dependencies.

In this project, Unity is used as the sole unit test execution framework for all firmware modules across all 7 ECUs (CVC, FZC, RZC, SC, BCM, ICU, TCU) and all 18+ BSW modules. Every `@verifies` tag in a test file relies on Unity's assertion macros to deliver a correct PASS/FAIL verdict.

## 2. Tool Classification

### 2.1 Tool Impact (TI)

**TI2** -- The test framework CAN fail to detect errors in the software under test.

If a Unity assertion macro contains a bug (e.g., `TEST_ASSERT_EQUAL` returns PASS when the values are not equal), a defective function could pass its test and the defect would go undetected. This is a "false PASS" failure mode: the tool fails to detect an error that exists in the safety work product.

### 2.2 Tool Error Detection (TD)

**TD1** -- High confidence that tool errors will be detected.

Rationale:
- Unity's assertion macros are simple, transparent comparisons. `TEST_ASSERT_EQUAL(expected, actual)` is fundamentally `if (expected != actual) { FAIL(); }`. The logic is trivially inspectable.
- A faulty assertion would cause visible, reproducible effects: either tests that should PASS would FAIL (immediately noticed by developers), or tests that should FAIL would PASS (caught by expected-fail validation and test result review).
- Test output is directly observable: Unity prints each test name with PASS/FAIL/IGNORED status and a total summary line (`X Tests Y Failures Z Ignored`). Any anomaly is immediately visible.
- Unity's own test suite validates assertion correctness across platforms.
- The same tests produce the same results across GCC versions and across platforms (Linux, Windows MSYS2), providing cross-validation.

### 2.3 Tool Confidence Level (TCL)

Per ISO 26262-8:2018 Table 4:

| | **TD1** | TD2 | TD3 |
|---|---|---|---|
| TI1 | TCL1 | TCL1 | TCL1 |
| **TI2** | **TCL1** | TCL2 | TCL3 |

**Result: TCL1** -- No formal qualification required.

**Note for assessors**: Some certification bodies classify test frameworks at TCL2, arguing that TD1 is too optimistic because a subtle assertion bug in a complex macro (e.g., float comparison with epsilon) could produce undetected false PASSes. This qualification record is maintained at full rigor regardless of the TCL1 classification, to satisfy both interpretations. If the assessor requires TCL2, qualification method (1a) + (1c) as documented below meets TCL2 requirements at ASIL D.

## 3. Intended Use

Unity is used for the following purposes:

| Use | Scope | Description |
|-----|-------|-------------|
| Unit test assertion | All firmware modules | `TEST_ASSERT_EQUAL`, `TEST_ASSERT_TRUE`, `TEST_ASSERT_NULL`, `TEST_ASSERT_EQUAL_HEX8`, `TEST_ASSERT_FLOAT_WITHIN` |
| Test runner | All test executables | `UNITY_BEGIN()` / `RUN_TEST(test_func)` / `UNITY_END()` orchestrates test execution and reports results |
| Test verdict reporting | CI pipeline | Unity's exit code (0 = all pass, non-zero = failures) is used as CI gate |
| BSW module tests | 18+ BSW modules | All AUTOSAR-like BSW components tested via Unity |
| ECU SWC tests | 7 ECUs | All ECU-specific software components tested via Unity |
| Integration tests | Cross-module | Integration test binaries use Unity for assertion and reporting |

Unity is NOT used for: mocking (manual mocks are used; CMock available if needed), code generation, compilation, static analysis, or coverage measurement.

## 4. Known Limitations

| # | Limitation | Description |
|---|-----------|-------------|
| L1 | Single-threaded only | Unity does not support concurrent test execution. All tests run sequentially in a single thread. This is acceptable for this project because the test suite completes in <60 seconds. |
| L2 | No built-in mocking | Unity provides assertion macros only, not mock/stub generation. Mock functions are written manually in test files. CMock (companion tool from ThrowTheSwitch.org) is available if needed. |
| L3 | No automatic test discovery | Tests must be explicitly registered in `main()` via `RUN_TEST()` calls. If a test function exists but is not registered, it will not execute. This is mitigated by code review and by convention (every `test_*` function must appear in the `RUN_TEST()` list). |
| L4 | Float comparison uses epsilon | `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)` requires an explicit tolerance parameter. If the tolerance is set too loosely, precision errors could be masked. |
| L5 | No built-in timeout | If a test function enters an infinite loop, the test runner hangs. CI timeout (2-minute limit) provides external detection. |

## 5. Risk Assessment

**Primary risk**: A bug in Unity's assertion macros could cause a "false PASS" -- a test that should fail instead reports success, allowing a defective function to pass verification.

| Failure Mode | Likelihood | Severity | Detection Method | Residual Risk |
|-------------|------------|----------|-----------------|---------------|
| Assertion macro returns wrong verdict (false PASS) | Extremely Low | High | Expected-fail tests validate assertions work correctly; code review of test results; cross-platform validation | Negligible |
| Assertion macro returns wrong verdict (false FAIL) | Extremely Low | Low | Immediately noticed by developer (unexpected failure on known-good code) | Negligible |
| Test runner skips a registered test | Very Low | Medium | Test count in output summary (`X Tests`) compared against expected count | Negligible |
| Test runner skips an unregistered test | Low | Medium | Code review ensures all `test_*` functions are in `RUN_TEST()` list | Low |
| Memory corruption in framework code | Extremely Low | Medium | AddressSanitizer enabled in debug/test builds (`-fsanitize=address`) | Negligible |

**Overall residual risk**: ACCEPTABLE. Unity's assertion logic is trivially simple and transparent. The false-PASS failure mode requires the fundamental comparison operator (`!=`, `<`, `>`) to be wrong, which is a compiler defect (covered by TQ-001 GCC qualification), not a Unity defect.

## 6. Qualification Evidence

| # | Evidence Item | Location | Description |
|---|--------------|----------|-------------|
| E1 | Unity source code (vendored, reviewed) | `firmware/shared/bsw/test/unity/` | Complete Unity 2.6.0 source committed to repository. Code review confirmed assertion macros are straightforward comparisons. |
| E2 | Proven-in-use record | https://github.com/ThrowTheSwitch/Unity | Unity is the most widely used C unit test framework in embedded systems. Active development since 2007. >3,500 GitHub stars. Used by automotive, medical device, and aerospace companies. |
| E3 | Unit test results | CI artifact: `test.yml` | All tests compiled and executed by CI on every commit. PASS/FAIL results recorded. |
| E4 | Test count validation | CI output: `X Tests Y Failures Z Ignored` | Total test count verified against expected count in CI. |
| E5 | Expected-fail cross-validation | Test files contain intentional assertion tests | Tests with known-incorrect expected values confirm that Unity correctly reports FAIL. Tests with known-correct values confirm PASS. |
| E6 | Cross-platform consistency | CI (Ubuntu x86-64), local (Windows MSYS2) | Same test source produces same PASS/FAIL results across platforms, confirming Unity's behavior is platform-independent. |

## 7. Compensating Measures

1. **Expected-fail test cases**: The test suite includes deliberately incorrect assertions to validate that Unity correctly detects and reports failures. If Unity's assertion macros were broken (false PASS), these expected-fail tests would also falsely pass, which would be detected during test result review.

2. **Developer review of test results**: Test output is reviewed by the developer who wrote the tests. Unexpected PASS or FAIL results are investigated. This human review catches anomalies that automated CI cannot.

3. **Test count reconciliation**: The total number of tests reported by Unity (`X Tests`) is compared against the expected count. A missing or skipped test would cause a count mismatch.

4. **AddressSanitizer in test builds**: Test binaries are compiled with `-fsanitize=address,undefined` to detect memory corruption within Unity itself or within the code under test.

5. **Vendored source with diff tracking**: Unity is vendored (not fetched at build time). Any modification to the Unity source would appear as a diff in version control and would be reviewed.

## 8. Use Restrictions

| # | Restriction | Rationale |
|---|------------|-----------|
| R1 | All `test_*` functions MUST be registered in `RUN_TEST()` | Unity has no auto-discovery. An unregistered test will not execute, creating a coverage gap. |
| R2 | Float comparisons MUST use `TEST_ASSERT_FLOAT_WITHIN` with explicit tolerance | Default equality comparison for floats (`TEST_ASSERT_EQUAL`) is inappropriate due to floating-point precision. |
| R3 | Unity version changes require diff review | Any update to the vendored Unity source must be reviewed for changes to assertion logic. |
| R4 | CI must enforce non-zero exit code on failure | Unity returns non-zero from `UNITY_END()` when failures exist. CI must treat this as a build failure. |
| R5 | Each test function must have a `@verifies` tag | Per project testing standard, every test traces to a requirement ID (SSR, SWR, HSR). |

## 9. Anomaly Log

| Date | Unity Version | Anomaly | Impact | Resolution |
|------|---------------|---------|--------|------------|
| -- | -- | No anomalies recorded to date | -- | -- |

This log shall be updated whenever a Unity-related anomaly is discovered during development, testing, or production.

## 10. References

| # | Reference | Description |
|---|-----------|-------------|
| R1 | ISO 26262-8:2018, Clause 11 | Confidence in the use of software tools |
| R2 | IEC 61508-3:2010, Section 7.4.4 | Tool qualification for safety-related development |
| R3 | ISO 26262-6:2018, Table 7 | Unit testing methods at ASIL D |
| R4 | Unity GitHub | https://github.com/ThrowTheSwitch/Unity |
| R5 | Unity Documentation | https://github.com/ThrowTheSwitch/Unity/tree/master/docs |
| R6 | ThrowTheSwitch.org | https://www.throwtheswitch.org/ |
| R7 | TQ-001 (GCC qualification) | Compiler that builds Unity and test binaries |
| R8 | TQ-004 (gcov qualification) | Coverage measurement of test execution |
| R9 | Project testing standard | `.claude/rules/testing.md` -- test naming, coverage targets, TDD mandate |
