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

# Weekly Status Template (MAN.3)

Week of: `YYYY-MM-DD`
Owner:

## Commitments

- [ ] C1
- [ ] C2
- [ ] C3

## Progress

- Completed:
- In progress:
- Not started:

## Gate Impact

- G1:
- G2:
- G3:
- G4:

## Risks and Blockers

- Risk:
- Blocker:
- Mitigation:

## Next Week Plan

- [ ] N1
- [ ] N2
- [ ] N3

