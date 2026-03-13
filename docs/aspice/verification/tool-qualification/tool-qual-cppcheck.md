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

# Tool Qualification Record: cppcheck + MISRA C:2012 Addon

| Field | Value |
|-------|-------|
| Document ID | TQ-003 |
| Tool | cppcheck with MISRA C:2012 addon |
| Version | 2.17.1 (local development), 2.13.0 (CI / Ubuntu apt) |
| Supplier | Daniel Marjamaki (open-source) |
| Classification | TI2, TD2 --> TCL2 |
| ASIL Context | ASIL D |
| Qualification Method | (1a) Increased confidence from use + (1c) Validation via known-violation injection |
| Date | 2026-02-24 |
| Author | Taktflow Systems |
| Status | Active |

## 1. Tool Description

cppcheck is an open-source static analysis tool for C and C++ code. It performs general static analysis (null pointer dereference, buffer overrun, memory leak, uninitialized variable, etc.) and, with its MISRA addon, checks compliance against the MISRA C:2012 coding standard (including Amendments 1 and 2, covering ~150+ of 175 rules).

In this project, cppcheck with the MISRA addon is the primary automated MISRA compliance checking tool. It runs in two environments:

- **CI (GitHub Actions)**: cppcheck 2.13.0 from Ubuntu apt, configured with `--error-exitcode=1` (blocking). The CI workflow (`.github/workflows/misra.yml`) runs on every push and pull request. Any MISRA violation that is not suppressed causes the build to fail.
- **Local development**: cppcheck 2.17.1 installed via pip. Used by developers for pre-commit MISRA checking.

Configuration files:
- Rule definitions: `tools/misra/misra.json`
- Approved suppressions: `tools/misra/suppressions.txt`

## 2. Tool Classification

### 2.1 Tool Impact (TI)

**TI2** -- cppcheck CAN fail to detect errors in safety-related work products.

Specifically, cppcheck could produce a false negative: failing to report a genuine MISRA C:2012 violation. If the tool misses a violation, non-compliant code could enter the firmware without being flagged. MISRA violations often correspond to undefined behavior, implementation-defined behavior, or error-prone constructs. Undetected violations could lead to latent defects in safety-critical code.

### 2.2 Tool Error Detection (TD)

**TD2** -- Medium confidence that tool errors (false negatives) will be detected.

Detection mechanisms:
- Code review against MISRA rules provides an independent human check
- Compiler warnings (`-Wall -Wextra -Werror`) catch some of the same issues MISRA rules target
- Defensive programming practices (range checks, plausibility checks) provide runtime detection of defects that a missed MISRA violation might enable

However, TD2 (not TD1) because:
- cppcheck does not cover all 175 MISRA C:2012 rules (covers ~150+)
- Inter-file analysis has known limitations (Rule 8.5 false positives documented)
- Version differences between 2.13 (CI) and 2.17 (local) may cause some violations to be detected in one version but not the other
- cppcheck does not perform data flow analysis as deeply as commercial tools (Polyspace, QA-C, PC-lint)
- A missed violation in a non-obvious rule (e.g., implicit type conversion) may not be caught by code review

### 2.3 Tool Confidence Level (TCL)

Per ISO 26262-8:2018 Table 4:

| | TD1 | **TD2** | TD3 |
|---|---|---|---|
| TI1 | TCL1 | TCL1 | TCL1 |
| **TI2** | TCL1 | **TCL2** | TCL3 |

**Result: TCL2** -- Qualification required at medium rigor.

At ASIL D with TCL2, qualification method (1a) is recommended (+) and method (1c) is highly recommended (++). Both are applied.

## 3. Intended Use

cppcheck with MISRA addon is used for the following purposes:

| Use | Description |
|-----|-------------|
| MISRA C:2012 compliance checking | Automated analysis of all firmware source files (`firmware/**/*.c`, `firmware/**/*.h`) against MISRA C:2012 rules |
| CI-blocking static analysis | `--error-exitcode=1` in CI workflow; any unsuppressed violation fails the build |
| MISRA compliance report generation | Output serves as evidence artifact for safety case and ASPICE SWE.4 |
| General static analysis | In addition to MISRA, cppcheck detects null dereference, buffer overrun, memory leak, and other defect classes |
| Deviation management | Suppressions file (`tools/misra/suppressions.txt`) manages approved deviations documented in the deviation register |

cppcheck is NOT used for: code generation, compilation, test execution, coverage measurement, or data flow analysis.

## 4. Known Limitations

