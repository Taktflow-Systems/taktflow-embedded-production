---
document_id: HIL-RPT
title: "HIL Test Report"
version: "0.1"
status: planned
aspice_process: SYS.4
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


# Hardware-in-the-Loop (HIL) Test Report

<!-- Phase 13 deliverable -->

## Purpose

Full hardware validation with all 4 physical ECUs, real sensors/actuators, and fault injection.

## Hardware Configuration

- 4 physical ECUs on CAN bus
- 3 simulated ECUs via CAN bridge
- Pi gateway connected
- All sensors and actuators wired

## Test Scenarios

All 16 demo scenarios + endurance test.

| # | Scenario | Result | Evidence |
|---|----------|--------|----------|
| 1-16 | — | — | — |

## xIL Comparison

| Test | MIL | SIL | PIL | HIL |
|------|-----|-----|-----|-----|
| — | — | — | — | — |

## Results

<!-- To be completed in Phase 13 -->

