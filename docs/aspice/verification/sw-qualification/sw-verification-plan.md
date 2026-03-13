---
document_id: SVP
title: "SW Verification Plan"
version: "0.1"
status: planned
aspice_process: SWE.6
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


# SW Verification Plan

<!-- Phase 12 deliverable -->

## Purpose

Define the software qualification test approach per ASPICE SWE.6.

## Verification Methods

| Method | Tool | ASIL D Required |
|--------|------|----------------|
| Requirements-based testing | Unity/pytest | Yes |
| Interface testing | SIL + CAN | Yes |
| Fault injection testing | CAN injection | Yes |
| Static analysis | cppcheck + MISRA | Yes |
| Resource usage measurement | linker map | Yes |

## Qualification Criteria

- All safety requirements have passing tests
- No MISRA mandatory violations
- MC/DC coverage >= 100% for ASIL D modules
- WCET within budget for all safety functions