| # | Limitation | Description |
|---|-----------|-------------|
| L1 | Incomplete MISRA rule coverage | cppcheck covers ~150+ of the 175 MISRA C:2012 rules (with Amendments). Rules requiring deep semantic analysis, inter-procedural data flow, or complex type resolution may not be checked. |
| L2 | False positives on inter-file analysis (Rule 8.5) | cppcheck reports false positive Rule 8.5 violations on configuration/header cross-references. These are globally suppressed with documented rationale. |
| L3 | Version differences (2.13 vs 2.17) | CI runs cppcheck 2.13 (Ubuntu apt), local development uses 2.17.1 (pip). Known behavioral differences: (a) 2.13 cannot parse `#` comments in suppressions.txt; (b) 2.13 triggers error-exitcode on style findings that 2.17 does not; (c) some rules have improved detection in 2.17 that is absent in 2.13. |
| L4 | `misra-config` error on POSIX ioctl constants | POSIX-specific constants trigger `misra-config` errors in POSIX hardware abstraction files. Suppressed for `*_hw_posix.c` files. |
| L5 | No deep data flow analysis | cppcheck does not perform inter-procedural data flow analysis as deeply as commercial tools (Polyspace, QA-C). Complex data flow paths involving multiple function calls may not be fully analyzed. |
| L6 | Style findings on error-exitcode (2.13) | cppcheck 2.13 treats some style-category findings as errors when `--error-exitcode=1` is set. These are explicitly suppressed in `suppressions.txt`. |
| L7 | Single-translation-unit analysis | cppcheck primarily analyzes one translation unit at a time. Cross-translation-unit issues (e.g., inconsistent function declarations across files) may not be detected. |

## 5. Risk Assessment

**Primary risk**: A false negative (missed MISRA violation) could allow non-compliant code to enter the firmware. If the missed violation corresponds to undefined behavior, the compiler (GCC) could exploit it during optimization, producing incorrect code.

| Failure Mode | Likelihood | Severity | Detection Method | Residual Risk |
|-------------|------------|----------|-----------------|---------------|
| False negative (missed MISRA violation) | Medium | Medium | Code review against MISRA rules; compiler warnings catch overlapping issues | Low |
| False positive (spurious violation reported) | Low | Low | Suppression with documented rationale in suppressions.txt | Negligible |
| Tool crash during analysis | Very Low | Low | CI build fails, immediately detected; re-run resolves transient issues | Negligible |
| Version inconsistency (different results in CI vs local) | Low | Low | CI is authoritative; local results are advisory. Both versions logged. | Low |
| Suppression masking a real violation | Low | Medium | Deviation register requires justification for every suppression; independent review of suppression list | Low |

**Overall residual risk**: ACCEPTABLE. cppcheck provides the primary automated MISRA check. Its gaps are covered by code review, compiler warnings, defensive programming, and unit testing.

## 6. Qualification Evidence

| # | Evidence Item | Location | Description |
|---|--------------|----------|-------------|
| E1 | Proven-in-use record | https://github.com/danmar/cppcheck | cppcheck is used by thousands of projects including automotive. Active development since 2007. >5,600 GitHub stars. Used by OEMs and Tier-1 suppliers for MISRA checking. |
| E2 | Known-violation injection test | Validation during MISRA pipeline setup | Known MISRA violations were deliberately injected into source code. cppcheck correctly detected and reported them. Clean code (post-fix) produced 0 violations. |
| E3 | MISRA compliance report (0 violations) | CI artifact: `misra-report.txt` | Current codebase passes cppcheck MISRA analysis with 0 unsuppressed violations. |
| E4 | Suppression validation | Validation during pipeline setup | Deviation-suppressed code (DEV-001, DEV-002) is properly suppressed and does not trigger violations. Removing suppressions causes the expected violations to reappear. |
| E5 | CI vs local consistency check | Validation during pipeline setup | Both cppcheck 2.13 (CI) and 2.17 (local) produce consistent results on the clean codebase, with documented workarounds for version-specific differences. |
| E6 | Tool configuration | `tools/misra/misra.json` | MISRA rule definitions configured for C:2012 with Amendments. |
| E7 | Suppression list | `tools/misra/suppressions.txt` | All suppressions correspond to documented deviations or version-specific workarounds. |
| E8 | Deviation register | `docs/safety/analysis/misra-deviation-register.md` | DEV-001 (Rule 11.5) and DEV-002 (Rule 11.8) formally documented with justification, risk assessment, and compensating measures. |
| E9 | CI workflow | `.github/workflows/misra.yml` | cppcheck runs on every push/PR with `--error-exitcode=1` (blocking). |

