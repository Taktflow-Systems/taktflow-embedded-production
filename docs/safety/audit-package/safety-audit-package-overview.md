---
document_id: AEP-001
title: "Audit Evidence Package â€” Taktflow Embedded Platform"
version: "1.0"
status: interim
standard: "ISO 26262:2018 (ASIL D) + Automotive SPICE 4.0"
date: 2026-02-24
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


# Audit Evidence Package â€” Taktflow Embedded Platform

| Field | Value |
|-------|-------|
| **Document ID** | AEP-001 |
| **Standard** | ISO 26262:2018 (ASIL D) + Automotive SPICE 4.0 |
| **Date** | 2026-02-24 |
| **Status** | Interim (portfolio demonstration) |

## Executive Summary

The Taktflow Embedded Platform is a 7-ECU zonal vehicle architecture comprising 4 physical microcontrollers (STM32 and TMS570) and 3 simulated ECUs running in Docker containers. The platform implements an AUTOSAR-like Base Software (BSW) stack, CAN bus communication with E2E protection, and targets ASIL D safety integrity per ISO 26262:2018. This document serves as the master evidence index for the complete safety evidence package, guiding an assessor from concept-phase safety analysis through implementation and verification.

---

## Evidence Index (V-Model Left-to-Right)

### Concept Phase (ISO 26262 Part 3)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| Concept | Item Definition | `docs/safety/concept/item-definition.md` | Complete |
| Concept | Hazard Analysis & Risk Assessment (HARA) | `docs/safety/concept/hara.md` | Complete |
| Concept | Safety Goals | `docs/safety/concept/safety-goals.md` | Complete |
| Concept | Functional Safety Concept | `docs/safety/concept/functional-safety-concept.md` | Complete |

### Safety Planning (ISO 26262 Part 2)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| Planning | Safety Plan | `docs/safety/plan/safety-plan.md` | Complete |
| Planning | Safety Case | `docs/safety/plan/safety-case.md` | Complete |

### Safety Analysis (ISO 26262 Part 9)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| Analysis | FMEA | `docs/safety/analysis/fmea.md` | Complete |
| Analysis | Dependent Failure Analysis (DFA) | `docs/safety/analysis/dfa.md` | Complete |
| Analysis | ASIL Decomposition | `docs/safety/analysis/asil-decomposition.md` | Complete |
| Analysis | Hardware Metrics | `docs/safety/analysis/hardware-metrics.md` | Complete |
| Analysis | MISRA Deviation Register | `docs/safety/analysis/misra-deviation-register.md` | Complete (2 deviations) |

### Requirements (ISO 26262 Parts 3-6, ASPICE SYS.2/SWE.1)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| Requirements | Functional Safety Requirements | `docs/safety/requirements/functional-safety-reqs.md` | Complete |
| Requirements | Technical Safety Requirements | `docs/safety/requirements/technical-safety-reqs.md` | Complete |
| Requirements | SW Safety Requirements | `docs/safety/requirements/sw-safety-reqs.md` | Complete |
| Requirements | HW Safety Requirements | `docs/safety/requirements/hw-safety-reqs.md` | Complete |
| Requirements | HSI Specification | `docs/safety/requirements/hsi-specification.md` | Complete |
| Requirements | SW Requirements (per ECU) | `docs/aspice/software/sw-requirements/SWR-*.md` | Complete (195 requirements) |

### System Architecture (ASPICE SYS.3)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| System Architecture | System Architecture | `docs/aspice/system/` | Complete |
| System Architecture | CAN Message Matrix | `docs/aspice/system/can-message-matrix.md` | Complete |

### Software Architecture (ASPICE SWE.2)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| SW Architecture | SW Architecture | `docs/aspice/software/` | Complete |

### Implementation (ASPICE SWE.3)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| Implementation | BSW Source Code | `firmware/shared/bsw/` | Complete (24 modules) |
| Implementation | ECU Application Code | `firmware/cvc/`, `fzc/`, `rzc/`, `sc/` | Complete (4 physical ECUs) |
| Implementation | MISRA Compliance | 0 violations, CI blocking | Complete |

### Unit Verification (ASPICE SWE.4, ISO 26262 Part 6 Tables 7-9)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| Unit Verification | Unit Test Suite | `firmware/shared/bsw/test/` | Complete (18 files, 443 tests) |
| Unit Verification | Unit Test Report | `docs/aspice/verification/unit-test/unit-test-report.md` | Complete |
| Unit Verification | Code Coverage | `make coverage` (gcov/lcov) | Configured |
| Unit Verification | MISRA Static Analysis | `.github/workflows/misra.yml` | CI green |

