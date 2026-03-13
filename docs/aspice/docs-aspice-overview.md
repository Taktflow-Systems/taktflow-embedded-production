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

# ASPICE Folder Map

This folder contains project artifacts organized by Automotive SPICE process areas.

## How To Read This Folder

- `plans` files are execution workstreams (tasks, gates, progress).
- `system`, `software`, and `verification` files are the main technical specifications and evidence.
- If you are new, start with the "Recommended Reading Path" below.

## Recommended Reading Path

1. `docs/INDEX.md` (master registry and process coverage)
2. `docs/aspice/system/stakeholder-requirements.md` (SYS.1 intent and stakeholder needs)
3. `docs/aspice/system/system-requirements.md` (SYS.2 technical requirements)
4. `docs/aspice/system/system-architecture.md` (SYS.3 decomposition and allocation)
5. `docs/aspice/system/interface-control-doc.md` and `docs/aspice/system/can-message-matrix.md` (interfaces and communication contracts)
6. `docs/aspice/software/sw-architecture/` (SWE.2 architecture details)
7. `docs/aspice/traceability/traceability-matrix.md` (end-to-end trace links)

## Plan Vs Spec (Important)

Use these together:

- `docs/aspice/plans/SYS.1-system-requirements/safety-workstream.md` -> links to SYS.1 and safety requirement/spec docs
- `docs/aspice/plans/SYS.2-system-architecture/interfaces-and-bsw-workstream.md` -> links to SYS.2/SYS.3 interface and architecture docs
- `docs/aspice/plans/SWE.1-2-requirements-and-architecture/` -> software requirements and architecture execution tracking

The plan files do not replace the specification files; they point to them and track completion.

## Subfolders

- `docs/aspice/plans/` - MAN.3 execution plans and tracking
- `docs/aspice/system/` - SYS.1 to SYS.3 artifacts
- `docs/aspice/software/` - SWE.1 to SWE.3 architecture/requirements
- `docs/aspice/verification/` - SWE.4 to SWE.6 and SYS.4/SYS.5 reports
- `docs/aspice/hardware-eng/` - HWE artifacts
- `docs/aspice/cm/` - SUP.8 configuration/change management
- `docs/aspice/quality/` - SUP.1 quality artifacts
- `docs/aspice/traceability/` - traceability matrix and mappings

