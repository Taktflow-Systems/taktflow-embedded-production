---
document_id: INDEX
title: "Document Registry — Taktflow Embedded"
version: "0.2"
status: draft
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


# Document Registry — Taktflow Embedded

Master index for all project documentation. Organized by Automotive SPICE 4.0 process area and ISO 26262 part.

Every document must be registered here. If it is not in this index, it does not exist for assessment purposes.

---

## V-Model — Process Mapping

```
        Specification                                    Verification
        ─────────────                                    ────────────

  SYS.1  Requirements Elicitation
     │
  SYS.2  System Requirements Analysis    <─────────>  SYS.5  System Verification
     │                                                    │
  SYS.3  System Architectural Design     <─────────>  SYS.4  System Integration Test
     │                                                    │
  SWE.1  SW Requirements Analysis        <─────────>  SWE.6  SW Qualification Test
     │                                                    │
  SWE.2  SW Architectural Design         <─────────>  SWE.5  SW Integration Test
     │                                                    │
  SWE.3  SW Detailed Design & Unit       <─────────>  SWE.4  SW Unit Verification
                Construction
```

---

## Document Inventory

### MAN.3 — Project Management

| Document | Path | Status |
|----------|------|--------|
| Master Plan | [plans/master-plan.md](plans/master-plan.md) | draft |
| HIL Gap Analysis | [plans/gap-analysis-hil-bench-vs-professional.md](plans/gap-analysis-hil-bench-vs-professional.md) | draft |
| Platform Abstraction Cleanup | [plans/plan-platform-abstraction-cleanup.md](plans/plan-platform-abstraction-cleanup.md) | done |
| Execution Roadmap | [aspice/plans/MAN.3-project-management/execution-roadmap.md](aspice/plans/MAN.3-project-management/execution-roadmap.md) | active |
| Weekly Status Template | [aspice/plans/MAN.3-project-management/weekly-status-template.md](aspice/plans/MAN.3-project-management/weekly-status-template.md) | active |
| Weekly Status (Current) | [aspice/plans/MAN.3-project-management/weekly-status-2026-W08.md](aspice/plans/MAN.3-project-management/weekly-status-2026-W08.md) | active |
| Daily Progress Template | [aspice/plans/MAN.3-project-management/daily-progress-template.md](aspice/plans/MAN.3-project-management/daily-progress-template.md) | active |
| Daily Log (Latest) | [aspice/plans/MAN.3-project-management/daily-log/2026-02-21.md](aspice/plans/MAN.3-project-management/daily-log/2026-02-21.md) | active |
| Progress Dashboard | [aspice/plans/MAN.3-project-management/progress-dashboard.md](aspice/plans/MAN.3-project-management/progress-dashboard.md) | active |
| Risk Register | [aspice/plans/MAN.3-project-management/risk-register.md](aspice/plans/MAN.3-project-management/risk-register.md) | draft |
| Issue Log | [aspice/plans/MAN.3-project-management/issue-log.md](aspice/plans/MAN.3-project-management/issue-log.md) | active |
| Decision Log | [aspice/plans/MAN.3-project-management/decision-log.md](aspice/plans/MAN.3-project-management/decision-log.md) | active |
| Gate Readiness Checklist | [aspice/plans/MAN.3-project-management/gate-readiness-checklist.md](aspice/plans/MAN.3-project-management/gate-readiness-checklist.md) | active |

### ISO 26262 — Safety (Parts 2-9)

