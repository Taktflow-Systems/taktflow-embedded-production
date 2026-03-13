---
document_id: MAN3-GATE-CHECKLIST
title: "Gate Readiness Checklist"
version: "0.1"
status: active
updated: "2026-02-21"
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


# Gate Readiness Checklist

## Gate G1 - Safety Baseline

- [ ] SYS.1 safety artifacts drafted
- [ ] SYS.1 safety artifacts reviewed
- [ ] SYS.2 interface baseline drafted
- [ ] Traceability links established for safety requirements
- [ ] Open critical risks have mitigation plans

## Gate G2 - Software Baseline

- [ ] Shared BSW integrated in CVC/FZC/RZC build
- [ ] Safety Controller minimal functionality validated
- [ ] SWE.1/SWE.2 baselines approved
- [ ] Unit verification strategy approved
- [ ] Known blockers assessed and accepted

## Gate G3 - Integration Baseline

- [ ] vECUs operational in SIL
- [ ] Gateway ingest + cloud telemetry path operational
- [ ] Integration test plan approved
- [ ] Diagnostic path (UDS/DTC) demonstrable
- [ ] Integration risks re-assessed

## Gate G4 - Release Baseline

- [ ] xIL evidence package complete
- [ ] HIL evidence package complete
- [ ] Safety case updated with latest evidence
- [ ] Release notes and artifact manifest complete
- [ ] Final go/no-go decision recorded in decision log