### Integration Verification (ASPICE SWE.5)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| Integration Verification | Integration Test Plan | `docs/aspice/verification/integration-test/integration-test-plan.md` | Complete |
| Integration Verification | Integration Test Suite | `test/integration/` | Complete (11 files, 60 tests) |
| Integration Verification | Integration Test Report | `docs/aspice/verification/integration-test/integration-test-report.md` | Complete |

### SIL Verification (ISO 26262 Part 6)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| SIL Verification | SIL Orchestrator | `test/sil/run_sil.sh` | Complete |
| SIL Verification | SIL Verdict Checker | `test/sil/verdict_checker.py` (21 verdict types) | Complete |
| SIL Verification | SIL Scenarios | `test/sil/scenarios/` | Complete (15 scenarios) |
| SIL Verification | SIL CI Nightly | `.github/workflows/sil-nightly.yml` | Complete |
| SIL Verification | SIL README | `test/sil/apps-web-overview.md` | Complete |

### Traceability (ASPICE SWE.4-6, ISO 26262 Part 6)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| Traceability | Automated Traceability Script | `scripts/gen-traceability.sh` | Complete |
| Traceability | Traceability Matrix | `docs/aspice/verification/traceability-matrix.md` | Auto-generated |

### Tool Qualification (ISO 26262 Part 8)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| Tool Qualification | GCC Qualification | `docs/aspice/verification/tool-qualification/tool-qual-gcc.md` | Complete (TCL2) |
| Tool Qualification | Unity Qualification | `docs/aspice/verification/tool-qualification/tool-qual-unity.md` | Complete (TCL1) |
| Tool Qualification | cppcheck Qualification | `docs/aspice/verification/tool-qualification/tool-qual-cppcheck.md` | Complete (TCL2) |
| Tool Qualification | gcov Qualification | `docs/aspice/verification/tool-qualification/tool-qual-gcov.md` | Complete (TCL2) |

### Configuration Management (ASPICE SUP.8)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| CM | CM Strategy | `docs/aspice/cm/` | Complete |
| CM | Baselines | Git tags (vX.Y.Z) | Active |
| CM | CI Pipeline | `.github/workflows/` | Green |

### Safety Validation (ISO 26262 Part 4)

| Phase | Document | Location | Status |
|-------|----------|----------|--------|
| Safety Validation | Safety Validation Report | `docs/safety/validation/safety-validation-report.md` | Portfolio scope |

---

## Cross-Reference Matrix

| Standard | Clause / Process | Evidence |
|----------|-----------------|----------|
| ISO 26262-2 | Safety Management | Safety Plan, Safety Case |
| ISO 26262-3 | Concept Phase | HARA, Safety Goals, Functional Safety Concept |
| ISO 26262-4 | System Level | Technical Safety Requirements, System Architecture |
| ISO 26262-5 | Hardware Level | Hardware Metrics, HW Safety Requirements, HSI Specification |
| ISO 26262-6 | Software Level | SW Safety Requirements, SW Requirements, Source Code, Unit Tests, Coverage |
| ISO 26262-8 | Supporting Processes | Tool Qualification, Configuration Management |
| ISO 26262-9 | Safety Analyses | FMEA, DFA, ASIL Decomposition, MISRA Deviation Register |
| ASPICE SWE.4 | Unit Verification | Unit Test Suite (443 tests), Coverage (gcov/lcov), MISRA Static Analysis |
| ASPICE SWE.5 | Integration Verification | Integration Test Suite (60 tests), SIL Scenarios (15), SIL Verdict Checker |
| ASPICE SUP.8 | Configuration Management | Git version control, Baselines (tags), CI Pipeline |

---

## Assessor Notes

1. This is a **portfolio project** demonstrating ASIL D development capability across the full V-model lifecycle. It is not a production vehicle system.
2. **No external independent assessment (I3)** has been performed. ISO 26262 requires I3-level independence for ASIL D functional safety assessments; this would be provided by a certification body (e.g., TUV, SGS, exida) in a production programme.
3. **PIL/HIL testing** requires physical hardware targets and is out of scope for this portfolio demonstration. The SIL layer provides the highest level of automated verification achievable without physical ECUs.
4. **Safety validation at vehicle level** (ISO 26262 Part 4) requires a physical system operating in its intended environment. The safety validation report documents the validation strategy for a production context.
5. All evidence is maintained under Git version control with CI enforcement, providing full change history and baseline management per ASPICE SUP.8.

