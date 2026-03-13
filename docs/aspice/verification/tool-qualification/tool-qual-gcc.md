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

# Tool Qualification Record: GCC (GNU Compiler Collection)

| Field | Value |
|-------|-------|
| Document ID | TQ-001 |
| Tool | GCC (GNU Compiler Collection) |
| Version | arm-none-eabi-gcc 12.x (target), gcc 13.x (host tests) |
| Supplier | GNU Project / Free Software Foundation |
| Classification | TI2, TD2 --> TCL2 |
| ASIL Context | ASIL D |
| Qualification Method | (1a) Increased confidence from use + (1c) Validation of tool output via testing |
| Date | 2026-02-24 |
| Author | Taktflow Systems |
| Status | Active |

## 1. Tool Description

GCC (GNU Compiler Collection) is an open-source compiler system supporting multiple programming languages and target architectures. In this project, GCC is used in two configurations:

- **arm-none-eabi-gcc 12.x**: Cross-compiler targeting ARM Cortex-M (STM32F4) and ARM Cortex-R (TMS570) microcontrollers. Produces bare-metal firmware binaries for the 4 physical ECUs (CVC, FZC, RZC, SC).
- **gcc 13.x (host)**: Native compiler for x86-64 Linux/POSIX. Compiles unit test binaries, integration test binaries, and SIL test executables. Also used for coverage-instrumented builds (gcov).

GCC translates C99 source code into machine code. As a compiler, it is in the direct path between source code (the verified artifact) and the executable binary (the deployed artifact). Any defect in GCC's code generation could introduce a latent error into the firmware that was not present in the source code.

## 2. Tool Classification

### 2.1 Tool Impact (TI)

**TI2** -- The compiler CAN introduce errors into safety-related work products.

A miscompilation (incorrect code generation, incorrect optimization, incorrect register allocation) could silently produce incorrect behavior in the compiled firmware binary. The source code may be correct and pass all static analysis checks, yet the compiled output could behave differently than the source specifies. This places GCC squarely at TI2 per ISO 26262-8:2018 Table 3.

### 2.2 Tool Error Detection (TD)

**TD2** -- Medium confidence that compiler-introduced errors will be detected.

Detection mechanisms:
- Unit tests execute the compiled binary and detect functional deviations from expected behavior
- MISRA C static analysis (cppcheck) detects undefined behavior and implementation-defined constructs that GCC might exploit during optimization
- Integration tests and SIL tests exercise compiled binaries in system-level scenarios
- Compiler warnings (`-Wall -Wextra -Werror`) catch some code patterns that interact poorly with optimization

However, TD2 (not TD1) because:
- Subtle miscompilation (e.g., incorrect register spill at specific optimization levels) may not be triggered by functional tests
- Tests validate behavior at the interface level but cannot exhaustively verify every generated instruction
- Optimization-related bugs may manifest only under specific data patterns or timing conditions

### 2.3 Tool Confidence Level (TCL)

Per ISO 26262-8:2018 Table 4:

| | TD1 | **TD2** | TD3 |
|---|---|---|---|
| TI1 | TCL1 | TCL1 | TCL1 |
| **TI2** | TCL1 | **TCL2** | TCL3 |

**Result: TCL2** -- Qualification required at medium rigor.

At ASIL D with TCL2, qualification method (1a) is recommended (+) and method (1c) is highly recommended (++). Both are applied.

## 3. Intended Use

GCC is used for the following purposes in this project:

| Use | Compiler | Target | Optimization | Purpose |
|-----|----------|--------|-------------|---------|
| Target firmware (debug) | arm-none-eabi-gcc 12.x | ARM Cortex-M4/R4 | `-Og` | Development and debugging of safety-critical firmware for STM32F4 (CVC, FZC, RZC) and TMS570 (SC) |
| Target firmware (release) | arm-none-eabi-gcc 12.x | ARM Cortex-M4/R4 | `-Os` / `-O2` | Production firmware binaries |
| Host unit tests | gcc 13.x | x86-64 Linux | `-Og -g3` | Compilation and execution of Unity-based unit tests |
| Host integration tests | gcc 13.x | x86-64 Linux | `-Og -g3` | Compilation and execution of integration test binaries |
| Coverage builds | gcc 13.x | x86-64 Linux | `-O0 --coverage` | Instrumented builds for gcov/lcov statement and branch coverage measurement |
| SIL test builds | gcc 13.x | x86-64 Linux | `-Og` | Software-in-the-Loop testing of simulated ECUs (BCM, ICU, TCU) |

