---
document_id: SIR
title: "System Integration Report"
version: "0.1"
status: planned
aspice_process: SYS.4
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


# System Integration Report

<!-- Phase 13 deliverable -->

## Purpose

Document system integration test results per ASPICE SYS.4.

## Integration Configuration

| Element | Status | Interface Verified |
|---------|--------|-------------------|
| CVC ↔ CAN | — | — |
| FZC ↔ CAN | — | — |
| RZC ↔ CAN | — | — |
| SC ↔ CAN | — | — |
| BCM (Docker) ↔ CAN | — | — |
| ICU (Docker) ↔ CAN | — | — |
| TCU (Docker) ↔ CAN | — | — |
| Gateway ↔ CAN | — | — |
| Gateway ↔ Cloud | — | — |

## Results

<!-- To be completed in Phase 13 -->

