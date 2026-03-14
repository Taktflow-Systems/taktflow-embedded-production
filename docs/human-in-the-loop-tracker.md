
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



Purpose: track where manual review is currently focused and what has been completed, to avoid double work.

Status labels:
- `IN PROGRESS`
- `DONE`
- `RECOMMENDED (NOT DONE)`
- `BLOCKED`

## Session

- Last updated: `2026-02-23`
- Active reviewer: `Ngoc An Dao`
- Active document: `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md`

## Current Block

- Block ID: `B-034`
- Scope: `STK-009 review comment write-down`
- Location: `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md:172`
- Status: `DONE`
- Notes:
  - Added HITL comment block for STK-009 with `Why + Tradeoff + Alternative`.
  - Captured rationale for CAN listen-only monitoring and independent hardware kill-relay enforcement.

## Next Block

- Block ID: `B-035`
- Scope: `Requirement-by-requirement review continuation (STK-010 next)`
- Location: `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md:182`
- Status: `IN PROGRESS`

## Block History

| Date | Block ID | Document | Scope | Status | Evidence |
|---|---|---|---|---|---|
| 2026-02-23 | B-001 | `taktflow-embedded/docs/PROJECT_STATE.md` | Phase 0 status normalization | DONE | `DONE` vs `RECOMMENDED (NOT DONE)` items added |
| 2026-02-23 | B-002 | `taktflow-embedded/docs/PROJECT_STATE.md` | Phase 1 gap check | DONE | FSC open items (FSC-O-001..005), FSR open items (FSR-O-001..006) confirmed |
| 2026-02-23 | B-003 | `taktflow-embedded/docs/PROJECT_STATE.md` | Phase 1 status normalization (DONE vs RECOMMENDED gaps) | BLOCKED | Deferred to metrics audit first due baseline inconsistency risk |
| 2026-02-23 | B-004 | `taktflow-embedded/docs/PROJECT_STATE.md` | Safety metrics consistency audit | DONE | HARA has SG-001..013 while Safety Goals/PROJECT_STATE use SG-001..008 |
| 2026-02-23 | B-005 | `taktflow-embedded/docs/PROJECT_STATE.md` | Write findings directly into reviewed doc | DONE | Phase 1 now includes explicit `RECOMMENDED (NOT DONE)` findings and closure signals |
| 2026-02-23 | B-007 | `taktflow-embedded/docs/PROJECT_STATE.md` | Phase 2 safety analysis gap review | DONE | Phase 2 now includes explicit recommended actions for metrics wording, HWM open items, and FMEA RPN completion |
| 2026-02-23 | B-009 | `taktflow-embedded/docs/PROJECT_STATE.md` | Phase 3 requirements and governance integrity review | DONE | Phase 3 now includes severity-tagged findings for ASIL summary integrity, traceability drift, and open-item governance |
| 2026-02-23 | B-011 | `taktflow-embedded/docs/PROJECT_STATE.md` | Phase 4 governance integrity review | DONE | Phase 4 now includes severity-tagged findings for BOM totals, artifact maturity alignment, and index status synchronization |
| 2026-02-23 | B-013 | `taktflow-embedded/docs/PROJECT_STATE.md` | Phase 5 BSW evidence and metric integrity review | DONE | Phase 5 now includes severity-tagged findings for execution evidence, test-count baseline, LOC scope, and maturity alignment |
| 2026-02-23 | B-015 | `taktflow-embedded/docs/PROJECT_STATE.md` | Phase 6 CVC firmware evidence and maturity review | DONE | Phase 6 now includes severity-tagged findings for formal test-evidence publication and status-maturity synchronization |
| 2026-02-23 | B-016 | `taktflow-embedded/docs/PROJECT_STATE.md` | Phase 6 traceability and metric alignment review | DONE | Added Phase 6 HIGH finding for CVC test-case range drift (`~70` documented vs `88` in source inventory) |
| 2026-02-23 | B-017 | `taktflow-embedded/docs/PROJECT_STATE.md` | Phase 6 source-of-truth alignment edits (timing-source clarity) | DONE | Added timing-source table with per-metric evidence type and hardware-closure criteria |
| 2026-02-23 | B-019 | `taktflow-embedded/docs/PROJECT_STATE.md` | Phase 7 readiness review using Phase 6 evidence-first method | DONE | Added Phase 7 snapshot with HIGH/MEDIUM findings: empty FZC implementation baseline, traceability-vs-implementation gap, missing phase gates, and status vocabulary drift |
| 2026-02-23 | B-020 | `taktflow-embedded/docs/PROJECT_STATE.md` | Hardware feasibility integrity review | DONE | Added HIGH/MEDIUM findings: open rail/fuse closure, BOM part/cost drift, missing proof artifacts, and procurement tracker mismatch |
| 2026-02-23 | B-021 | `taktflow-embedded/docs/PROJECT_STATE.md` | Hardware feasibility verdict write-down | DONE | Added explicit verdict: partially correct, not fully verified, with closure rule |
| 2026-02-23 | B-023 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | Stakeholder requirements integrity review | DONE | Added HIGH/MEDIUM findings for stale open-item target, numbering drift, STK-to-SYS derivation-rule mismatch, and SYS.1 over-specific constraints |
| 2026-02-23 | B-024 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | Stakeholder architecture-fit write-up | DONE | Added architecture-fit verdict with strengths/gaps and recommended SYS.1 usage boundary |
| 2026-02-23 | B-025 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | STK-001 review note write-down | DONE | Added `Reviewed` note for STK-001 to lock the stakeholder WHAT vs downstream HOW boundary |
| 2026-02-23 | B-026 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | STK-001 comment-format correction to HITL standard | DONE | Replaced non-standard review note with HITL `Why + Tradeoff + Alternative` block |
| 2026-02-23 | B-027 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | STK-002 review comment write-down | DONE | Added HITL `Why + Tradeoff + Alternative` block for ASPICE process-maturity stakeholder requirement |
| 2026-02-23 | B-028 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | STK-003 review comment write-down | DONE | Added HITL `Why + Tradeoff + Alternative` block for AUTOSAR-like BSW architecture stakeholder requirement |
| 2026-02-23 | B-029 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | STK-004 review comment write-down | DONE | Added HITL `Why + Tradeoff + Alternative` block for board-selection rationale under diverse redundancy requirement |
| 2026-02-23 | B-030 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | STK-005 review comment write-down | DONE | Added HITL `Why + Tradeoff + Alternative` block explaining stakeholder-level simplicity vs downstream detail allocation |
| 2026-02-23 | B-031 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | STK-006 review comment write-down | DONE | Added HITL `Why + Tradeoff + Alternative` block for steering-feedback meaning and prototype-limit boundary |
| 2026-02-23 | B-032 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | STK-007 review comment write-down | DONE | Added HITL `Why + Tradeoff + Alternative` block for proportional braking + fail-safe default behavior |
| 2026-02-23 | B-033 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | STK-008 review comment write-down | DONE | Added HITL `Why + Tradeoff + Alternative` block for graduated obstacle response and safe-default lidar-fault handling |
| 2026-02-23 | B-034 | `taktflow-embedded/docs/aspice/system/stakeholder-requirements.md` | STK-009 review comment write-down | DONE | Added HITL `Why + Tradeoff + Alternative` block for independent monitoring, CAN listen-only policy, and kill-relay enforcement path |

## Update Rule

On every move from one review block to the next:
1. Set current block to `DONE` (or `BLOCKED`) and write one-line evidence.
2. Append one row to `Block History`.
3. Create/update `Next Block` with location and status.







