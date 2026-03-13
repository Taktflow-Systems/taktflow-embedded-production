---
document_id: SVTR
title: "System Verification Report"
version: "0.1"
status: planned
aspice_process: SYS.5
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


# System Verification Report

<!-- Phase 13-14 deliverable -->

## Purpose

Verify system requirements are met per ASPICE SYS.5.

## Demo Scenario Results

| # | Scenario | Result | Evidence |
|---|----------|--------|----------|
| 1-16 | See master-plan.md | — | — |

## Endurance Test

| Metric | Target | Actual |
|--------|--------|--------|
| Duration | 30 min | — |
| CAN errors | 0 | — |
| Watchdog resets | 0 | — |
| Memory drift | 0 | — |

## Results

<!-- To be completed in Phase 13-14 -->

