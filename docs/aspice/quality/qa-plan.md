---
document_id: QAP
title: "Quality Assurance Plan"
version: "0.2"
status: draft
aspice_process: SUP.1
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

Every QA topic in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/lessons-learned/`](../lessons-learned/). One file per QA topic. File naming: `QA-<topic>.md`.

# Quality Assurance Plan

<!-- Phase 0 deliverable -->

## Purpose

Define QA activities, responsibilities, and criteria per ASPICE SUP.1.

This plan covers product quality assurance, process compliance, safety evidence
quality, and release gate readiness. It is intended to be used with the safety
plan, verification index, traceability matrices, and CM/change records.

<!-- HITL-LOCK START:COMMENT-BLOCK-QA-SEC1 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The purpose statement is clear and correctly references SUP.1. However, SUP.1 base practices (BP1-BP7) are not explicitly mapped in this document. The plan should consider adding a BP-to-activity traceability table to demonstrate full SUP.1 coverage. Also, "responsibilities" are mentioned in the purpose but no roles or responsibility assignments appear anywhere in the document.
<!-- HITL-LOCK END:COMMENT-BLOCK-QA-SEC1 -->

## SUP.1 Base Practice Coverage

| SUP.1 Practice | Project Activity | Evidence |
|----------------|------------------|----------|
| Establish QA strategy | Maintain this QA plan, safety plan, and level-gate workflow. | QA plan, safety plan, standards/level-gate workflow. |
| Assure work products | Review requirements, architecture, safety analyses, code, tests, and release notes against checklists. | Review records, issue dispositions, traceability matrix. |
| Assure process compliance | Audit that required ASPICE and ISO 26262 work products exist before each gate. | Process compliance audit log, gate checklist. |
| Report QA findings | Log findings with severity, owner, due date, and closure evidence. | QA finding register or issue tracker. |
| Escalate nonconformance | Block release when P0/P1 findings, trace gaps, or safety evidence gaps remain unresolved. | Gate decision record and release notes. |
| Verify closure | Re-review closed findings and attach objective evidence. | Finding closure record and linked evidence. |
| Maintain independence | Assign reviewer independence by highest affected ASIL and process risk. | Review assignment and independence record. |

## QA Activities

| Activity | Frequency | Owner | Method | Evidence |
|----------|-----------|-------|--------|----------|
| Code review | Every PR or change batch | SW Engineer + reviewer | Checklist review with ASIL-aware focus on safety mechanisms, interfaces, and error handling. | Review record, finding list, disposition. |
| Static analysis | Every build and release baseline | SW Engineer | cppcheck + MISRA; deviations recorded and justified. | CI/local log, MISRA deviation register. |
| Unit test execution | Every build and release baseline | Test Engineer | Unity/pytest execution with pass/fail and coverage collection. | Unit test report, coverage output. |
| Integration test execution | Each integration baseline | Test Engineer | BSW, SWC, CAN, E2E, diagnostic, and safe-state integration tests. | Integration test plan/report. |
| xIL scenario execution | Each xIL baseline | Test Engineer | MIL/SIL/PIL/HIL scenarios with fault injection and CAN traces. | xIL reports, logs, CAN traces. |
| Documentation review | Each lifecycle gate | QA + responsible engineer | Check completeness, consistency, traceability, assumptions, and reviewer comments. | Document review record. |
| Traceability audit | Each lifecycle gate and release | QA + Safety Manager | Regenerate/check SG->FSR->TSR->SSR/HSR->SWR->test links. | Traceability matrix and gap log. |
| Process compliance audit | Each lifecycle gate | QA | Verify ASPICE/ISO work products and approvals exist for the gate. | Process audit checklist. |
| Safety review | Concept, architecture, implementation, verification, release | Independent reviewer | Review against highest affected ASIL and required independence level. | Safety review record and action log. |

<!-- HITL-LOCK START:COMMENT-BLOCK-QA-SEC2 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The QA activities table covers the core practices well. The MISRA pipeline status (all phases done, 0 violations, CI blocking) from MEMORY confirms the static analysis activity is operational. Consider adding: (1) process compliance audits (SUP.1 BP3 — ensure processes are followed, not just products are correct), (2) documentation review as a QA activity, and (3) who is responsible for each activity (role assignment). The "Independent review" for safety reviews needs clarification on what level of independence is required (I2 or I3 per ISO 26262).
<!-- HITL-LOCK END:COMMENT-BLOCK-QA-SEC2 -->

## Responsibility and Independence Rules

| Highest Affected ASIL | Minimum Reviewer Independence | QA Expectation |
|-----------------------|-------------------------------|----------------|
| QM / ASIL A | I0 or peer review | Self-review is acceptable only with a checklist and recorded disposition. |
| ASIL B | I1 | Reviewer must be different from the author. |
| ASIL C | I2 target | Reviewer should be independent of the implementation workstream. |
| ASIL D | I2 minimum for this project; I3 normative gap recorded | External or organizationally independent assessment is required before production-style claims. |

One person may hold multiple project roles, but the same person must not both
author and independently approve the same ASIL C/D work product without recording
a tailoring/gap rationale.

## Quality Criteria

| Criterion | Target |
|-----------|--------|
| MISRA mandatory violations | 0 |
| MISRA required violations | 0 or formally deviated |
| Unit test pass rate | 100% |
| Integration test pass rate | 100% for release baseline |
| Statement coverage (ASIL D) | 100% target or justified gap |
| Branch coverage (ASIL D) | 100% target or justified gap |
| MC/DC coverage (ASIL D) | 100% |
| Traceability gaps | 0 |
| Open P0/P1 safety findings at release | 0 |
| Open critical defects at release | 0 |
| Unreviewed ASIL C/D work products at release | 0 |
| Unbaselined verification evidence at release | 0 |

<!-- HITL-LOCK START:COMMENT-BLOCK-QA-SEC3 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The quality criteria are well-defined and align with ASIL D targets from the project rules (100% MC/DC from `testing.md`, 0 MISRA mandatory violations from `misra-c.md`). Consider adding: branch coverage target (100% for ASIL D), statement coverage target, MISRA required violation target (0 or all formally deviated), and maximum cyclomatic complexity per function. The MISRA criterion currently only mentions "mandatory" — required rule compliance should also be tracked.
<!-- HITL-LOCK END:COMMENT-BLOCK-QA-SEC3 -->

## Finding Severity

| Severity | Meaning | Release Impact |
|----------|---------|----------------|
| P0 | Safety goal violation, missing ASIL D evidence, failed safe-state behavior, or blocker process nonconformance. | Release blocked. |
| P1 | Traceability gap, missing required review, incomplete coverage/static-analysis evidence, or unresolved safety-analysis action. | Release blocked unless formally accepted with rationale. |
| P2 | Documentation inconsistency, stale path, unclear assumption, or incomplete non-critical evidence. | Must have owner and due date. |
| P3 | Editorial or maintainability issue. | Can be deferred if recorded. |

## Review Schedule

| Phase | Review Type | Scope | Minimum Independence | Method |
|-------|-------------|-------|----------------------|--------|
| 0 | Safety management setup | Safety plan, QA plan, CM strategy, coding/test standards. | I1 | Walk-through |
| 1 | Safety concept review | Item definition, HARA, safety goals, FSC, FSR. | I2 for ASIL C/D | Inspection |
| 2 | Safety analysis review | FMEA, DFA, hardware metrics, ASIL decomposition, timing assumptions. | I2 | Inspection |
| 3 | Requirements review | TSR, SSR, HSR, HSI, traceability. | I2 for ASIL C/D | Inspection |
| 4 | System and software architecture review | SYS.3, SWE.2, BSW, vECU, CAN interfaces. | I2 for ASIL C/D | Inspection |
| 5-10 | Implementation review | BSW, SWC, SC, platform, diagnostics, safety mechanisms. | I1/I2 by ASIL | Code inspection |
| 11 | Integration readiness review | Integration strategy, plan, interfaces, test environment. | I1/I2 by ASIL | Gate review |
| 12 | Verification review | Unit/integration/SIL results, static analysis, coverage, traceability. | I2 for ASIL C/D | Formal review |
| 13 | System integration review | SYS.4 ECU/CAN/gateway integration and fault injection evidence. | I2 | Formal review |
| 14 | Safety validation review | SYS.5, safety validation, release notes, safety case. | I2 minimum; I3 gap recorded | Assessment-style review |
| 18 | Physical HIL review | Hardware build, HIL timing, fault injection, endurance. | I2 minimum; I3 recommended | Assessment-style review |

<!-- HITL-LOCK START:COMMENT-BLOCK-QA-SEC4 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The review schedule maps reviews to project phases which is good for planning. However, there are significant gaps between phases (e.g., phases 6-11 have no scheduled reviews, phases 2 and 4 are also skipped). For ASIL D, ISO 26262 Part 2 requires confirmation measures at each lifecycle phase. Consider adding reviews for: implementation reviews (phases 6-10), integration test reviews (phase 11), and safety validation review (phase 13). Also missing: who conducts each review (independence level) and the review method (walkthrough, inspection, formal review per ASPICE SUP.4).
<!-- HITL-LOCK END:COMMENT-BLOCK-QA-SEC4 -->

## Release QA Gate

A release or public safety claim may proceed only when:

- P0 findings are closed.
- P1 findings are closed or formally accepted with residual-risk rationale.
- Safety and ASPICE traceability matrices are regenerated for the baseline.
- Static analysis, unit, integration, xIL, and system verification evidence use the same baseline identifier.
- Safety case open gaps are either closed or explicitly excluded from the release claim.

