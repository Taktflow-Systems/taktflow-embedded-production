---
document_id: MIL-RPT
title: "MIL Test Report"
version: "0.1"
status: planned
aspice_process: SWE.5
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


# Model-in-the-Loop (MIL) Test Report

<!-- Phase 12 deliverable -->

## Purpose

Validate control algorithms using Python plant models (no firmware, no CAN).

## Plant Models

| Model | File | Parameters |
|-------|------|-----------|
| DC Motor | test/mil/plant_motor.py | J, B, Ke, Kt, R, L |
| Steering Servo | test/mil/plant_steering.py | Rate limit, saturation |
| Vehicle Dynamics | test/mil/plant_vehicle.py | Mass, drag, tire radius |

## Test Scenarios

| Scenario | Expected | Actual | Status |
|----------|----------|--------|--------|
| Normal operation | — | — | — |
| Overcurrent | — | — | — |
| Thermal runaway | — | — | — |
| Emergency brake | — | — | — |

## Results

<!-- To be completed in Phase 12 -->

