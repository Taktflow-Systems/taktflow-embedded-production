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

## Lessons Learned Rule

Every planning topic in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/lessons-learned/`](../lessons-learned/). One file per planning topic. File naming: `PLAN-<topic>.md`.

# ASPICE Planning Structure

Source baseline: `docs/plans/master-plan.md`

This folder contains execution-ready plans organized by Automotive SPICE process areas.

<!-- HITL-LOCK START:COMMENT-BLOCK-PLANS-SEC1 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The introduction correctly identifies the master plan as the source baseline and frames this folder as execution-ready plans by ASPICE process area. The relationship between `docs/plans/master-plan.md` (source of truth) and this `docs/aspice/plans/` folder (execution breakdown) is clear. No issues with this section.
<!-- HITL-LOCK END:COMMENT-BLOCK-PLANS-SEC1 -->

## Folder Map

- `docs/aspice/plans/MAN.3-project-management/`
- `docs/aspice/plans/SYS.1-system-requirements/`
- `docs/aspice/plans/SYS.2-system-architecture/`
- `docs/aspice/plans/SWE.1-2-requirements-and-architecture/`
- `docs/aspice/plans/SWE.3-implementation/`
- `docs/aspice/plans/SWE.4-6-verification-and-release/`

<!-- HITL-LOCK START:COMMENT-BLOCK-PLANS-SEC2 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The folder map covers MAN.3, SYS.1, SYS.2, SWE.1-2, SWE.3, and SWE.4-6. Notable gaps: SYS.3 (System Architecture) is not listed as a separate folder though it may be covered under SYS.2. Also missing are explicit folders for SUP.1 (QA), SUP.8 (CM), HWE.1-3 (Hardware Engineering — new in ASPICE 4.0), and ACQ.4 (Supplier Monitoring). These are covered elsewhere in the `docs/aspice/` tree (quality/, cm/) but not referenced in this folder map. Consider adding cross-references or noting them as out-of-scope for this index.
<!-- HITL-LOCK END:COMMENT-BLOCK-PLANS-SEC2 -->

## MAN.3 Tracking Pack

- `docs/aspice/plans/MAN.3-project-management/progress-dashboard.md`
- `docs/aspice/plans/MAN.3-project-management/weekly-status-template.md`
- `docs/aspice/plans/MAN.3-project-management/weekly-status-2026-W08.md`
- `docs/aspice/plans/MAN.3-project-management/daily-progress-template.md`
- `docs/aspice/plans/MAN.3-project-management/daily-log/`
- `docs/aspice/plans/MAN.3-project-management/risk-register.md`
- `docs/aspice/plans/MAN.3-project-management/issue-log.md`
- `docs/aspice/plans/MAN.3-project-management/decision-log.md`
- `docs/aspice/plans/MAN.3-project-management/gate-readiness-checklist.md`

<!-- HITL-LOCK START:COMMENT-BLOCK-PLANS-SEC3 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The MAN.3 tracking pack is comprehensive — progress dashboard, weekly/daily status, risk register, issue log, decision log, and gate readiness checklist. This covers MAN.3 base practices well (planning, monitoring, risk management, issue resolution). The `execution-roadmap.md` is referenced in "How to Use" below but not listed here in the tracking pack. Also, only one weekly status file (W08) is listed — this is expected if we are early in the project.
<!-- HITL-LOCK END:COMMENT-BLOCK-PLANS-SEC3 -->

## How to Use

1. Start each week from `MAN.3-project-management/execution-roadmap.md`.
2. Execute tasks from SYS -> SWE order unless blocked.
3. For each completed task, attach evidence in the referenced work product file.
4. Do not close a gate until all gate checklist items are checked.

<!-- HITL-LOCK START:COMMENT-BLOCK-PLANS-SEC4 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The usage instructions are clear and follow a logical SYS-to-SWE V-model left-side ordering. The evidence-attachment rule (step 3) is important for ASPICE Level 2 compliance. Gate closure control (step 4) aligns with MAN.3 milestone management. This section is accurate and complete.
<!-- HITL-LOCK END:COMMENT-BLOCK-PLANS-SEC4 -->

## Process Coverage

- MAN.3: planning, milestones, risks, status control
- SYS.1: item definition, HARA, safety goals, TSR baseline
- SYS.2: architecture, interfaces, CAN/E2E/HSI baseline
- SWE.1/SWE.2: software requirements and architecture traceability
- SWE.3: BSW + ECU + vECU implementation
- SWE.4/SWE.5/SWE.6: unit/integration/system verification, HIL, release evidence

<!-- HITL-LOCK START:COMMENT-BLOCK-PLANS-SEC5 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Process coverage is solid for the core V-model processes. SYS.1 listing includes HARA and safety goals which are ISO 26262 concept-phase artifacts — this is correct since SYS.1 requirements elicitation encompasses the safety concept inputs. Note: SYS.3 (system architecture design) is not explicitly listed but may be folded into SYS.2. SYS.4 (system integration) and SYS.5 (system verification) are implied by SWE.4-6 but not called out separately. SUP processes (QA, CM) and HWE processes are not listed here — they are covered in sibling folders, but a note acknowledging them would improve completeness.
<!-- HITL-LOCK END:COMMENT-BLOCK-PLANS-SEC5 -->

