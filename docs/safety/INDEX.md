---
document_id: SAFETY-INDEX
title: "Safety Documentation Index"
version: "0.1"
status: draft
iso_26262_parts: "2, 3, 4, 5, 6, 9"
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

## Concept Phase (Part 3)

| Document | Path | Status |
|----------|------|--------|
| Item Definition | [concept/item-definition.md](concept/item-definition.md) | Planned |
| HARA | [concept/hara.md](concept/hara.md) | Planned |
| Safety Goals | [concept/safety-goals.md](concept/safety-goals.md) | Planned |
| Functional Safety Concept | [concept/functional-safety-concept.md](concept/functional-safety-concept.md) | Planned |

## Safety Management (Part 2)

| Document | Path | Status |
|----------|------|--------|
| Safety Plan | [plan/safety-plan.md](plan/safety-plan.md) | Planned |
| Safety Case | [plan/safety-case.md](plan/safety-case.md) | Planned |

## Safety Analysis (Parts 5, 9)

| Document | Path | Status |
|----------|------|--------|
| FMEA | [analysis/fmea.md](analysis/fmea.md) | Planned |
| DFA | [analysis/dfa.md](analysis/dfa.md) | Planned |
| Hardware Metrics | [analysis/hardware-metrics.md](analysis/hardware-metrics.md) | Planned |
| ASIL Decomposition | [analysis/asil-decomposition.md](analysis/asil-decomposition.md) | Planned |

## Safety Requirements (Parts 3-6)

| Document | Path | Status |
|----------|------|--------|
| Functional Safety Requirements | [requirements/functional-safety-reqs.md](requirements/functional-safety-reqs.md) | Planned |
| Technical Safety Requirements | [requirements/technical-safety-reqs.md](requirements/technical-safety-reqs.md) | Planned |
| SW Safety Requirements | [requirements/sw-safety-reqs.md](requirements/sw-safety-reqs.md) | Planned |
| HW Safety Requirements | [requirements/hw-safety-reqs.md](requirements/hw-safety-reqs.md) | Planned |
| HSI Specification | [requirements/hsi-specification.md](requirements/hsi-specification.md) | Planned |

## Safety Validation (Part 4)

| Document | Path | Status |
|----------|------|--------|
| Safety Validation Report | [validation/safety-validation-report.md](validation/safety-validation-report.md) | Planned |

## Traceability

Safety Goal → FSR → TSR → SSR/HSR → Code → Test

See [../traceability/traceability-matrix.md](../traceability/traceability-matrix.md)