| Document | Path | ISO Part | Status |
|----------|------|----------|--------|
| Safety Sub-Index | [safety/INDEX.md](safety/INDEX.md) | — | draft |
| **Concept Phase (Part 3)** | | | |
| Item Definition | [safety/concept/item-definition.md](safety/concept/item-definition.md) | 3 | planned |
| HARA | [safety/concept/hara.md](safety/concept/hara.md) | 3 | planned |
| Safety Goals | [safety/concept/safety-goals.md](safety/concept/safety-goals.md) | 3 | planned |
| Functional Safety Concept | [safety/concept/functional-safety-concept.md](safety/concept/functional-safety-concept.md) | 3 | planned |
| **Safety Management (Part 2)** | | | |
| Safety Plan | [safety/plan/safety-plan.md](safety/plan/safety-plan.md) | 2 | planned |
| Safety Case | [safety/plan/safety-case.md](safety/plan/safety-case.md) | 2 | planned |
| **Safety Analysis (Parts 5, 9)** | | | |
| FMEA | [safety/analysis/fmea.md](safety/analysis/fmea.md) | 9 | planned |
| Dependent Failure Analysis | [safety/analysis/dfa.md](safety/analysis/dfa.md) | 9 | planned |
| Hardware Metrics | [safety/analysis/hardware-metrics.md](safety/analysis/hardware-metrics.md) | 5 | planned |
| ASIL Decomposition | [safety/analysis/asil-decomposition.md](safety/analysis/asil-decomposition.md) | 9 | planned |
| **Safety Requirements (Parts 3-6)** | | | |
| Functional Safety Requirements | [safety/requirements/functional-safety-reqs.md](safety/requirements/functional-safety-reqs.md) | 3 | planned |
| Technical Safety Requirements | [safety/requirements/technical-safety-reqs.md](safety/requirements/technical-safety-reqs.md) | 4 | planned |
| SW Safety Requirements | [safety/requirements/sw-safety-reqs.md](safety/requirements/sw-safety-reqs.md) | 6 | planned |
| HW Safety Requirements | [safety/requirements/hw-safety-reqs.md](safety/requirements/hw-safety-reqs.md) | 5 | planned |
| HSI Specification | [safety/requirements/hsi-specification.md](safety/requirements/hsi-specification.md) | 4 | planned |
| **Safety Validation (Part 4)** | | | |
| Safety Validation Report | [safety/validation/safety-validation-report.md](safety/validation/safety-validation-report.md) | 4 | planned |

### SYS.1-3 — System Engineering

| Document | Path | Process | Status |
|----------|------|---------|--------|
| Stakeholder Requirements | [aspice/system/stakeholder-requirements.md](aspice/system/stakeholder-requirements.md) | SYS.1 | draft |
| System Requirements | [aspice/system/system-requirements.md](aspice/system/system-requirements.md) | SYS.2 | draft |
| System Architecture | [aspice/system/system-architecture.md](aspice/system/system-architecture.md) | SYS.3 | planned |
| Interface Control Document | [aspice/system/interface-control-doc.md](aspice/system/interface-control-doc.md) | SYS.3 | planned |
| CAN Message Matrix | [aspice/system/can-message-matrix.md](aspice/system/can-message-matrix.md) | SYS.3 | planned |

### SWE.1-2 — Software Engineering

| Document | Path | Process | Status |
|----------|------|---------|--------|
| **SW Requirements (SWE.1)** | | | |
| SWR — CVC | [aspice/software/sw-requirements/SWR-CVC.md](aspice/software/sw-requirements/SWR-CVC.md) | SWE.1 | planned |
| SWR — FZC | [aspice/software/sw-requirements/SWR-FZC.md](aspice/software/sw-requirements/SWR-FZC.md) | SWE.1 | planned |
| SWR — RZC | [aspice/software/sw-requirements/SWR-RZC.md](aspice/software/sw-requirements/SWR-RZC.md) | SWE.1 | planned |
| SWR — SC | [aspice/software/sw-requirements/SWR-SC.md](aspice/software/sw-requirements/SWR-SC.md) | SWE.1 | planned |
| SWR — BCM | [aspice/software/sw-requirements/SWR-BCM.md](aspice/software/sw-requirements/SWR-BCM.md) | SWE.1 | planned |
| SWR — ICU | [aspice/software/sw-requirements/SWR-ICU.md](aspice/software/sw-requirements/SWR-ICU.md) | SWE.1 | planned |
| SWR — TCU | [aspice/software/sw-requirements/SWR-TCU.md](aspice/software/sw-requirements/SWR-TCU.md) | SWE.1 | planned |
| SWR — BSW | [aspice/software/sw-requirements/SWR-BSW.md](aspice/software/sw-requirements/SWR-BSW.md) | SWE.1 | planned |
| **SW Architecture (SWE.2)** | | | |
| SW Architecture | [aspice/software/sw-architecture/sw-architecture.md](aspice/software/sw-architecture/sw-architecture.md) | SWE.2 | draft |
| BSW Architecture | [aspice/software/sw-architecture/bsw-architecture.md](aspice/software/sw-architecture/bsw-architecture.md) | SWE.2 | draft |
| vECU Architecture | [aspice/software/sw-architecture/vecu-architecture.md](aspice/software/sw-architecture/vecu-architecture.md) | SWE.2 | draft |

