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

# SYS.2 Interface and Platform Architecture Plan

Process area: SYS.2 System Architectural Design
Scope: phase 4 and architectural parts of phase 5

## Read This First (Authoritative Docs)

This file is a workstream plan (tasks + gates). For the actual architecture/interface requirements, read:

- `docs/aspice/system/system-requirements.md` (input requirements for architecture)
- `docs/aspice/system/system-architecture.md` (primary SYS.2/SYS.3 architecture spec)
- `docs/aspice/system/interface-control-doc.md` (interface contracts and ownership)
- `docs/aspice/system/can-message-matrix.md` (message/signal-level communication baseline)
- `docs/safety/requirements/hsi-specification.md` (hardware-software interface requirements)
- `docs/aspice/software/sw-architecture/bsw-architecture.md` (BSW layering and dependencies)
- `docs/aspice/traceability/traceability-matrix.md` (requirement allocation and trace links)

## Entry Criteria

- [ ] SYS.1 outputs baselined
- [ ] Target hardware and pin constraints known

## Current Artifact Status (SYS.2 Core)

- [x] `docs/aspice/system/system-requirements.md` (input, drafted)
- [x] `docs/aspice/system/system-architecture.md` (drafted)
- [x] `docs/aspice/system/interface-control-doc.md` (drafted)
- [x] `docs/aspice/system/can-message-matrix.md` (drafted)
- [x] `docs/safety/requirements/hsi-specification.md` (drafted)
- [ ] Cross-document consistency pass pending (CAN IDs/UDS ownership/alignment)

## Work Breakdown

## IA1 Communication Architecture Baseline

- [ ] Build `docs/aspice/system/can-message-matrix.md` with ID, sender, receiver, signal map, cycle time
- [ ] Define E2E protection strategy for safety-critical frames
- [ ] Define diagnostic communication allocation (UDS/OBD responsibilities)
- [ ] Define heartbeat supervision frame and timeout contracts

## IA2 Hardware Software Interface Baseline

- [ ] Publish `docs/safety/requirements/hsi-specification.md`
- [ ] Finalize `hardware/pin-mapping.md` and verify against board schematics
- [ ] Finalize `hardware/bom.md` with procurement substitutions
- [ ] Publish vECU interface spec in `docs/aspice/software/sw-architecture/vecu-architecture.md`

## IA3 Shared Platform Services Architecture

- [ ] Finalize service boundaries for MCAL, ECUAL, services, and RTE
- [ ] Define configuration strategy for ECU-specific variants
- [ ] Define platform abstraction strategy for STM32/TMS570/POSIX boundaries

## Outputs

- [ ] Interface baseline package published
- [ ] HSI baseline package published
- [ ] Platform architecture baseline published

## Review Checklist (Gate G1 -> G2)

- [ ] No undefined safety-relevant message ID
- [ ] Safety-relevant signals have E2E policy
- [ ] HSI pin maps are internally consistent
- [ ] Architecture is traceable to requirement IDs

