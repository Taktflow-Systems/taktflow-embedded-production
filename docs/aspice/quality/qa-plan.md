---
document_id: QAP
title: "Quality Assurance Plan"
version: "0.1"
status: planned
aspice_process: SUP.1
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

<!-- HITL-LOCK START:COMMENT-BLOCK-QA-SEC1 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The purpose statement is clear and correctly references SUP.1. However, SUP.1 base practices (BP1-BP7) are not explicitly mapped in this document. The plan should consider adding a BP-to-activity traceability table to demonstrate full SUP.1 coverage. Also, "responsibilities" are mentioned in the purpose but no roles or responsibility assignments appear anywhere in the document.
<!-- HITL-LOCK END:COMMENT-BLOCK-QA-SEC1 -->

## QA Activities

| Activity | Frequency | Method |
|----------|-----------|--------|
| Code review | Every PR | GitHub PR review |
| Static analysis | Every build | cppcheck + MISRA |
| Unit test execution | Every build | Unity + pytest |
| Traceability audit | Per phase | Manual review |
| Safety review | Per phase | Independent review |

<!-- HITL-LOCK START:COMMENT-BLOCK-QA-SEC2 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The QA activities table covers the core practices well. The MISRA pipeline status (all phases done, 0 violations, CI blocking) from MEMORY confirms the static analysis activity is operational. Consider adding: (1) process compliance audits (SUP.1 BP3 — ensure processes are followed, not just products are correct), (2) documentation review as a QA activity, and (3) who is responsible for each activity (role assignment). The "Independent review" for safety reviews needs clarification on what level of independence is required (I2 or I3 per ISO 26262).
<!-- HITL-LOCK END:COMMENT-BLOCK-QA-SEC2 -->

## Quality Criteria

| Criterion | Target |
|-----------|--------|
| MISRA mandatory violations | 0 |
| Unit test pass rate | 100% |
| MC/DC coverage (ASIL D) | 100% |
| Traceability gaps | 0 |
| Open critical defects at release | 0 |

<!-- HITL-LOCK START:COMMENT-BLOCK-QA-SEC3 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The quality criteria are well-defined and align with ASIL D targets from the project rules (100% MC/DC from `testing.md`, 0 MISRA mandatory violations from `misra-c.md`). Consider adding: branch coverage target (100% for ASIL D), statement coverage target, MISRA required violation target (0 or all formally deviated), and maximum cyclomatic complexity per function. The MISRA criterion currently only mentions "mandatory" — required rule compliance should also be tracked.
<!-- HITL-LOCK END:COMMENT-BLOCK-QA-SEC3 -->

## Review Schedule

| Phase | Review Type | Scope |
|-------|------------|-------|
| 1 | Safety concept review | HARA, safety goals |
| 3 | Requirements review | TSR, SSR, HSR |
| 5 | Architecture review | BSW design |
| 12 | Verification review | Test results, coverage |
| 14 | Release review | Safety case, traceability |

<!-- HITL-LOCK START:COMMENT-BLOCK-QA-SEC4 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The review schedule maps reviews to project phases which is good for planning. However, there are significant gaps between phases (e.g., phases 6-11 have no scheduled reviews, phases 2 and 4 are also skipped). For ASIL D, ISO 26262 Part 2 requires confirmation measures at each lifecycle phase. Consider adding reviews for: implementation reviews (phases 6-10), integration test reviews (phase 11), and safety validation review (phase 13). Also missing: who conducts each review (independence level) and the review method (walkthrough, inspection, formal review per ASPICE SUP.4).
<!-- HITL-LOCK END:COMMENT-BLOCK-QA-SEC4 -->

