---
document_id: PIL-RPT
title: "PIL Test Report"
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


# Processor-in-the-Loop (PIL) Test Report

<!-- Phase 12 deliverable -->

## Purpose

Validate real-time behavior on target MCU with simulated plant via CAN bridge.

## Configuration

- 1+ STM32 on real CAN bus
- CANable on PC bridges to plant simulator
- Remaining ECUs simulated

## Test Scenarios

| Scenario | Target MCU | Real-Time Met | CAN Jitter |
|----------|-----------|---------------|------------|
| — | — | — | — |

## SIL vs PIL Comparison

| Metric | SIL | PIL | Delta |
|--------|-----|-----|-------|
| Loop time | — | — | — |
| CAN latency | — | — | — |

## Results

<!-- To be completed in Phase 12 -->

