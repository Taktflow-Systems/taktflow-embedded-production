---
document_id: MAN3-ISSUE-LOG
title: "Issue Log"
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


# Issue Log

| Issue ID | Summary | Severity | Opened | Owner | Target Resolve | Status | Resolution Evidence |
|----------|---------|----------|--------|-------|----------------|--------|---------------------|
| I-001 | No single live dashboard existed for gate readiness | Medium | 2026-02-21 | TBD | 2026-02-21 | Closed | `docs/aspice/plans/MAN.3-project-management/progress-dashboard.md` |
| I-002 | Weekly tracking existed only as template | Low | 2026-02-21 | TBD | 2026-02-21 | Closed | `docs/aspice/plans/MAN.3-project-management/weekly-status-2026-W08.md` |

## Rules

- Use `Open` only if blocker affects delivery or gate readiness.
- Close only with linked evidence path.

