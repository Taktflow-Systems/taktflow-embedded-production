# Plan: Claude Rules Consolidation — Context Window Optimization

## Context

The `taktflow-embedded/.claude/rules/` directory has **30 files, 3,374 lines**. When editing firmware files, nearly all load simultaneously — consuming ~30-40% of context window before the conversation starts. Users see context compaction after only ~5 minutes in other sessions.

Three main problems:
1. **HITL boilerplate duplicated 30×** — 18-line header in every file = ~540 wasted lines
2. **5 ASIL-D reference files** (739 lines) load on any firmware edit — mostly educational tables, not actionable constraints
3. **5 files use `paths: **/*`** (always loaded) — some don't need global scope

## Status

| Phase | Name | Status |
|-------|------|--------|
| 1 | Deduplicate HITL boilerplate | PENDING |
| 2 | Consolidate 5 ASIL-D files → 1 | PENDING |
| 3 | Narrow `**/*` scope | PENDING |
| 4 | Trim reference-heavy files | PENDING |
| 5 | Merge small/overlapping files | PENDING |

## Phase 1: Deduplicate HITL Boilerplate

**Current**: Every rule file starts with the same 18-line HITL block.
**Fix**: Create one `hitl.md` rule with `paths: **/*` containing the HITL standard. Remove it from all 29 other files.

**Savings**: ~522 lines (18 lines × 29 files)

## Phase 2: Consolidate 5 ASIL-D Files → 1 Compact File

**Current** (5 files, 739 lines):
- `asil-d-software.md` (178 lines) — defensive programming, MC/DC, prohibited patterns
- `asil-d-hardware.md` (148 lines) — SPFM/LFM/PMHF metrics, redundancy architectures
- `asil-d-architecture.md` (145 lines) — FFI, timing, E2E communication
- `asil-d-verification.md` (162 lines) — verification methods, fault injection, independence
- `asil-decomposition.md` (106 lines) — decomposition table, constraints, DFA

**Fix**: Merge into one `asil-d.md` (~250 lines) with:
- Actionable constraints only (do/don't rules)
- Remove educational explanations and large reference tables
- Move full reference tables to `docs/reference/asil-d-reference.md` (not a rule, not loaded into context)

**Savings**: ~490 lines from rules context

## Phase 3: Narrow `**/*` Scope on 3 Files

**Current**: These load on EVERY file edit:
- `aspice.md` (154 lines) — ASPICE process areas, V-model, work products
- `iso-compliance.md` (110 lines) — ISO 26262 overview, ASIL matrix
- `safety-lifecycle.md` (152 lines) — FSE role, HARA, safety plan, DFA

**Fix**: Change paths to `firmware/**/*`, `hardware/**/*`, `docs/safety/**/*`, `docs/aspice/**/*` — they're only relevant when working on firmware/safety, not when editing web app or scripts.

Keep `**/*` for:
- `workflow.md` — planning discipline applies everywhere (116 lines, worth it)
- `documentation.md` — doc standards apply everywhere (97 lines, worth it)

**Savings**: ~416 lines not loaded when editing non-firmware files

## Phase 4: Trim Reference-Heavy Files

These files are >50% educational reference tables that Claude already knows:

| File | Current | Action | Target |
|------|---------|--------|--------|
| `aspice.md` | 154 lines | Keep process area table, remove V-model, capability descriptions | ~60 lines |
| `iso-compliance.md` | 110 lines | Keep ASIL matrix + cross-references, remove Part descriptions | ~50 lines |
| `safety-lifecycle.md` | 152 lines | Keep requirement flow-down + safety case checklist, remove FSE cert table | ~70 lines |
| `traceability.md` | 146 lines | Keep traceability chain + code patterns, remove explanation paragraphs | ~60 lines |
| `misra-c.md` | 116 lines | Keep critical rules + deviation process, remove rule explanations | ~60 lines |
| `tool-qualification.md` | 115 lines | Keep TCL matrix + project rules, remove method descriptions | ~50 lines |

**Savings**: ~443 lines

## Phase 5: Merge Small/Overlapping Files

| Merge From | Into | Rationale |
|---|---|---|
| `procurement.md` (17 lines) | `workflow.md` | Too small for its own file, it's a workflow rule |

**Savings**: 1 fewer file, ~17 lines

## Impact Summary

| Action | Lines Saved | Files Reduced |
|--------|-------------|---------------|
| HITL deduplication | ~522 | 0 (moved, not deleted) |
| ASIL-D consolidation | ~490 | 5 → 1 (net -4) |
| Narrow `**/*` scope | ~416 (conditional) | 0 |
| Trim reference content | ~443 | 0 |
| Merge procurement | ~17 | -1 |
| **Total** | **~1,870 lines** | **30 → 25 files** |

**Before**: 30 files, 3,374 lines, firmware edits load ~2,800 lines
**After**: 25 files, ~1,500 lines, firmware edits load ~1,200 lines

**~57% reduction** in context consumed by rules.

## File Changes

| File | Action |
|------|--------|
| `.claude/rules/hitl.md` | NEW — single HITL boilerplate (paths: `**/*`) |
| `.claude/rules/asil-d.md` | NEW — consolidated ASIL-D constraints (~250 lines) |
| `docs/reference/asil-d-reference.md` | NEW — full ASIL-D reference tables (moved from rules) |
| `.claude/rules/asil-d-software.md` | DELETE |
| `.claude/rules/asil-d-hardware.md` | DELETE |
| `.claude/rules/asil-d-architecture.md` | DELETE |
| `.claude/rules/asil-d-verification.md` | DELETE |
| `.claude/rules/asil-decomposition.md` | DELETE |
| `.claude/rules/procurement.md` | DELETE (merged into workflow.md) |
| `.claude/rules/aspice.md` | TRIM + narrow paths |
| `.claude/rules/iso-compliance.md` | TRIM + narrow paths |
| `.claude/rules/safety-lifecycle.md` | TRIM + narrow paths |
| `.claude/rules/traceability.md` | TRIM |
| `.claude/rules/misra-c.md` | TRIM |
| `.claude/rules/tool-qualification.md` | TRIM |
| `.claude/rules/workflow.md` | ADD procurement rule |
| All 29 other rule files | REMOVE HITL boilerplate |

## Verification

- `make test` still passes (rules don't affect build)
- Editing a firmware `.c` file loads consolidated `asil-d.md`, not old 5 files
- Editing a markdown file does NOT load aspice/iso-compliance/safety-lifecycle
- Grep: no rule content accidentally deleted vs moved
- HITL boilerplate still appears for all files (via hitl.md)
