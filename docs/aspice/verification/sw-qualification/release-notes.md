---
document_id: RN
title: "Release Notes"
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


# Release Notes

<!-- Phase 14 deliverable -->

## Release: v1.0.0

- **Date**: —
- **Baseline**: BL-001
- **Branch**: release/1.0.0

## Changes

<!-- To be completed at release -->

## Known Issues

## Test Summary

## Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Project Lead | — | — | — |
| Safety Engineer | — | — | — |

