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

# MAN.3 Execution Roadmap

Process area: MAN.3 Project Management
Source: `docs/plans/master-plan.md`
Plan horizon: 19.5 working days (stretch 25 to 30 days)

## Milestones and Gates

1. Gate G1 Safety Baseline
- [ ] SYS.1 deliverables approved
- [ ] SYS.2 interface baseline approved
2. Gate G2 Software Baseline
- [ ] Shared BSW integrated into CVC/FZC/RZC
- [ ] Safety Controller minimum behavior validated
3. Gate G3 Integration Baseline
- [ ] vECUs running in SIL
- [ ] Gateway cloud and alert path operational
4. Gate G4 Release Baseline
- [ ] xIL evidence complete
- [ ] HIL evidence complete
- [ ] safety case and release assets complete

## Planned Sequence

1. Days 1 to 4
- [ ] SYS.1 and SYS.2 foundations
2. Days 5 to 10
- [ ] SWE.1/2 and SWE.3 (BSW + physical ECUs)
3. Days 11 to 14
- [ ] SWE.3 (vECU + gateway) and early SWE.4
4. Days 15 to 19.5
- [ ] SWE.4/5/6, HIL, release packaging

## Weekly Execution Checklist

## Week 1

- [ ] Freeze scope for v1.0.0
- [ ] Complete safety documents and traceability draft
- [ ] Freeze CAN message matrix and HSI baseline

## Week 2

- [ ] Complete shared BSW done criteria
- [ ] Complete CVC/FZC/RZC/SC done criteria
- [ ] Publish first integration evidence snippets

## Week 3

- [ ] Complete vECU + gateway done criteria
- [ ] Complete xIL + HIL evidence package
- [ ] Finalize safety case, README, tag plan

## Metrics (Track Daily)

- [ ] Planned vs completed tasks (count)
- [ ] Blocked tasks (count and age)
- [ ] Defect discovery rate (new/day)
- [ ] Reopen rate (%)
- [ ] Evidence completion (%)

## Risk and Escalation Rules

- [ ] Toolchain blocker over 2 hours -> switch to fallback path and log in issue tracker
- [ ] Hardware blocker over 1 day -> continue SIL/PIL track and update dependencies
- [ ] Scope increase request -> evaluate against G4 date before acceptance

## Tracking Files

- `docs/aspice/plans/MAN.3-project-management/progress-dashboard.md`
- `docs/aspice/plans/MAN.3-project-management/weekly-status-2026-W08.md`
- `docs/aspice/plans/MAN.3-project-management/risk-register.md`
- `docs/aspice/plans/MAN.3-project-management/issue-log.md`
- `docs/aspice/plans/MAN.3-project-management/decision-log.md`
- `docs/aspice/plans/MAN.3-project-management/gate-readiness-checklist.md`

