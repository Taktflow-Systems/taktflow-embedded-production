---
document_id: MAN3-DAILY-TEMPLATE
title: "Daily Progress Template"
version: "0.1"
status: active
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


# Daily Progress - YYYY-MM-DD

Date: `YYYY-MM-DD`
Owner: `TBD`
LinkedIn Ready: `yes/no`

## Today Plan

- [ ] P1
- [ ] P2
- [ ] P3

## Completed Today

- [ ] C1
- [ ] C2
- [ ] C3

## Evidence Added

- `path/to/file-1`
- `path/to/file-2`

## Blockers

- Blocker:
- Impact:
- Mitigation/Next action:

## Metrics

- Tasks planned: `0`
- Tasks completed: `0`
- Completion %: `0%`
- Open blockers: `0`

## LinkedIn Summary (copy-ready)

Today I progressed [short summary]. I completed [key outputs], and next I will [next step].

