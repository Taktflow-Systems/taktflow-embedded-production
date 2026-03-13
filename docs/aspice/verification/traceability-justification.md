---
document_id: TRACE-JUST
title: "Traceability Justification — Constraint Requirements Without SWR Decomposition"
version: "1.0"
status: draft
aspice_process: SWE.1, SYS.2
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


# Traceability Justification — Constraint Requirements

## 1. Purpose

This document provides formal justification for system requirements that do not decompose into individual SWR-level software requirements. Per ISO 26262 Part 6 and ASPICE SWE.1, every system requirement should trace to at least one software requirement or have a documented rationale for why SWR decomposition is not applicable.

The four requirements below are **process/constraint requirements** — they constrain how the software is built and verified rather than specifying what the software does. They are verified through static analysis, build process inspection, and audit, not through runtime software behavior.

## 2. Justified Requirements

### SYS-051: MISRA C Compliance

| Field | Value |
|-------|-------|
| **SYS ID** | SYS-051 |
| **Title** | MISRA C Compliance |
| **Safety relevance** | ASIL D |
| **Traces up** | STK-028, STK-030 |
| **Traces down** | SWR-ALL-001 (applies to all firmware modules) |
| **Verification method** | Static analysis (cppcheck with MISRA addon) |
| **Verification artifact** | CI pipeline: `.github/workflows/misra.yml` |
| **Justification** | This is a coding standard constraint applied uniformly to all firmware source files. It is not decomposed into per-module SWR requirements because MISRA compliance is a cross-cutting process requirement. Compliance is enforced by the CI pipeline (cppcheck, error-exitcode=1) and documented in the MISRA deviation register (`docs/safety/analysis/misra-deviation-register.md`). The SWR-ALL-001 identifier in system-requirements.md represents this cross-cutting constraint. |
| **Residual risk** | None — enforcement is automated and blocking. |

---

### SYS-052: Static RAM Only — No Dynamic Allocation

| Field | Value |
|-------|-------|
| **SYS ID** | SYS-052 |
| **Title** | Static RAM Only — No Dynamic Allocation |
| **Safety relevance** | ASIL D |
| **Traces up** | STK-016, STK-030 |
| **Traces down** | SWR-ALL-002 (applies to all firmware modules) |
| **Verification method** | Code inspection + linker map analysis |
| **Verification artifact** | Linker map output (build process), grep for malloc/calloc/realloc/free |
| **Justification** | This is an architectural constraint prohibiting dynamic memory allocation in all production firmware. It is not decomposed into per-module SWR requirements because the prohibition is universal. Verification is performed by: (1) grep-based scan for banned function calls in CI, (2) linker map confirming heap size is zero, (3) MISRA Rule 21.3 / Directive 4.12 enforcement via static analysis. The SWR-ALL-002 identifier represents this cross-cutting constraint. |
| **Residual risk** | None — both static analysis and linker configuration enforce this. |

---

### SYS-054: Flash Memory Utilization ≤ 80%

| Field | Value |
|-------|-------|
| **SYS ID** | SYS-054 |
| **Title** | Flash Memory Utilization ≤ 80% |
| **Safety relevance** | QM |
| **Traces up** | STK-021 |
| **Traces down** | SWR-ALL-004 (applies to all firmware modules) |
| **Verification method** | Linker output analysis |
| **Verification artifact** | CI build output: firmware binary size vs flash partition size |
| **Justification** | This is a resource budget constraint on total firmware binary size per MCU. It is not decomposed into per-module SWR requirements because it is a system-level aggregate metric, not a functional behavior. Verification is performed by checking the linker map output after each build: total .text + .rodata + .data must be below 80% of flash capacity (STM32G474RE: 512 KB, TMS570LC43x: 4 MB). The SWR-ALL-004 identifier represents this cross-cutting constraint. |
| **Residual risk** | Low — flash utilization is visible in every build output. |

---

### SYS-055: Bidirectional Traceability Chain

| Field | Value |
|-------|-------|
| **SYS ID** | SYS-055 |
| **Title** | Bidirectional Traceability Chain |
| **Safety relevance** | ASIL D |
| **Traces up** | STK-001, STK-002, STK-028, STK-029, STK-031 |
| **Traces down** | (this requirement governs the traceability process itself) |
| **Verification method** | Inspection / audit |
| **Verification artifact** | `scripts/gen-traceability.sh`, `scripts/trace-slice.py`, traceability-matrix.md |
| **Justification** | This is a meta-requirement that governs the traceability process itself. Decomposing it into SWR requirements would create a circular dependency (an SWR requirement about the process that creates SWR requirements). Instead, this requirement is verified by: (1) the automated traceability matrix generator (`gen-traceability.sh --check`), (2) the trace-slice scripts that verify STK→SYS and SYS→SWR coverage, (3) manual inspection of the generated traceability-matrix.md for completeness. The CI pipeline enforces 100% SWR→code→test coverage. |
| **Residual risk** | Low — automated scripts detect orphans and gaps. |

---

## 3. Stakeholder Requirement Without SYS Decomposition

### STK-027: Reproducible Build and Demo Process

| Field | Value |
|-------|-------|
| **STK ID** | STK-027 |
| **Title** | Reproducible Build and Demo Process |
| **SYS child** | None — process requirement |
| **Verification method** | Inspection + demonstration |
| **Verification artifact** | `Makefile`, `scripts/`, `docker/`, CI pipeline |
| **Justification** | STK-027 is a project infrastructure and process requirement, not a system-level technical requirement. It requires that the build system produces reproducible binaries and that the demo environment can be reliably set up. This is addressed by: (1) the Makefile-based build system with pinned toolchain versions, (2) Docker-based simulation environment for the 3 simulated ECUs, (3) CI pipeline that builds and tests on every commit, (4) `scripts/deploy.sh` for reproducible deployment. No SYS requirement is needed because this is a development process concern, not a system behavior. |
| **Residual risk** | Low — CI provides continuous verification of build reproducibility. |

---

## 4. Summary

| Requirement | Type | Decomposition | Verification |
|-------------|------|---------------|--------------|
| SYS-051 | Coding standard constraint | SWR-ALL-001 (cross-cutting) | Static analysis CI (cppcheck MISRA) |
| SYS-052 | Architectural constraint | SWR-ALL-002 (cross-cutting) | Linker map + static analysis |
| SYS-054 | Resource budget constraint | SWR-ALL-004 (cross-cutting) | Build output analysis |
| SYS-055 | Process meta-requirement | N/A (self-referential) | Traceability scripts + audit |
| STK-027 | Process/infrastructure | N/A (no SYS needed) | CI pipeline + Makefile + Docker |

All requirements have automated or documented verification mechanisms. No traceability gap exists — these constraints are enforced through process, tooling, and infrastructure rather than through individual software functions.
