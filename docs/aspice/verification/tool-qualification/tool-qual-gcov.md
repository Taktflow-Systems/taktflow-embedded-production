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

# Tool Qualification Record: gcov / lcov Coverage Tools

| Field | Value |
|-------|-------|
| Document ID | TQ-004 |
| Tool | gcov (GNU Coverage) + lcov (graphical frontend) |
| Version | gcov (matches GCC version), lcov 1.16+ |
| Supplier | GNU Project / FSF (gcov), Linux Test Project (lcov) |
| Classification | TI2, TD2 --> TCL2 |
| ASIL Context | ASIL D |
| Qualification Method | (1a) Proven-in-use + (1c) Validation via manual coverage spot-check |
| Date | 2026-02-24 |
| Author | Taktflow Systems |
| Status | Active |

## 1. Tool Description

gcov is the GNU Coverage tool, a companion to GCC. When source code is compiled with the `--coverage` flag (equivalent to `-fprofile-arcs -ftest-coverage`), GCC instruments the binary to record which source lines and branches are executed during program execution. After test execution, gcov reads the `.gcda` (runtime data) and `.gcno` (compile-time graph) files to produce per-file coverage reports.

lcov is a graphical frontend for gcov that aggregates coverage data across multiple source files and generates HTML reports with annotated source code (green = covered, red = uncovered). lcov also supports filtering (include/exclude patterns) and merging of coverage data from multiple test runs.

In this project, gcov and lcov are used to measure:
- **Statement (line) coverage**: Percentage of source lines executed by the test suite
- **Branch (decision) coverage**: Percentage of branch outcomes (true/false) taken
- **Function coverage**: Percentage of functions called at least once

Coverage results are used as evidence for ISO 26262 Part 6 Table 9 (structural coverage) and ASPICE SWE.4 (unit verification).

## 2. Tool Classification

### 2.1 Tool Impact (TI)

**TI2** -- The coverage tool CAN fail to detect errors in the verification process.

If gcov/lcov reports incorrect coverage data (specifically, if it reports higher coverage than actually achieved), untested code could pass the coverage gate. Over-reported coverage gives false confidence that the test suite is comprehensive when it is not. Untested code may contain latent defects that go undetected.

### 2.2 Tool Error Detection (TD)

**TD2** -- Medium confidence that coverage reporting errors will be detected.

Detection mechanisms:
- Coverage HTML reports can be manually spot-checked by comparing annotated source lines against known test execution paths
- Known untested code paths (e.g., error handlers not triggered by any test) would appear as coverage gaps in the report, providing a sanity check
- Coverage numbers that increase without new tests being added would indicate an anomaly
- Code review independently assesses test completeness, regardless of coverage metrics

However, TD2 (not TD1) because:
- Branch coverage counting includes compiler-generated branches (ternary operators, short-circuit evaluation, switch fall-through) that inflate the total branch count, making the denominator inaccurate
- Subtle accuracy errors in branch counting (e.g., a branch reported as covered when only one direction was actually taken) are difficult to detect without exhaustive manual review
- Optimization levels above `-O0` can merge or eliminate branches, affecting coverage accuracy

### 2.3 Tool Confidence Level (TCL)

Per ISO 26262-8:2018 Table 4:

| | TD1 | **TD2** | TD3 |
|---|---|---|---|
| TI1 | TCL1 | TCL1 | TCL1 |
| **TI2** | TCL1 | **TCL2** | TCL3 |

**Result: TCL2** -- Qualification required at medium rigor.

At ASIL D with TCL2, qualification method (1a) is recommended (+) and method (1c) is highly recommended (++). Both are applied.

## 3. Intended Use

gcov/lcov are used for the following purposes:

| Use | Description |
|-----|-------------|
| Statement coverage measurement | Percentage of source lines executed by unit and integration tests |
| Branch coverage measurement | Percentage of branch outcomes (true/false) taken during test execution |
| Function coverage measurement | Percentage of functions called at least once |
| HTML report generation | Annotated source code with line-by-line coverage highlighting for human review |
| CI coverage reporting | Coverage summary output in CI logs; planned CI gate enforcement when targets are achieved |
| Coverage evidence for ISO 26262 | Statement and branch coverage metrics serve as evidence for Part 6 Table 9 structural coverage requirements |
| Coverage evidence for ASPICE SWE.4 | Unit verification coverage metrics for ASPICE assessment |

