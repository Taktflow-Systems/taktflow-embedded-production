# Plan: Deduplicate Hardware Bringup Plan + Workbook

> **Status**: DONE
> **Created**: 2026-03-02
> **Completed**: 2026-03-03

## Goal

Remove duplicated content between `plan-hardware-bringup.md` (the plan) and `plan-hardware-bringup-workbook.md` (the workbook). Each file gets a clear role with no overlap.

## Role Assignment

| File | Role | Contains |
|------|------|----------|
| **plan-hardware-bringup.md** | Strategy & overview | Context, inventory, phase readiness, dependency diagram, time estimates, risk assessment, tips, safety rules. Phase descriptions are **brief summaries only** (goal + key decisions + pass criteria). |
| **plan-hardware-bringup-workbook.md** | Execution reference | All step-by-step assembly instructions (GATHER/TOOLS/DO/MEASURE/WRONG/FIX), verification checklists, weekend checkpoints, appendices (pin refs, wire colors, troubleshooting). |

## What Changed

### In the plan (`plan-hardware-bringup.md`)

1. **Kept as-is**: Context, Inventory, Phase Readiness, Reference Files, Phase Dependencies, Estimated Time, Weekend Schedule, 3 Hardest Parts, Risk Assessment, Essential Tips, Safety Rules
2. **Slimmed down**: Phases 0-7 replaced with brief summaries (~5-8 bullet points each) with key decisions and pass criteria. All detailed wiring, solder, and measurement steps removed (live in workbook only).
3. **Phase 4.6**: Kept as brief summary in plan. Full step-by-step moved to workbook.
4. **Result**: Plan went from ~703 lines to ~294 lines (58% reduction). No content lost — all detail moved to workbook.

### In the workbook (`plan-hardware-bringup-workbook.md`)

1. **Added**: Step 4.5 (Preemptive ADC/GPIO Overvoltage Protection) with full GATHER/TOOLS/DO/MEASURE/WRONG format, including verification table for each channel
2. **Added**: Cross-reference header pointing to plan for overview content (estimates, risk, safety rules)
3. **Added**: Zener clamp checkpoint to Weekend 3 checklist
4. **Updated**: Header metadata (removed "Companion to" line, added "See also" reference)

## Execution Steps

- [x] Step 1: Add Phase 4.6 content to workbook (new Step 4.5)
- [x] Step 2: Add cross-reference header to workbook
- [x] Step 3: Slim down plan Phases 0-7 to brief summaries
- [x] Step 4: Update Reference Files section in plan
- [x] Step 5: Verify no orphaned content
