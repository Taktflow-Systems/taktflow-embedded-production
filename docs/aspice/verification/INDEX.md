---
document_id: VER-INDEX
title: "Verification Documentation Index"
version: "0.2"
status: draft
aspice_processes: "SWE.4, SWE.5, SWE.6, SYS.4, SYS.5"
date: 2026-07-06
---

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

## Lessons Learned Rule

Every verification topic in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/lessons-learned/`](../lessons-learned/). One file per verification topic. File naming: `VER-<topic>.md`.

# Verification Documentation Index

Right side of the V-model — all verification and validation evidence.

## Current Verification Readiness

This index reflects the 2026-07-06 evidence review. SWE.4 and SWE.5 have
executed evidence. SWE.6, SYS.4, SYS.5, and HIL remain release gaps until their
test specifications, baselines, logs, and reports are completed.

| Area | Evidence Strength | Release Gap |
|------|-------------------|-------------|
| SWE.4 unit verification | Unit plan and report are approved; recorded report shows 1,459/1,459 tests passing and no failed tests. | Attach refreshed coverage/static-analysis output to the release baseline; MC/DC remains pending. |
| SWE.5 integration verification | Integration strategy, plan, and report exist; recorded report shows 60/60 passing integration tests. | Ensure inter-ECU/SIL integration evidence is tied to the same baseline as the software release. |
| SWE.6 software qualification | Verification plan and release notes exist as stubs. | Add SW qualification test specification and report, including release acceptance criteria. |
| SYS.4 system integration | System integration report exists as a stub. | Add system integration strategy/specification and fill ECU/gateway/CAN interface results. |
| SYS.5 system verification | System verification report exists as a stub. | Add system verification plan/specification and link safety validation evidence. |
| xIL | MIL/SIL/PIL/HIL report files exist; SIL upstream is active but scenario evidence is not complete. | Record scenario-level logs, build baseline, pass/fail, CAN traces, and HIL execution results. |

<!-- HITL-LOCK START:COMMENT-BLOCK-VER-SEC1 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The index correctly positions itself as the right side of the V-model. The frontmatter correctly lists SWE.4, SWE.5, SWE.6, SYS.4, SYS.5 as the covered ASPICE processes. All document statuses are "Planned" which is consistent with the current project phase. No issues with the introduction.
<!-- HITL-LOCK END:COMMENT-BLOCK-VER-SEC1 -->

## Unit Verification (SWE.4)

| Document | Path | Status |
|----------|------|--------|
| Unit Test Plan | [unit-test/unit-test-plan.md](unit-test/unit-test-plan.md) | Approved |
| Unit Test Report | [unit-test/unit-test-report.md](unit-test/unit-test-report.md) | Approved / refresh for current baseline |
| Static Analysis and MISRA Evidence | [unit-test/unit-test-report.md](unit-test/unit-test-report.md), [tool-qualification/tool-qual-cppcheck.md](tool-qualification/tool-qual-cppcheck.md), [../../safety/analysis/misra-deviation-register.md](../../safety/analysis/misra-deviation-register.md) | Recorded / release refresh needed |
| Structural Coverage Evidence | [unit-test/unit-test-report.md](unit-test/unit-test-report.md) | Partial; MC/DC pending |

<!-- HITL-LOCK START:COMMENT-BLOCK-VER-SEC2 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SWE.4 coverage lists a test plan and test report. For ASIL D, SWE.4 also requires static analysis results and structural coverage reports (statement, branch, MC/DC). Consider adding entries for a static analysis report and a coverage report as separate documents, or noting they will be included within the unit test report.
<!-- HITL-LOCK END:COMMENT-BLOCK-VER-SEC2 -->

## Integration Test (SWE.5)

| Document | Path | Status |
|----------|------|--------|
| Integration Strategy | [integration-test/integration-strategy.md](integration-test/integration-strategy.md) | Approved |
| Integration Test Plan | [integration-test/integration-test-plan.md](integration-test/integration-test-plan.md) | Approved |
| Integration Test Report | [integration-test/integration-test-report.md](integration-test/integration-test-report.md) | Executed / refresh for current baseline |

<!-- HITL-LOCK START:COMMENT-BLOCK-VER-SEC3 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SWE.5 lists an integration strategy and report, which are the core work products. The integration strategy should define the integration order (bottom-up, top-down, or sandwich) and the interfaces to test at each integration step. For a 7-ECU zonal platform with CAN bus, the inter-ECU integration strategy is particularly important. Consider adding an integration test specification document separate from the strategy.
<!-- HITL-LOCK END:COMMENT-BLOCK-VER-SEC3 -->

## SW Qualification (SWE.6)

| Document | Path | Status |
|----------|------|--------|
| SW Verification Plan | [sw-qualification/sw-verification-plan.md](sw-qualification/sw-verification-plan.md) | Planned |
| SW Verification Test Specification | To be created under `sw-qualification/` | Missing |
| SW Qualification Test Report | To be created under `sw-qualification/` | Missing |
| Release Notes | [sw-qualification/release-notes.md](sw-qualification/release-notes.md) | Planned |

<!-- HITL-LOCK START:COMMENT-BLOCK-VER-SEC4 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SWE.6 lists a verification plan and release notes. The release notes are a good inclusion as an SWE.6 output. However, a SW qualification test report (separate from the plan) should be listed — the test results are a distinct work product. Also consider adding a SW verification test specification document for the test case definitions.
<!-- HITL-LOCK END:COMMENT-BLOCK-VER-SEC4 -->

## System Integration (SYS.4)

| Document | Path | Status |
|----------|------|--------|
| System Integration Strategy / Plan | To be created under `system-integration/` | Missing |
| System Integration Test Specification | To be created under `system-integration/` | Missing |
| System Integration Report | [system-integration/system-integration-report.md](system-integration/system-integration-report.md) | Planned |

<!-- HITL-LOCK START:COMMENT-BLOCK-VER-SEC5 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SYS.4 lists only an integration report. A system integration test plan/strategy should also be listed — ASPICE SYS.4 requires a defined integration approach at the system level (ECU-to-ECU, gateway integration, CAN bus system-level). The system integration test specification (test cases) is also a distinct work product from the report.
<!-- HITL-LOCK END:COMMENT-BLOCK-VER-SEC5 -->

## System Verification (SYS.5)

| Document | Path | Status |
|----------|------|--------|
| System Verification Plan | To be created under `system-verification/` | Missing |
| System Verification Test Specification | To be created under `system-verification/` | Missing |
| System Verification Report | [system-verification/system-verification-report.md](system-verification/system-verification-report.md) | Planned |
| Safety Validation Report | [../../safety/validation/safety-validation-report.md](../../safety/validation/safety-validation-report.md) | Planned / cross-reference |

<!-- HITL-LOCK START:COMMENT-BLOCK-VER-SEC6 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Same observation as SYS.4 — only a report is listed. SYS.5 also needs a system verification plan and system verification test specification. For ISO 26262, the safety validation report is typically part of SYS.5 scope. The `docs/safety/validation/` folder likely covers this, but a cross-reference here would be helpful.
<!-- HITL-LOCK END:COMMENT-BLOCK-VER-SEC6 -->

## xIL Test Reports

| Document | Path | Level | Status |
|----------|------|-------|--------|
| MIL Report | [xil/mil-report.md](xil/mil-report.md) | Model-in-the-Loop | Planned |
| SIL Report | [xil/sil-report.md](xil/sil-report.md) | Software-in-the-Loop | In progress; scenario evidence pending |
| PIL Report | [xil/pil-report.md](xil/pil-report.md) | Processor-in-the-Loop | Planned |
| HIL Report | [xil/hil-report.md](xil/hil-report.md) | Hardware-in-the-Loop | Planned |

<!-- HITL-LOCK START:COMMENT-BLOCK-VER-SEC7 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The xIL section is well-structured covering all four simulation levels (MIL, SIL, PIL, HIL) which aligns with ASIL D verification best practices. The SIL demo is already live at `sil.taktflow-systems.com` per project MEMORY, so the SIL report status could potentially be updated to "In Progress". These xIL reports do not map directly to a single ASPICE process area — they span SWE.4 through SYS.5. Consider adding a column indicating which ASPICE process each xIL level primarily supports (e.g., SIL maps to SWE.4/SWE.5, HIL maps to SYS.4/SYS.5).
<!-- HITL-LOCK END:COMMENT-BLOCK-VER-SEC7 -->

## xIL to ASPICE Process Mapping

| Level | Primary ASPICE Processes | Main Evidence Expected | Release Acceptance |
|-------|--------------------------|------------------------|--------------------|
| MIL | SWE.4, SWE.6 | Model behavior tests, plant model assumptions, requirement links. | Supports early logic validation only; not sufficient for release alone. |
| SIL | SWE.4, SWE.5, SWE.6 | POSIX/vECU execution logs, vCAN traces, fault-injection results, requirement links. | Every safety scenario has pass/fail, build baseline, and CAN trace. |
| PIL | SWE.5, SWE.6 | Target-compiler/processor execution, timing measurements, resource usage. | Confirms software behavior under target toolchain and representative timing. |
| HIL | SYS.4, SYS.5, ISO 26262 validation | Physical ECU/CAN/sensor/actuator test logs and fault injection. | Required for system-level release and final safety validation. |

## Missing or Weak Work Products

| Priority | Work Product | Why It Matters |
|----------|--------------|----------------|
| P0 | System integration strategy and test specification | SYS.4 requires the integration order, interfaces, test cases, and pass/fail criteria before the report can be trusted. |
| P0 | System verification plan and test specification | SYS.5 needs requirement-based system test cases, not only a final report shell. |
| P0 | SW qualification test specification and report | SWE.6 needs release-level software qualification evidence distinct from unit/integration reports. |
| P1 | MC/DC and structural coverage report | ASIL D release claims require stronger coverage evidence than the current summary. |
| P1 | Scenario-level SIL/HIL logs | xIL reports need reproducible traces and baselines, not only environment descriptions. |
| P1 | Traceability matrix refresh | Current matrices are useful but dated; regenerate them after current implementation and document changes. |