Compiler flags applied to all builds:
```
-Wall -Wextra -Werror -std=c99 -pedantic
-Wshadow -Wdouble-promotion -Wformat=2 -Wundef
```

## 4. Known Limitations

| # | Limitation | Description |
|---|-----------|-------------|
| L1 | No formal proof of correctness | GCC has no formal verification of its code generation. The compiler is validated empirically through its own test suite (>200,000 test cases) and decades of widespread use, but theoretical miscompilation is possible. |
| L2 | Optimization-level sensitivity | Higher optimization levels (`-O2`, `-O3`, `-Os`) enable aggressive transformations that have historically been sources of miscompilation bugs. GCC Bugzilla contains reports of incorrect code generation at `-O2`/`-O3` that do not manifest at `-O0`/`-Og`. |
| L3 | Version differences | CI runners (Ubuntu apt) and local developer workstations may have different minor GCC versions. Different versions may generate different code for the same source, and may accept or reject different warning patterns. |
| L4 | Undefined behavior exploitation | GCC is permitted by the C standard to assume undefined behavior does not occur. If MISRA-non-compliant code containing undefined behavior is compiled, GCC may optimize in ways that produce unexpected results. |
| L5 | Cross-compilation target verification gap | The arm-none-eabi-gcc cross-compiler produces ARM binaries that cannot be directly executed on the host. Unit tests validate logic on x86 (host GCC), but do not validate the cross-compiled ARM binary instruction-for-instruction. |

## 5. Risk Assessment

**Primary risk**: Miscompilation could introduce a latent defect in the firmware binary that was not present in the verified source code. This defect could cause incorrect safety function behavior (e.g., wrong sensor threshold comparison, incorrect actuator command calculation, missed watchdog feed).

| Failure Mode | Likelihood | Severity | Detection Method | Residual Risk |
|-------------|------------|----------|-----------------|---------------|
| Incorrect code generation at `-Og`/`-O0` | Very Low | High | Unit tests, integration tests, SIL tests detect functional errors | Low |
| Incorrect code generation at `-O2`/`-Os` (release) | Low | High | Release builds tested against same test suites; back-to-back comparison with debug build | Low |
| Missing/incorrect compiler warning | Low | Medium | MISRA static analysis (cppcheck) provides independent check | Low |
| Compiler crash during build | Very Low | Low | Build failure is immediately detected | Negligible |
| Silent data corruption in output binary | Extremely Low | High | CRC/checksum of build output; end-to-end SIL tests verify system behavior | Low |

**Overall residual risk**: ACCEPTABLE. The combination of MISRA compliance (eliminating undefined behavior that GCC might exploit), comprehensive unit/integration/SIL testing (validating compiled output against requirements), and conservative optimization levels (`-Og` for test builds) provides sufficient confidence.

## 6. Qualification Evidence

| # | Evidence Item | Location | Description |
|---|--------------|----------|-------------|
| E1 | GCC proven-in-use record | https://gcc.gnu.org/ | GCC is the most widely deployed compiler in automotive embedded. Used in Linux kernel, glibc, and by multiple Tier-1 automotive suppliers for both host and target compilation. Active development since 1987. |
| E2 | GCC test suite | https://gcc.gnu.org/testing/ | GCC maintains >200,000 test cases in its own test suite, continuously run across multiple target architectures by the GCC farm. |
| E3 | Unit test results (compiled by GCC) | CI artifact: `test.yml` | All unit tests compiled by GCC and executed. PASS/FAIL results validate compiled output against requirements. |
| E4 | Integration test results | CI artifact: `test.yml` | Integration tests compiled by GCC validate cross-module behavior of compiled binaries. |
| E5 | SIL test results | CI artifact: `sil-test.yml` | End-to-end SIL tests validate complete system behavior of GCC-compiled binaries. |
| E6 | MISRA compliance report | CI artifact: `misra.yml` | Independent static analysis confirms source code avoids undefined behavior that GCC might exploit. |
| E7 | Compiler version log | CI output: `gcc --version`, `arm-none-eabi-gcc --version` | Version recorded in every CI run for traceability. |
| E8 | GCC bug database review | https://gcc.gnu.org/bugzilla/ | No open bugs for `-Og -std=c99` on ARM Cortex-M or x86-64 affecting the code patterns used in this project. |

