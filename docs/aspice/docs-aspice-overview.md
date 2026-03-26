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

> Full interactive version: [docs/guides/reading-list.html](../guides/reading-list.html) (open in browser for checkboxes and progress tracking)

### Core — Start Here

1. `docs/INDEX.md` — master registry and process coverage
2. `docs/PROJECT_STATE.md` — current status, what works, what is blocked
3. `docs/safety/concept/item-definition.md` — what the system IS at the vehicle level
4. `docs/safety/concept/hara.md` — hazard analysis, the root of every design decision
5. `docs/safety/concept/safety-goals.md` — ASIL allocations, safe states, FTTI

### System & Software Architecture

6. `docs/aspice/system/stakeholder-requirements.md` — SYS.1 intent and stakeholder needs
7. `docs/aspice/system/system-requirements.md` — SYS.2 technical requirements
8. `docs/aspice/system/system-architecture.md` — SYS.3 7-ECU zonal decomposition
9. `docs/aspice/system/interface-control-doc.md` + `can-message-matrix.md` — interfaces and CAN contracts
10. `docs/aspice/software/sw-architecture/sw-architecture.md` — SWE.2 application architecture
11. `docs/aspice/software/sw-architecture/bsw-architecture.md` — AUTOSAR-like BSW stack internals
12. `docs/aspice/software/sw-architecture/vecu-architecture.md` — POSIX vECU / SIL design

### Safety & Traceability

13. `docs/safety/concept/functional-safety-concept.md` — safety goals to technical measures
14. `docs/safety/design/vsm-state-machine.md` — vehicle state machine, the runtime safety backbone
15. `docs/safety/analysis/dfa.md` — dependent failure analysis, CCF and cascading faults
16. `docs/reference/asil-d-reference.md` — ISO 26262 ASIL D tables
17. `docs/aspice/traceability/traceability-matrix.md` — end-to-end trace links

### Build, Process & DBC

18. `gateway/taktflow_vehicle.dbc` — THE single source of truth for all CAN communication
19. `docs/plans/master-plan.md` — execution roadmap, phases, milestones
20. `docs/standards/level-gate-workflow.md` — quality gate workflow

### Deep Dive — Safety Requirements & Analysis

21. `docs/safety/plan/safety-plan.md` — ISO 26262 Part 2 safety plan
22. `docs/safety/requirements/technical-safety-reqs.md` — TSRs bridging safety goals to code
23. `docs/safety/requirements/sw-safety-reqs.md` — software safety requirements
24. `docs/safety/requirements/hsi-specification.md` — hardware-software interface spec
25. `docs/safety/analysis/fmea.md` — failure mode and effects analysis
26. `docs/safety/analysis/asil-decomposition.md` — ASIL decomposition rationale
27. `docs/safety/os-sc3-safety-manual.md` — OS SC3 safety manual

### Codegen, Tooling & Build

28. `docs/arxmlgen/architecture.md` — ARXML code generator architecture
29. `docs/arxmlgen/user-guide.md` — codegen user guide
30. `docs/reference/build-guide.md` — build for POSIX/STM32/TMS570
31. `docs/standards/contract-matrix.md` — BSW module contract matrix

### Verification & Hardware

32. `docs/aspice/verification/integration-test/integration-strategy.md` — SWE.5 integration strategy
33. `docs/aspice/hardware-eng/hw-design.md` — hardware design and schematics
34. `docs/guides/traceability-guide.md` — traceability annotation guide
35. `docs/analysis/sc-safety-critical-dataflow.md` — safety-critical dataflow analysis
36. `docs/lessons-learned/embedded/bringup.md` — debugging lessons (read to avoid repeating mistakes)
37. `docs/safety/audit/gap-analysis-2026-03-25.md` — latest safety gap analysis

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

