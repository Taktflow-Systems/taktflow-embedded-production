---
document_id: BL-NNN
title: "Baseline: {Name}"
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


# Baseline: {BL-NNN} — {Name}

## Baseline Information

| Field | Value |
|-------|-------|
| Baseline ID | BL-NNN |
| Git Tag | vX.Y.Z |
| Date | YYYY-MM-DD |
| Branch | main |
| Commit | {SHA} |

## Scope

| Category | Items Included |
|----------|---------------|
| Safety docs | — |
| Requirements | — |
| Source code | — |
| Test results | — |

## Approval

| Role | Name | Date |
|------|------|------|
| Project Lead | — | — |
| Safety Engineer | — | — |
| CM Manager | — | — |

## Change History from Previous Baseline

| CR-ID | Description | Impact |
|-------|-------------|--------|
| — | — | — |

