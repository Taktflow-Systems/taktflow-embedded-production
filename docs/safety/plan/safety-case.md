---
document_id: SC-DOC
title: "Safety Case"
version: "0.1"
status: planned
iso_26262_part: 2
iso_26262_clause: "7"
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


# Safety Case

<!-- Phase 14 deliverable — see docs/plans/master-plan.md Phase 14 -->

## Purpose

Structured argument that the system achieves adequate safety, supported by evidence. Per ISO 26262-2 Clause 7.

## Structure

### 1. Claims
- Safety goals achieved
- Safety requirements satisfied

### 2. Argument
- Logical reasoning connecting evidence to claims
- Method: Goal Structuring Notation (GSN) or narrative

### 3. Evidence
- HARA results
- Safety analyses (FMEA, FTA, DFA)
- Verification reports (unit, integration, system)
- Test reports (MIL, SIL, PIL, HIL)
- Configuration management records
- Traceability matrix

## Versions

| Version | Phase | Scope |
|---------|-------|-------|
| Preliminary | After Phase 1 | Concept phase evidence |
| Interim | After Phase 12 | Development evidence |
| Final | After Phase 14 | Complete evidence package |

