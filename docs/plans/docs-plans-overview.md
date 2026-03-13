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

# Planning Docs

`docs/plans/` now keeps only the source master plan and entry pointers.

## Source

- `docs/plans/master-plan.md`

## Actionable ASPICE Plans

- `docs/aspice/plans/apps-web-overview.md`
- `docs/aspice/plans/MAN.3-project-management/execution-roadmap.md`
- `docs/aspice/plans/SYS.1-system-requirements/safety-workstream.md`
- `docs/aspice/plans/SYS.2-system-architecture/interfaces-and-bsw-workstream.md`
- `docs/aspice/plans/SWE.1-2-requirements-and-architecture/software-requirements-and-architecture-plan.md`
- `docs/aspice/plans/SWE.3-implementation/firmware-ecus-workstream.md`
- `docs/aspice/plans/SWE.3-implementation/vecu-gateway-workstream.md`
- `docs/aspice/plans/SWE.4-6-verification-and-release/verification-hil-release-workstream.md`


