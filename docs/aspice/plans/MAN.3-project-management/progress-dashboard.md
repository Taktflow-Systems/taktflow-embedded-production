---
document_id: MAN3-PROGRESS-DASHBOARD
title: "MAN.3 Progress Dashboard"
version: "0.1"
status: active
updated: "2026-02-21"
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


# MAN.3 Progress Dashboard

## Current Snapshot

- Reporting date: `2026-02-21`
- Release target: `v1.0.0`
- Active branch: `develop`
- Overall phase: `Phase 0 complete, Phase 1 starting`

## Gate Readiness

| Gate | Readiness % | Status | Blocking Items |
|------|-------------|--------|----------------|
| G1 Safety Baseline | 20% | In progress | HARA complete review, TSR baseline |
| G2 Software Baseline | 5% | Not started | Shared BSW integration |
| G3 Integration Baseline | 0% | Not started | vECU + gateway bring-up |
| G4 Release Baseline | 0% | Not started | xIL/HIL evidence package |

## Workstream Completion

| Workstream | Completion % | Status | Owner |
|------------|--------------|--------|-------|
| SYS.1 Safety Requirements | 25% | In progress | TBD |
| SYS.2 System Architecture | 15% | In progress | TBD |
| SWE.1/2 SW Req + Arch | 5% | Not started | TBD |
| SWE.3 Implementation | 0% | Not started | TBD |
| SWE.4/5/6 Verification | 0% | Not started | TBD |

## Weekly Commitments vs Actual

| Week | Committed | Completed | Carry-over |
|------|-----------|-----------|------------|
| 2026-W08 | 3 | 0 | 3 |

## Daily Tracking

- Template: `docs/aspice/plans/MAN.3-project-management/daily-progress-template.md`
- Latest log: `docs/aspice/plans/MAN.3-project-management/daily-log/2026-02-21.md`

## Quality and Delivery KPIs

| KPI | Target | Current | Trend |
|-----|--------|---------|-------|
| Planned vs completed tasks | >= 80% | 0% | Flat |
| Open blockers | <= 3 | 0 | Flat |
| Blocker aging (days) | <= 2 | 0 | Flat |
| Defect reopen rate | < 10% | N/A | N/A |
| Evidence completion | >= 70% by G1 | 15% | Up |

## Immediate Next Actions

- [ ] Fill weekly status for current week
- [ ] Update risk register with owners and due dates
- [ ] Review G1 checklist and mark objective evidence links