## 7. Compensating Measures

The following compensating measures address cppcheck's known limitations:

1. **Code review against MISRA rules**: All firmware code is reviewed by a developer familiar with MISRA C:2012. The review checklist includes rules that cppcheck is known to not cover (inter-procedural rules, complex type analysis).

2. **Compiler warnings as independent check**: GCC with `-Wall -Wextra -Werror` catches many of the same defect classes that MISRA rules target (implicit conversions, unused variables, format string issues, shadowed variables). This provides an independent detection mechanism for issues that cppcheck might miss.

3. **Defensive programming standards enforced**: The project enforces ISO 26262 Part 6 defensive programming (range checks, plausibility checks, program flow monitoring). Even if a MISRA violation is missed and results in a subtle code defect, defensive checks provide runtime detection.

4. **Unit testing of compiled output**: The Unity test suite validates the behavior of the compiled code. Even if non-MISRA-compliant code passes static analysis, functional testing detects behavioral defects.

5. **Dual-version cross-check**: Running cppcheck on both 2.13 (CI) and 2.17 (local) provides a degree of cross-validation. A violation detected by one version but not the other is investigated.

6. **Deviation register with independent review**: Every suppression in `suppressions.txt` either corresponds to a formal deviation in the deviation register (reviewed independently) or to a documented version-specific workaround.

## 8. Use Restrictions

| # | Restriction | Rationale |
|---|------------|-----------|
| R1 | CI result is authoritative | When CI (2.13) and local (2.17) disagree, the CI result is the official compliance status. |
| R2 | No `#` comments in suppressions.txt | cppcheck 2.13 cannot parse `#` comments. Use only the `//` syntax or no comments. |
| R3 | Every suppression must be justified | No blanket suppressions. Each entry in `suppressions.txt` must correspond to a deviation register entry or a documented version-specific workaround. |
| R4 | Version upgrades require re-evaluation | Upgrading cppcheck (especially across major versions) requires: re-running analysis, reviewing new/changed findings, updating suppressions, and updating this qualification record. |
| R5 | cppcheck alone is not sufficient for MISRA compliance | cppcheck covers ~150+ of 175 rules. The remaining rules must be verified by code review or by a complementary tool. |
| R6 | `.gitignore` must have exception for suppressions.txt | The `.gitignore` pattern `tools/misra/*.txt` blocks `suppressions.txt`. A `!tools/misra/suppressions.txt` exception is required. |

## 9. Anomaly Log

| Date | cppcheck Version | Anomaly | Impact | Resolution |
|------|-----------------|---------|--------|------------|
| 2026-02-24 | 2.13.0 | `#` comments in suppressions.txt cause parse failure | CI fails to load suppressions, reports false violations | Removed `#` comments from suppressions.txt |
| 2026-02-24 | 2.13.0 | Style findings trigger `--error-exitcode=1` | False build failures on style-category findings | Style-category findings explicitly suppressed |
| 2026-02-24 | 2.13.0 | Rule 8.5 false positives on cfg/header cross-references | False violations on valid configuration patterns | Globally suppressed for affected files with documented rationale |
| 2026-02-24 | 2.13.0 | `misra-config` error on POSIX ioctl constants | False violations in POSIX HAL files | Suppressed for `*_hw_posix.c` files |

## 10. References

| # | Reference | Description |
|---|-----------|-------------|
| R1 | ISO 26262-8:2018, Clause 11 | Confidence in the use of software tools |
| R2 | IEC 61508-3:2010, Section 7.4.4 | Tool qualification for safety-related development |
| R3 | ISO 26262-6:2018, Section 8.4.6 | Use of language subsets (MISRA C) |
| R4 | MISRA C:2012 (with Amendments 1 and 2) | Coding standard being enforced |
| R5 | MISRA Compliance:2020 | Guidelines for MISRA compliance claims and deviation documentation |
| R6 | cppcheck GitHub | https://github.com/danmar/cppcheck |
| R7 | cppcheck MISRA addon documentation | https://cppcheck.sourceforge.io/misra.php |
| R8 | Project MISRA pipeline plan | `docs/plans/plan-misra-pipeline.md` |
| R9 | MISRA deviation register | `docs/safety/analysis/misra-deviation-register.md` |
| R10 | TQ-001 (GCC qualification) | Compiler warnings provide complementary MISRA coverage |