### HWE.1-3 — Hardware Engineering

| Document | Path | Process | Status |
|----------|------|---------|--------|
| HW Requirements | [aspice/hardware-eng/hw-requirements.md](aspice/hardware-eng/hw-requirements.md) | HWE.1 | draft |
| HW Design | [aspice/hardware-eng/hw-design.md](aspice/hardware-eng/hw-design.md) | HWE.2 | draft |

### SWE.4-6, SYS.4-5 — Verification

| Document | Path | Process | Status |
|----------|------|---------|--------|
| Verification Sub-Index | [aspice/verification/INDEX.md](aspice/verification/INDEX.md) | — | draft |
| Unit Test Plan | [aspice/verification/unit-test/unit-test-plan.md](aspice/verification/unit-test/unit-test-plan.md) | SWE.4 | planned |
| Unit Test Report | [aspice/verification/unit-test/unit-test-report.md](aspice/verification/unit-test/unit-test-report.md) | SWE.4 | planned |
| Integration Strategy | [aspice/verification/integration-test/integration-strategy.md](aspice/verification/integration-test/integration-strategy.md) | SWE.5 | planned |
| Integration Test Report | [aspice/verification/integration-test/integration-test-report.md](aspice/verification/integration-test/integration-test-report.md) | SWE.5 | planned |
| SW Verification Plan | [aspice/verification/sw-qualification/sw-verification-plan.md](aspice/verification/sw-qualification/sw-verification-plan.md) | SWE.6 | planned |
| Release Notes | [aspice/verification/sw-qualification/release-notes.md](aspice/verification/sw-qualification/release-notes.md) | SWE.6 | planned |
| System Integration Report | [aspice/verification/system-integration/system-integration-report.md](aspice/verification/system-integration/system-integration-report.md) | SYS.4 | planned |
| System Verification Report | [aspice/verification/system-verification/system-verification-report.md](aspice/verification/system-verification/system-verification-report.md) | SYS.5 | planned |
| MIL Test Report | [aspice/verification/xil/mil-report.md](aspice/verification/xil/mil-report.md) | SWE.5 | planned |
| SIL Test Report | [aspice/verification/xil/sil-report.md](aspice/verification/xil/sil-report.md) | SWE.5 | planned |
| PIL Test Report | [aspice/verification/xil/pil-report.md](aspice/verification/xil/pil-report.md) | SWE.5 | planned |
| HIL Test Report | [aspice/verification/xil/hil-report.md](aspice/verification/xil/hil-report.md) | SYS.4 | planned |

### SUP.1 — Quality Assurance

| Document | Path | Status |
|----------|------|--------|
| QA Plan | [aspice/quality/qa-plan.md](aspice/quality/qa-plan.md) | planned |

### SUP.8 — Configuration Management

| Document | Path | Status |
|----------|------|--------|
| CM Strategy | [aspice/cm/cm-strategy.md](aspice/cm/cm-strategy.md) | draft |
| Baseline Template | [aspice/cm/baselines/TEMPLATE.md](aspice/cm/baselines/TEMPLATE.md) | template |
| Change Request Template | [aspice/cm/change-requests/TEMPLATE.md](aspice/cm/change-requests/TEMPLATE.md) | template |

### Traceability

| Document | Path | Status |
|----------|------|--------|
| Traceability Matrix | [aspice/traceability/traceability-matrix.md](aspice/traceability/traceability-matrix.md) | planned |

### ASPICE Execution Plans

