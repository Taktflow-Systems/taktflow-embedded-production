---
document_id: SAFETY-INDEX
title: "Safety Documentation Index"
version: "0.2"
status: draft
iso_26262_parts: "2, 3, 4, 5, 6, 9"
date: 2026-07-06
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


# Safety Documentation Index

Master index for all ISO 26262 safety work products.

## Current Safety Assessment

This index reflects the 2026-07-06 document review. It is not a release approval.
The safety work products are usable as a draft safety lifecycle package, but the
safety case remains conditional until the open verification, timing, hardware
metric, and confirmation-review gaps are closed.

| Area | Current State | Release Gap |
|------|---------------|-------------|
| Concept phase | Item definition, HARA, safety goals, and FSC exist as draft evidence. Safety goals now map HE-001 through HE-020 into SG-001 through SG-008. | Close reviewer comments and confirm the grouping rationale for broad goals such as SG-008. |
| Traceability | Safety trace matrix reports 353 requirements, 100% test linkage, and 86% full implementation-plus-test trace. ASPICE trace matrix reports SG/FSR/TSR coverage at 100%. | Refresh generated matrices after current implementation changes; close partial implementation trace, HSR down-trace, and SWR test gaps. |
| Verification | Unit and integration reports contain executed evidence; xIL and system-level reports exist but are not all scenario-complete. | Record scenario-level SIL/PIL/HIL/system verification evidence with logs, build baseline, and pass/fail results. |
| Timing and safety analysis | FMEA, DFA, FTTI, CAN dataflow, and timing audits exist. | Resolve open FTTI/E2E timing findings and document residual risk acceptance where local safety mechanisms, not CAN feedback, close the FTTI. |
| Safety management | Safety plan and QA plan exist with role, activity, and confirmation-measure structure. | Add confirmation review records with required independence levels and release gate decisions. |

## Concept Phase (Part 3)

| Document | Path | Status |
|----------|------|--------|
| Item Definition | [concept/item-definition.md](concept/item-definition.md) | Draft |
| HARA | [concept/hara.md](concept/hara.md) | Draft |
| Safety Goals | [concept/safety-goals.md](concept/safety-goals.md) | Draft |
| Functional Safety Concept | [concept/functional-safety-concept.md](concept/functional-safety-concept.md) | Draft |

## Safety Management (Part 2)

| Document | Path | Status |
|----------|------|--------|
| Safety Plan | [plan/safety-plan.md](plan/safety-plan.md) | Draft |
| Safety Case | [plan/safety-case.md](plan/safety-case.md) | Draft / conditional |

## Safety Analysis (Parts 5, 9)

| Document | Path | Status |
|----------|------|--------|
| FMEA | [analysis/fmea.md](analysis/fmea.md) | Draft |
| OS SW-FMEA | [analysis/os-fmea.md](analysis/os-fmea.md) | Draft |
| DFA | [analysis/dfa.md](analysis/dfa.md) | Draft |
| Hardware Metrics | [analysis/hardware-metrics.md](analysis/hardware-metrics.md) | Draft / quantitative data needed |
| ASIL Decomposition | [analysis/asil-decomposition.md](analysis/asil-decomposition.md) | Draft |
| Complete FTTI Analysis | [analysis/ftti-complete.md](analysis/ftti-complete.md) | Draft / open findings |
| MISRA Deviation Register | [analysis/misra-deviation-register.md](analysis/misra-deviation-register.md) | Active |

## Safety Manuals (Part 6, Clause 9)

| Document | Path | Status |
|----------|------|--------|
| OS SC3 Protection Safety Manual | [os-sc3-safety-manual.md](os-sc3-safety-manual.md) | Draft |

## Safety Requirements (Parts 3-6)

| Document | Path | Status |
|----------|------|--------|
| Functional Safety Requirements | [requirements/functional-safety-reqs.md](requirements/functional-safety-reqs.md) | Draft |
| Technical Safety Requirements | [requirements/technical-safety-reqs.md](requirements/technical-safety-reqs.md) | Draft |
| SW Safety Requirements | [requirements/sw-safety-reqs.md](requirements/sw-safety-reqs.md) | Draft / path refresh needed |
| HW Safety Requirements | [requirements/hw-safety-reqs.md](requirements/hw-safety-reqs.md) | Draft |
| HSI Specification | [requirements/hsi-specification.md](requirements/hsi-specification.md) | Draft |

## Safety Validation (Part 4)

| Document | Path | Status |
|----------|------|--------|
| Safety Validation Report | [validation/safety-validation-report.md](validation/safety-validation-report.md) | Planned / HIL evidence pending |

## Traceability

Safety Goal → FSR → TSR → SSR/HSR → Code → Test

Primary safety traceability: [verification/traceability-matrix.md](verification/traceability-matrix.md)

Full ASPICE V-model traceability: [../aspice/traceability/traceability-matrix.md](../aspice/traceability/traceability-matrix.md)

Current readiness review: [audit/iso26262-aspice-readiness-review-2026-07-06.md](audit/iso26262-aspice-readiness-review-2026-07-06.md)

