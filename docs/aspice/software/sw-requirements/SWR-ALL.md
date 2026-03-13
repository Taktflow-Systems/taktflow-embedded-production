---
document_id: SWR-ALL
title: "Software Requirements — Cross-Cutting (All ECUs)"
version: "1.0"
status: draft
aspice_process: SWE.1
asil: D
date: 2026-02-25
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


# Software Requirements — Cross-Cutting (All ECUs)

## 1. Purpose

This document specifies cross-cutting software requirements that apply to ALL firmware ECUs (CVC, FZC, RZC, SC, BCM, ICU, TCU) of the Taktflow Zonal Vehicle Platform. These requirements are enforced at the build/CI level and verified by static analysis rather than individual unit tests.

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SYSREQ | System Requirements Specification | 1.0 |
| STK | Stakeholder Requirements Specification | 1.0 |

## 3. Requirement Conventions

### 3.1 Requirement Structure

Each requirement follows the format:

- **ID**: SWR-ALL-NNN (sequential)
- **Title**: Descriptive name
- **ASIL**: Varies (QM to D)
- **Traces up**: SYS-xxx
- **Traces down**: All firmware source files
- **Verified by**: CI pipeline analysis (MISRA workflow, linker map check, WCET measurement)
- **Status**: draft | reviewed | approved | implemented | verified

### 3.2 Scope

These requirements apply uniformly across all ECUs. They are verified at the project level (CI, static analysis, linker output) rather than per-module unit tests.

---

## 4. Coding Standard Requirements

### SWR-ALL-001: MISRA C:2012 Compliance

- **ASIL**: D
- **Traces up**: SYS-051
- **Traces down**: firmware/**/*.c, firmware/**/*.h
- **Verified by**: CI-MISRA-001 (cppcheck MISRA addon, `.github/workflows/misra.yml`)
- **Verification method**: Static analysis
- **Status**: draft

All firmware source code shall comply with MISRA C:2012 (with Amendment 2 / 2023 updates). Mandatory rules shall have zero violations. Required rule deviations shall follow the formal deviation process documented in `docs/safety/analysis/misra-deviation-register.md`. The MISRA check shall run as a blocking CI gate on every push and pull request.

---

### SWR-ALL-002: Static RAM Only — No Dynamic Allocation

- **ASIL**: D
- **Traces up**: SYS-052
- **Traces down**: firmware/**/*.c
- **Verified by**: CI-MISRA-001 (MISRA C:2012 Rule 21.3 / Directive 4.12 enforcement)
- **Verification method**: Static analysis
- **Status**: draft

All firmware shall use static memory allocation only. The functions `malloc`, `calloc`, `realloc`, and `free` (and their variants) shall not appear in any production firmware source file. All buffers, queues, and data structures shall be statically sized at compile time. This is enforced by MISRA C:2012 Rule 21.3 (Required) and Directive 4.12 (Required), both checked by the MISRA CI pipeline.

---

## 5. Resource Budget Requirements

### SWR-ALL-003: WCET Within Deadline Margin

- **ASIL**: D
- **Traces up**: SYS-053
- **Traces down**: firmware/**/*.c
- **Verified by**: CI-TIMING-001 (WCET measurement on target hardware)
- **Verification method**: Analysis (measurement-based WCET)
- **Status**: draft

The worst-case execution time (WCET) of each safety-critical task shall not exceed 80% of its scheduling deadline. For the 10 ms main control loop on each ECU, WCET shall be below 8 ms. WCET shall be measured on target hardware under worst-case conditions (all interrupts enabled, maximum CAN bus load, all fault paths active) and documented in the timing analysis report.

---

### SWR-ALL-004: Flash Memory Utilization Below 80%

- **ASIL**: QM
- **Traces up**: SYS-054
- **Traces down**: firmware/**/*.c
- **Verified by**: CI-SIZE-001 (linker map analysis in build pipeline)
- **Verification method**: Analysis (linker output)
- **Status**: draft

Flash memory utilization shall not exceed 80% of the available flash on each target MCU (STM32G474RE: 512 KB, TMS570LC43x: 4 MB). The linker map output shall be checked as part of the CI build process. Exceeding the 80% threshold shall generate a warning; exceeding 90% shall block the merge.

---

## 6. Requirements Summary

| Category | Count | SWR Range |
|----------|-------|-----------|
| Coding Standards | 2 | SWR-ALL-001 to SWR-ALL-002 |
| Resource Budgets | 2 | SWR-ALL-003 to SWR-ALL-004 |
| **Total** | **4** | SWR-ALL-001 to SWR-ALL-004 |
