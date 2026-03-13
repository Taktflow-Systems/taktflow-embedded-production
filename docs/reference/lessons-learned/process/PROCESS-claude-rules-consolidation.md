# Lessons Learned — Claude Rules Consolidation

**Date**: 2026-03-01
**Scope**: Context window optimization by consolidating `.claude/rules/` from 30 files / 3,374 lines to 26 files / 1,999 lines (41% reduction)
**Status**: Closed

---

## Problem

When editing firmware files, Claude loaded nearly all 30 rule files simultaneously — consuming ~30-40% of the context window before any conversation started. Users experienced context compaction after only ~5 minutes of work.

Three root causes:
1. **HITL boilerplate duplicated 30x** — 13-15 line header in every file (~400 wasted lines)
2. **5 ASIL-D reference files** (739 lines total) loaded on any firmware edit — mostly educational tables an AI already knows
3. **5 files used `**/*` scope** (always loaded) when they were only relevant to firmware/safety work

---

## Key Decisions

### 1. Deduplicate HITL into one file
- Created `hitl.md` with `paths: **/*` — single source of truth
- Removed the block from all 29 other files
- **Savings**: ~400 lines

### 2. Consolidate 5 ASIL-D files → 1 compact + 1 reference doc
- `asil-d.md` (135 lines): actionable do/don't constraints only
- `docs/reference/asil-d-reference.md`: full tables for human reference (not loaded into context)
- **Savings**: ~600 lines from context (739 → 135)

### 3. Narrow `**/*` scope to firmware/safety paths
- `aspice.md`, `iso-compliance.md`, `safety-lifecycle.md`, `traceability.md` changed from `**/*` to `firmware/**/*`, `hardware/**/*`, `docs/safety/**/*`, `docs/aspice/**/*`
- Only 3 files still use `**/*`: `hitl.md`, `workflow.md`, `documentation.md` (all warranted)

### 4. Trim educational content from 6 reference-heavy files
- Removed V-model explanations, capability level descriptions, full ASIL determination matrix, ISO 26262 12-part structure table, FSE certification table
- AI already knows these — keeping them wastes context on information it can recall from training

### 5. Merge tiny files
- `procurement.md` (17 lines) → merged into `workflow.md` under "Procurement & BOM" section

---

## Key Takeaways

1. **Rules should be actionable constraints, not educational reference.** The AI doesn't need to be taught what ISO 26262 Part 5 covers — it needs to know the project-specific rules it must follow. Move reference tables to `docs/reference/` where humans can read them without consuming context.

2. **Deduplicate aggressively with path-scoped global rules.** Any boilerplate that appears in every file should be a single file with `paths: **/*`. The 30x HITL duplication was the single largest waste.

3. **Scope rules to their actual audience.** ASPICE process areas are irrelevant when editing a Python gateway script or a markdown README. Using `**/*` when `firmware/**/*` suffices wastes context on every non-firmware edit.

4. **Consolidation beats fragmentation for related content.** Five ASIL-D files (software, hardware, architecture, verification, decomposition) that always load together should be one file. The split was logical but the cost (5 frontmatter blocks, 5 HITL blocks, 5 file loads) outweighed the organizational benefit.

5. **sed + shell loops are the right tool for bulk rule file edits.** Subagents with bash commands processed 24 files in seconds. Trying to edit each file individually through the Edit tool would have been 24 round-trips.

6. **Always update cross-references after restructuring.** `iso-compliance.md` and `CLAUDE.md` both listed the old 5 ASIL-D files — these had to be updated to point to `asil-d.md`. Grep for old filenames after any rename/delete.

---

## Metrics

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Rule files | 30 | 26 | -4 |
| Total lines | 3,374 | 1,999 | -1,375 (41%) |
| `**/*` scope files | 5 | 3 | -2 |
| HITL copies | 30 | 1 | -29 |
| ASIL-D files | 5 (739 lines) | 1 (135 lines) | -604 lines |
