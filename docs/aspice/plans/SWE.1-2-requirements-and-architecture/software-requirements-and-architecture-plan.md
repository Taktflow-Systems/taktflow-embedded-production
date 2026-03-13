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

# SWE.1 and SWE.2 Plan

Process areas:
- SWE.1 Software Requirements Analysis
- SWE.2 Software Architectural Design

## Objective

Convert safety and system requirements into implementable software requirements and architecture baselines for all ECUs and gateway components.

## Entry Criteria

- [ ] SYS.1 safety and requirement baseline approved
- [ ] SYS.2 interface baseline approved

## Activities

## SWE.1 Software Requirements

- [ ] Define software requirement IDs per ECU (CVC/FZC/RZC/SC/BCM/ICU/TCU/Gateway)
- [ ] Add acceptance criteria for each safety-relevant software requirement
- [ ] Define timing requirements (period, jitter, timeout)
- [ ] Define diagnostic requirements (DTC trigger, clear, status behavior)

## SWE.2 Software Architecture

- [ ] Define software component decomposition per ECU
- [ ] Define interfaces between SWCs and BSW services
- [ ] Define mode/state behavior and transitions
- [ ] Define error handling and degradation strategy
- [ ] Define platform abstraction boundaries (STM32, TMS570, POSIX)

## Work Products

- [ ] `docs/safety/sw-safety-requirements.md`
- [ ] `docs/aspice/software/sw-architecture/sw-architecture.md`
- [ ] `docs/aspice/traceability/traceability-matrix.md` updated with SWE links

## Exit Criteria

- [ ] Each software requirement is testable
- [ ] Architecture satisfies all safety-critical requirements
- [ ] Traceability links exist from requirements to planned tests

