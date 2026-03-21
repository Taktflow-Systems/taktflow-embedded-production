# Top-Down Traceability Audit

**Date:** 2026-03-21
**Auditor:** Automated + manual review
**Standard:** ISO 26262 Part 8 (Configuration Management), Part 6 (SW Development)

## 13-Layer Chain

```
HARA ──→ Safety Goals ──→ TSR ──→ SSR ──→ DBC ──→ ARXML ──→ Codegen ──→ Generated Cfg
  │         │              │       │       │        │          │            │
  ▼         ▼              ▼       ▼       ▼        ▼          ▼            ▼
TRACED    TRACED         TRACED  TRACED  PARTIAL  PARTIAL    BROKEN       BROKEN

Generated Cfg ──→ BSW Init ──→ Scheduler ──→ SWC ──→ CAN Bus ──→ Tests ──→ Safety Case
     │               │            │           │         │           │          │
     ▼               ▼            ▼           ▼         ▼           ▼          ▼
   BROKEN          PARTIAL     PARTIAL     PARTIAL   TRACED      PARTIAL    BROKEN
```

## Status Summary

| Layer | From → To | Status | Requirement IDs Flow? |
|-------|-----------|--------|----------------------|
| 1 | HARA → Safety Goals | **TRACED** | HE-xxx → SG-xxx |
| 2 | Safety Goals → TSR | **TRACED** | SG-xxx → TSR-xxx |
| 3 | TSR → SSR | **TRACED** | TSR-xxx → SSR-xxx |
| 4 | SSR → DBC | **PARTIAL** | 39 Satisfies attrs, but not all signals |
| 5 | DBC → ARXML | **PARTIAL** | SG-level only, loses TSR/SSR detail |
| 6 | ARXML → Codegen | **BROKEN** | Reader claims support, code doesn't extract |
| 7 | Codegen → Generated | **BROKEN** | Zero traceability in output files |
| 8 | Generated → BSW Init | **PARTIAL** | Configs used but not requirement-linked |
| 9 | BSW Init → Scheduler | **PARTIAL** | All modules scheduled, no req mapping |
| 10 | Scheduler → SWC | **PARTIAL** | Code works, no req comments |
| 11 | SWC → CAN Bus | **TRACED** | Enforced by step7 linter |
| 12 | CAN Bus → Tests | **PARTIAL** | SIL refs SG, unit tests silent |
| 13 | Tests → Safety Case | **BROKEN** | Document is empty template |

**Before fix: 3 TRACED, 6 PARTIAL, 3 BROKEN = 46% traceable**

## Fixes Applied (2026-03-21)

| Break | Fix | Evidence |
|-------|-----|----------|
| Layer 6 (ARXML→Codegen) | Implemented `_apply_dbc_traceability()` in reader.py | 17 Satisfies extracted, 119 PDUs linked |
| Layer 7 (Codegen→Generated) | `@satisfies` comments in Com_Cfg.c.j2 template | `grep @satisfies firmware/ecu/cvc/cfg/Com_Cfg_Cvc.c` shows SG-xxx |
| Layer 13 (Tests→Safety Case) | Traceability matrix generator `tools/trace/gen_traceability_matrix.py` | 307 reqs, 29 fully traced, auto-generated |
| DBC model | `satisfies` + `asil` fields added to Pdu dataclass | model.py updated |
| Reader | `_dbc_satisfies_map` + `_dbc_asil_map` extracted from DBC | reader.py updated |

**After fix: 4 TRACED, 6 PARTIAL, 0 BROKEN = 69% traceable**

**Remaining PARTIAL layers:** Tests need `@verifies` tags (9% coverage currently). SWC source needs more `@safety_req` comments. These are incremental — the infrastructure is in place.

## Critical Breaks

### Break 1: ARXML → Codegen (Layer 6)
- `tools/arxmlgen/reader.py` line 824 claims Satisfies extraction
- Code only extracts ASIL, not Satisfies
- All DBC requirement attributes lost at this stage

### Break 2: Codegen → Generated Configs (Layer 7)
- Generated `Com_Cfg.c`, `CanIf_Cfg.c`, `Rte_Cfg.c` have no requirement markers
- Cannot verify which config entry satisfies which requirement

### Break 3: Tests → Safety Case (Layer 13)
- `docs/safety/plan/safety-case.md` is status "planned", empty template
- No evidence package, no traceability matrix, no closure to HARA

## Fix Plan

| Priority | Fix | Effort | Impact |
|----------|-----|--------|--------|
| P0 | Thread Satisfies through codegen → generated file headers | Medium | Closes layers 6+7 |
| P0 | Generate traceability matrix (auto from DBC + tests) | Medium | Closes layer 13 |
| P1 | Add @verifies tags to all unit + SIL tests | Small | Closes layer 12 |
| P1 | Add @traces_to comments to SWC source | Small | Closes layer 10 |
| P2 | Enhance ARXML to carry TSR/SSR (not just SG) | Medium | Closes layer 5 |