| Document | Path | Process | Status |
|----------|------|---------|--------|
| Plans README | [aspice/plans/apps-web-overview.md](aspice/plans/apps-web-overview.md) | — | active |
| Safety Workstream | [aspice/plans/SYS.1-system-requirements/safety-workstream.md](aspice/plans/SYS.1-system-requirements/safety-workstream.md) | SYS.1 | active |
| Interfaces & BSW Workstream | [aspice/plans/SYS.2-system-architecture/interfaces-and-bsw-workstream.md](aspice/plans/SYS.2-system-architecture/interfaces-and-bsw-workstream.md) | SYS.2 | active |
| SW Req & Architecture Plan | [aspice/plans/SWE.1-2-requirements-and-architecture/software-requirements-and-architecture-plan.md](aspice/plans/SWE.1-2-requirements-and-architecture/software-requirements-and-architecture-plan.md) | SWE.1-2 | active |
| Firmware Implementation Plan | [aspice/plans/SWE.3-implementation/firmware-ecus-workstream.md](aspice/plans/SWE.3-implementation/firmware-ecus-workstream.md) | SWE.3 | active |
| vECU & Gateway Plan | [aspice/plans/SWE.3-implementation/vecu-gateway-workstream.md](aspice/plans/SWE.3-implementation/vecu-gateway-workstream.md) | SWE.3 | active |
| Verification & Release Plan | [aspice/plans/SWE.4-6-verification-and-release/verification-hil-release-workstream.md](aspice/plans/SWE.4-6-verification-and-release/verification-hil-release-workstream.md) | SWE.4-6 | active |

### Reference

| Document | Path | Status |
|----------|------|--------|
| Research Repository Index | [research/apps-web-overview.md](research/apps-web-overview.md) | active |
| Research Link Log | [research/link-log.md](research/link-log.md) | active |
| Process Playbook | [reference/process-playbook.md](reference/process-playbook.md) | active |
| Lessons Learned | [reference/lessons-learned-security-hardening.md](reference/lessons-learned-security-hardening.md) | active |

### Deliverables

| Document | Path | Status |
|----------|------|--------|
| Deliverable Artifact Catalog | [deliverable-artifact/apps-web-overview.md](deliverable-artifact/apps-web-overview.md) | active |

---

## ASPICE Process Coverage

| Process | ID | Key Document(s) | Coverage |
|---------|----|------------------|----------|
| Requirements Elicitation | SYS.1 | aspice/system/stakeholder-requirements.md | draft |
| System Requirements | SYS.2 | aspice/system/system-requirements.md | draft |
| System Architecture | SYS.3 | aspice/system/system-architecture.md, aspice/system/can-message-matrix.md | planned |
| System Integration | SYS.4 | aspice/verification/system-integration/ | planned |
| System Verification | SYS.5 | aspice/verification/system-verification/ | planned |
| SW Requirements | SWE.1 | aspice/software/sw-requirements/SWR-*.md | planned |
| SW Architecture | SWE.2 | aspice/software/sw-architecture/*.md | planned |
| SW Detail Design | SWE.3 | firmware/*/src/ | planned |
| SW Unit Verification | SWE.4 | aspice/verification/unit-test/, firmware/*/test/ | planned |
| SW Integration | SWE.5 | aspice/verification/integration-test/ | planned |
| SW Qualification | SWE.6 | aspice/verification/sw-qualification/ | planned |
| Project Management | MAN.3 | aspice/plans/MAN.3-project-management/, plans/master-plan.md | active |
| Quality Assurance | SUP.1 | aspice/quality/qa-plan.md | planned |
| Config Management | SUP.8 | aspice/cm/cm-strategy.md | draft |
| HW Requirements | HWE.1 | aspice/hardware-eng/hw-requirements.md | draft |
| HW Design | HWE.2 | aspice/hardware-eng/hw-design.md | draft |

---

## Document Status Summary

| Status | Count | Description |
|--------|-------|-------------|
| active | 8 | Reference docs and execution plans |
| draft | 8 | Content exists, under development |
| planned | 39 | Registered, not yet authored |
| template | 2 | Reusable templates |
| baselined | 0 | Approved, under change control |
| **Total** | **57** | |

---

*Last updated: 2026-03-10*