gcov/lcov are NOT used for: MC/DC coverage measurement (addressed separately -- see Section 8), code generation, compilation, test execution, or static analysis.

## 4. Known Limitations

| # | Limitation | Description |
|---|-----------|-------------|
| L1 | No native MC/DC measurement | gcov does not measure Modified Condition/Decision Coverage (MC/DC). ISO 26262 Part 6 Table 9 highly recommends MC/DC at ASIL D. This gap is addressed by: (a) GCC 14+ `-fcondition-coverage` flag for basic condition coverage, and (b) manual MC/DC analysis of complex boolean expressions in safety-critical modules. |
| L2 | Optimization affects accuracy | Optimization levels above `-O0` may cause GCC to merge, eliminate, or reorder branches. This can result in branches being reported as covered or uncovered when the actual execution path differs from the source-level structure. All coverage measurements must be taken at `-O0` or `-Og`. |
| L3 | Compiler-generated branches inflate count | Branch coverage counts include branches generated by the compiler for ternary operators (`?:`), short-circuit evaluation (`&&`, `||`), and switch statement fall-through. These inflated branch counts make it more difficult to achieve high branch coverage percentages and can obscure the true coverage of developer-written branches. |
| L4 | `.gcda` files lost on clean rebuild | gcov's runtime data files (`.gcda`) are generated alongside object files. A `make clean` deletes them. The coverage build target must always perform: clean, build with `--coverage`, run tests, collect `.gcda`, generate report -- as a single atomic sequence. |
| L5 | Single-translation-unit instrumentation | gcov instruments at the translation unit level. If a source file is compiled into multiple test binaries, the `.gcda` files may overwrite each other. lcov's merge capability (`lcov --add-tracefile`) must be used to combine coverage from multiple test executables. |
| L6 | Template/inline function coverage | For C code with `static inline` functions in headers, coverage is attributed to each translation unit that includes the header. This can produce confusing results in the HTML report but does not affect correctness. |

## 5. Risk Assessment

**Primary risk**: Over-reported coverage could give false confidence that untested code is tested. If coverage is reported as 100% when actual coverage is (for example) 85%, the 15% of untested code may contain latent defects that pass the coverage gate undetected.

| Failure Mode | Likelihood | Severity | Detection Method | Residual Risk |
|-------------|------------|----------|-----------------|---------------|
| Over-reporting statement coverage | Low | High | HTML report manual spot-check: compare annotated source against known test execution | Low |
| Over-reporting branch coverage | Low | High | HTML report review per module; focus on complex decision points | Low |
| Under-reporting coverage (conservative) | Low | Low | Conservative error -- results in additional testing, not reduced testing | Negligible |
| Incorrect branch counting (inflated denominator) | Medium | Medium | Document compiler-generated branches as exclusions in coverage report | Low |
| Tool crash / incomplete report | Very Low | Low | CI build failure; missing HTML report immediately detected | Negligible |
| `.gcda` file corruption | Very Low | Medium | Coverage numbers that decrease unexpectedly are investigated; atomic build-run-collect sequence | Low |

**Overall residual risk**: ACCEPTABLE. gcov/lcov provide reliable coverage measurement when used at `-O0`/`-Og`. HTML reports enable human verification. The MC/DC gap is addressed through complementary analysis documented in the MC/DC coverage strategy.

## 6. Qualification Evidence

| # | Evidence Item | Location | Description |
|---|--------------|----------|-------------|
| E1 | Proven-in-use record | https://gcc.gnu.org/ (gcov), https://github.com/linux-test-project/lcov (lcov) | gcov is the standard GCC coverage tool, used by millions of projects. lcov is the standard frontend, used in Linux kernel development. Both tools are mature (gcov since GCC 3.x, lcov since 2002). |
| E2 | Manual coverage spot-check | Performed during Phase 1 coverage baseline | A function with a known untested branch (error handler path) was verified to appear as uncovered (<100% branch coverage) in the gcov/lcov report. Adding a test that exercises the branch increased the coverage, confirming correct instrumentation. |
| E3 | Coverage HTML report | CI artifact: `combined-coverage` | Generated on every CI run. Annotated source code with per-line and per-branch coverage highlighting. |
| E4 | Coverage summary | CI output: `test.yml` workflow log | Statement, branch, and function coverage percentages logged in CI for every build. |
| E5 | Compiler flags (coverage instrumentation) | `firmware/Makefile.posix`, `firmware/shared/bsw/Makefile` | `--coverage` flag applied to coverage builds. `-O0` optimization level confirmed. |
| E6 | Known-untested-path check | Manual validation during Phase 1 | Injected a dead code path (unreachable `if` branch). gcov correctly reported it as 0% covered. Removing the dead code and re-running coverage returned to expected levels. |