## 7. Compensating Measures

The following compensating measures reduce the residual risk of GCC miscompilation:

1. **MISRA C compliance**: All firmware source code is MISRA C:2012 compliant (enforced by cppcheck in CI). MISRA compliance eliminates undefined behavior, implementation-defined behavior, and unspecified behavior that GCC might exploit during optimization. This removes the primary attack surface for miscompilation.

2. **Defensive programming**: All safety-critical code follows ISO 26262 Part 6 defensive programming requirements (range checks, plausibility checks, redundant variable storage, program flow monitoring). Even if a single compiled function produces incorrect output, defensive checks in calling code detect the anomaly.

3. **Unit tests on compiled output**: The Unity test suite (88+ test cases) validates the behavior of GCC-compiled binaries against requirements. Tests cover nominal paths, boundary values, error paths, and fault injection scenarios.

4. **Conservative optimization levels**: Test and coverage builds use `-Og` (optimize for debugging) or `-O0` (no optimization), minimizing the risk of optimization-related miscompilation. Release builds use `-Os`/`-O2` but are validated against the same test suites.

5. **Compiler warning escalation**: `-Wall -Wextra -Werror` ensures that all compiler-detected suspicious patterns are treated as build failures. This catches many classes of code that interact poorly with optimization.

6. **Code review**: All firmware source code is reviewed for patterns known to interact poorly with compiler optimization (strict aliasing violations, signed integer overflow, sequence point violations).

7. **Back-to-back build comparison**: Debug (`-Og`) and release (`-Os`/`-O2`) builds are tested against the same test suite. Any behavioral difference indicates a potential miscompilation.

## 8. Use Restrictions

| # | Restriction | Rationale |
|---|------------|-----------|
| R1 | Coverage builds MUST use `-O0` or `-Og` | Higher optimization levels can eliminate branches and merge code paths, producing inaccurate coverage data. |
| R2 | Test builds MUST use `-Og` | Minimizes optimization-related miscompilation risk while maintaining debuggability. |
| R3 | Compiler version changes require re-evaluation | A new GCC major version (e.g., 12.x to 13.x) requires re-running the full test suite and reviewing this qualification record. |
| R4 | No compiler-specific extensions in safety code | GCC-specific `__attribute__`, `__builtin_*`, and inline assembly are prohibited in safety-critical code without formal justification per MISRA Rule 1.2. |
| R5 | `-std=c99 -pedantic` is mandatory | Restricts compilation to the well-defined C99 language subset, preventing reliance on GCC extensions. |
| R6 | arm-none-eabi-gcc target builds require PIL/HIL validation | Host-compiled unit tests validate logic but not target-specific behavior (endianness, alignment, peripheral interaction). Target builds must be validated on actual hardware or via PIL. |

## 9. Anomaly Log

| Date | GCC Version | Anomaly | Impact | Resolution |
|------|-------------|---------|--------|------------|
| -- | -- | No anomalies recorded to date | -- | -- |

This log shall be updated whenever a GCC-related anomaly is discovered during development, testing, or production.

## 10. References

| # | Reference | Description |
|---|-----------|-------------|
| R1 | ISO 26262-8:2018, Clause 11 | Confidence in the use of software tools |
| R2 | IEC 61508-3:2010, Section 7.4.4 | Tool qualification for safety-related development |
| R3 | ISO 26262-6:2018, Section 8.4.6 | Use of language subsets (MISRA C) |
| R4 | GCC Manual | https://gcc.gnu.org/onlinedocs/ |
| R5 | GCC Bug Database | https://gcc.gnu.org/bugzilla/ |
| R6 | MISRA C:2012 / MISRA C:2023 | Coding standard enforced to eliminate undefined behavior |
| R7 | TQ-003 (cppcheck qualification) | Companion tool qualification for MISRA static analysis |
| R8 | TQ-002 (Unity qualification) | Companion tool qualification for unit test framework |
| R9 | TQ-004 (gcov qualification) | Companion tool qualification for coverage measurement |
