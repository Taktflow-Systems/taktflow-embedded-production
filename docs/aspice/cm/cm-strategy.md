---
document_id: CMS
title: "Configuration Management Strategy"
version: "0.1"
status: draft
aspice_process: SUP.8
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

## Lessons Learned Rule

Every CM topic in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/lessons-learned/`](../lessons-learned/). One file per CM topic. File naming: `CM-<topic>.md`.

# Configuration Management Strategy

## Purpose

Define CM strategy per ASPICE SUP.8: identification, control, status accounting, and auditing of configuration items.

<!-- HITL-LOCK START:COMMENT-BLOCK-CM-SEC1 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The purpose correctly references SUP.8 and lists the four key CM activities (identification, control, status accounting, auditing). This aligns well with SUP.8 BP1 (develop CM strategy). The document covers most SUP.8 base practices but does not explicitly reference BP3 (branch management — though branching is covered), BP7 (report configuration status), or BP9 (manage storage/backup/integrity). Consider adding explicit BP mapping.
<!-- HITL-LOCK END:COMMENT-BLOCK-CM-SEC1 -->

## CM Tool

Git + GitHub — all configuration items are version-controlled.

<!-- HITL-LOCK START:COMMENT-BLOCK-CM-SEC2 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Git + GitHub is appropriate for the project scope. This satisfies SUP.8 BP1 (tool selection). Consider noting access control mechanisms: who has write access to `main`, who can create tags/baselines, and whether branch protection rules are configured in GitHub. This maps to SUP.8 BP4 and BP5 (control modifications).
<!-- HITL-LOCK END:COMMENT-BLOCK-CM-SEC2 -->

## Branching Strategy

Git Flow:
- `main` — protected, tagged releases only (baselines)
- `develop` — integration branch
- `feature/` — one per feature/phase
- `release/` — release candidates
- `hotfix/` — emergency fixes

<!-- HITL-LOCK START:COMMENT-BLOCK-CM-SEC3 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The branching strategy is Git Flow, which aligns with the detailed branching rules in `workflow.md`. This is a well-known pattern for automotive projects that need stable baselines. The strategy correctly protects `main` for tagged releases. One observation: the current repo state shows only a `main` branch — the `develop` branch and Git Flow workflow have not yet been instantiated. This is expected if the project is still in documentation/planning phases.
<!-- HITL-LOCK END:COMMENT-BLOCK-CM-SEC3 -->

## Configuration Items

| Category | Items | Naming |
|----------|-------|--------|
| Source code | firmware/**/*.c, *.h | Per AUTOSAR module naming |
| Safety docs | docs/safety/**/*.md | Document ID in frontmatter |
| ASPICE docs | docs/aspice/**/*.md | Document ID in frontmatter |
| Build scripts | scripts/*, Makefile | Descriptive names |
| Test artifacts | test/**, firmware/*/test/ | TC-{ECU}-{MOD}-NNN |

<!-- HITL-LOCK START:COMMENT-BLOCK-CM-SEC4 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The CI table covers the major categories and their naming conventions (SUP.8 BP2). Missing categories to consider: gateway Python code (`gateway/**/*.py`), Docker/container files (`docker/`), hardware design files (`hardware/`), CI/CD configuration (`.github/workflows/`), and tool configuration files (`tools/misra/`). The AUTOSAR module naming convention for source code is a good practice. Document ID in YAML frontmatter is consistently used across safety and ASPICE docs.
<!-- HITL-LOCK END:COMMENT-BLOCK-CM-SEC4 -->

## Baseline Strategy

Baselines are Git tags on `main`:

| Baseline | Tag | Content |
|----------|-----|---------|
| BL-001 | v0.1.0 | Phase 0 complete — architecture docs |
| BL-002 | v0.2.0 | Phase 1-3 complete — safety + requirements |
| BL-003 | v0.5.0 | Phase 5 complete — BSW baseline |
| BL-004 | v1.0.0 | Phase 14 complete — release |

<!-- HITL-LOCK START:COMMENT-BLOCK-CM-SEC5 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The baseline strategy is clear — Git tags on `main` mapped to project phases (SUP.8 BP6). Four baselines cover the key milestones. There is a jump from BL-003 (v0.5.0, Phase 5) to BL-004 (v1.0.0, Phase 14) — phases 6-13 have no intermediate baseline. For a 14-phase project with ASIL D requirements, consider adding baselines at verification milestones (e.g., after integration testing, after HIL). The `scripts/baseline-tag.sh` script mentioned in the project layout should be referenced here.
<!-- HITL-LOCK END:COMMENT-BLOCK-CM-SEC5 -->

## Change Control

- All changes via pull request to `develop`
- Safety-relevant changes require independent review
- Baseline changes require formal change request (see change-requests/)

<!-- HITL-LOCK START:COMMENT-BLOCK-CM-SEC6 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Change control rules are concise and correct — PR-based workflow, independent review for safety changes, and formal CRs for baseline changes (SUP.8 BP4/BP5). The reference to `change-requests/` is good. Consider documenting: what constitutes a "safety-relevant change" (any change to ASIL-rated code, safety docs, or safety analysis?), how independence is defined for reviews (same person rule, team-level, org-level), and the change request approval workflow.
<!-- HITL-LOCK END:COMMENT-BLOCK-CM-SEC6 -->

## Naming Conventions

| Item | Convention | Example |
|------|-----------|---------|
| Requirements | {PREFIX}-{ECU}-{NNN} | SSR-CVC-001 |
| Test cases | TC-{ECU}-{MOD}-{NNN} | TC-CVC-PEDAL-001 |
| Change requests | CR-{NNN} | CR-001 |
| Baselines | BL-{NNN} | BL-001 |
| Documents | Document ID in YAML | HARA, FSC, SYSARCH |

<!-- HITL-LOCK START:COMMENT-BLOCK-CM-SEC7 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Naming conventions are consistent and well-defined. The `{PREFIX}-{ECU}-{NNN}` pattern for requirements supports per-ECU traceability across the 7-ECU platform. This aligns with the traceability rules in `traceability.md`. The document ID convention (YAML frontmatter) is already in use across safety and ASPICE docs. No issues with this section.
<!-- HITL-LOCK END:COMMENT-BLOCK-CM-SEC7 -->

## Status Accounting

Document status lifecycle: `planned` → `draft` → `review` → `approved` → `baselined`

<!-- HITL-LOCK START:COMMENT-BLOCK-CM-SEC8 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The status lifecycle is clear and covers the full document maturity progression (SUP.8 BP7). The five states are appropriate. Consider adding: how status is tracked (YAML frontmatter `status` field, which is already used in existing docs), who can transition between states (e.g., only FSE can move to "approved"), and whether a status report is generated periodically for project management visibility.
<!-- HITL-LOCK END:COMMENT-BLOCK-CM-SEC8 -->

## CM Audits

| Audit | Trigger | Scope |
|-------|---------|-------|
| Functional CM audit | Before baseline | All CIs match approved versions |
| Physical CM audit | Before release | Build reproduces from tagged source |

<!-- HITL-LOCK START:COMMENT-BLOCK-CM-SEC9 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The CM audit section covers both functional and physical audits (SUP.8 BP8), which are the two standard CM audit types. The triggers and scopes are correctly defined. The physical CM audit ("build reproduces from tagged source") directly supports the reproducible build requirement from `build-and-ci.md`. Consider adding: audit records/evidence format, who performs the audit (independence requirement?), and where audit results are stored. Also consider adding a periodic CM status audit (e.g., quarterly) beyond just the baseline/release triggers.
<!-- HITL-LOCK END:COMMENT-BLOCK-CM-SEC9 -->