## 7. Compensating Measures

The following compensating measures reduce the residual risk of incorrect coverage reporting:

1. **Coverage measured at `-O0` optimization**: All coverage builds use `-O0` (no optimization) to ensure that the source-level control flow structure matches the instrumented binary's execution flow. This eliminates optimization-related coverage accuracy issues.

2. **HTML report manual review**: Coverage HTML reports are reviewed by the developer, focusing on safety-critical modules. The annotated source view (green/red highlighting) enables direct visual verification that reported coverage matches expected test execution.

3. **Boundary value analysis regardless of coverage**: The project testing standard requires boundary value testing for all functions, regardless of the coverage number. Coverage is treated as a minimum gate, not as proof of test completeness.

4. **MC/DC addressed via documented analysis**: For complex boolean expressions in safety-critical modules (3+ conditions), MC/DC coverage is analyzed manually and documented in test reports. The expressions are tagged in source code with `/* MC/DC: N conditions */` comments.

5. **Test count correlation**: Coverage percentage increases should correlate with new test cases being added. Unexplained coverage increases without corresponding new tests are investigated as potential tool anomalies.

6. **Atomic build-run-collect sequence**: The coverage target in the Makefile performs clean, build, test execution, and coverage collection as a single atomic sequence, preventing stale `.gcda` files from corrupting results.

## 8. Use Restrictions

| # | Restriction | Rationale |
|---|------------|-----------|
| R1 | Coverage builds MUST use `-O0` or `-Og` | Higher optimization levels affect coverage accuracy by merging/eliminating branches. |
| R2 | Coverage data MUST be collected from a clean build | Stale `.gcda` files from previous runs corrupt coverage data. Always: clean, build, run, collect. |
| R3 | gcov version MUST match GCC version | gcov and GCC are companion tools. A version mismatch can produce incorrect `.gcno`/`.gcda` interpretation. |
| R4 | Branch coverage numbers require human review | Compiler-generated branches inflate the branch count. Raw branch coverage percentages should be interpreted with awareness of this inflation. |
| R5 | gcov/lcov do NOT measure MC/DC | MC/DC is a separate requirement at ASIL D. MC/DC compliance must be addressed through GCC 14+ `-fcondition-coverage`, manual analysis, or a complementary tool. gcov coverage alone does not satisfy the MC/DC requirement. |
| R6 | lcov `--ignore-errors mismatch` may be needed | When multi-file test binaries have source mapping issues, this flag prevents lcov from aborting. Its use must be documented per build. |
| R7 | Coverage targets are minimum gates, not sufficiency proof | 100% statement/branch coverage does not guarantee correctness. Boundary value analysis, fault injection, and error guessing are required independently per ISO 26262 Part 6. |

## 9. Anomaly Log

| Date | Tool Version | Anomaly | Impact | Resolution |
|------|-------------|---------|--------|------------|
| -- | -- | No anomalies recorded to date | -- | -- |

This log shall be updated whenever a gcov/lcov-related anomaly is discovered during development, testing, or production.

## 10. References

| # | Reference | Description |
|---|-----------|-------------|
| R1 | ISO 26262-8:2018, Clause 11 | Confidence in the use of software tools |
| R2 | IEC 61508-3:2010, Section 7.4.4 | Tool qualification for safety-related development |
| R3 | ISO 26262-6:2018, Table 9 | Structural coverage metrics at ASIL D (statement ++, branch ++, MC/DC ++) |
| R4 | gcov documentation | https://gcc.gnu.org/onlinedocs/gcc/Gcov.html |
| R5 | lcov GitHub | https://github.com/linux-test-project/lcov |
| R6 | GCC `-fcondition-coverage` (GCC 14+) | https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html |
| R7 | TQ-001 (GCC qualification) | gcov is a companion tool to GCC; gcov version must match GCC version |
| R8 | TQ-002 (Unity qualification) | Test framework that drives test execution for coverage measurement |
| R9 | TQ-003 (cppcheck qualification) | Static analysis provides complementary verification independent of coverage |
