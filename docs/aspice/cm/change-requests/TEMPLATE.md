---
document_id: CR-NNN
title: "Change Request: {Title}"
version: "0.1"
status: template
aspice_process: SUP.8
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


# Change Request: CR-NNN — {Title}

## Request Information

| Field | Value |
|-------|-------|
| CR ID | CR-NNN |
| Requested by | — |
| Date | YYYY-MM-DD |
| Priority | — |
| Affected baseline | BL-NNN |

## Description

{What needs to change and why}

## Impact Analysis

| Area | Impact |
|------|--------|
| Safety | — |
| Requirements | — |
| Architecture | — |
| Code | — |
| Tests | — |

## Approval

| Role | Decision | Date |
|------|----------|------|
| Project Lead | — | — |
| Safety Engineer | — | — |

## Implementation

| Task | Status |
|------|--------|
| — | — |

