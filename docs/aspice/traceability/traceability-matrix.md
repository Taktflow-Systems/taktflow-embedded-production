---
document_id: TRACE-FULL
title: "Full V-Model Traceability Matrix"
aspice_processes: "SYS.1-5, SWE.1-6"
iso_26262_part: "3, 4, 5, 6"
generated: "2026-03-10 21:37 UTC"
---

# Full V-Model Traceability Matrix

> **Auto-generated** by `scripts/trace-gen.py`
> Do not edit manually — regenerate after requirement or test changes.

**Standard references:**
- ISO 26262:2018 Parts 3-6 — Functional safety lifecycle
- Automotive SPICE 4.0 SYS.1-5, SWE.1-6 — V-model traceability

## Coverage Summary

**Total requirements**: 482

| Level | Total | Traced Up | Traced Down | Tested | Coverage |
|-------|-------|-----------|-------------|--------|----------|
| STK | 32 | — | 25 | 23 | 78% |
| SYS | 56 | 56 | 55 | 52 | 98% |
| SG | 8 | — | 8 | 8 | 100% |
| FSR | 26 | 26 | 26 | 25 | 100% |
| TSR | 52 | 52 | 52 | 50 | 100% |
| SSR | 82 | 82 | 81 | 81 | 98% |
| HSR | 25 | 25 | 0 | — | 0% |
| SWR | 201 | 199 | 0 | 187 | 93% |

## Full Traceability Chains

| SG | FSR | TSR | SSR/HSR | SWR | Source | Test | SIL | Status |
|---|---|---|---|---|---|---|---|---|
| SG-001 | FSR-001 | TSR-001 | HSR-CVC-001 | SWR-BSW-006 | firmware/bsw/mcal/Spi/include/Spi.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-001 | TSR-001 | HSR-CVC-001 | SWR-BSW-014 | firmware/bsw/ecual/IoHwAb/include/IoH... | — | — | **PARTIAL** |
| SG-001 | FSR-001 | TSR-001 | HSR-CVC-001 | SWR-CVC-001 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | test/sil/scenarios/sil_002_... | COVERED |
| SG-001 | FSR-001 | TSR-001 | HSR-CVC-001 | SWR-CVC-002 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-001 | TSR-001 | SSR-CVC-001 | SWR-BSW-006 | firmware/bsw/mcal/Spi/include/Spi.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-001 | TSR-001 | SSR-CVC-001 | SWR-BSW-014 | firmware/bsw/ecual/IoHwAb/include/IoH... | — | — | **PARTIAL** |
| SG-001 | FSR-001 | TSR-001 | SSR-CVC-001 | SWR-CVC-001 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | test/sil/scenarios/sil_002_... | COVERED |
| SG-001 | FSR-001 | TSR-001 | SSR-CVC-001 | SWR-CVC-002 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-001 | TSR-001 | SSR-CVC-002 | SWR-BSW-006 | firmware/bsw/mcal/Spi/include/Spi.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-001 | TSR-001 | SSR-CVC-002 | SWR-BSW-014 | firmware/bsw/ecual/IoHwAb/include/IoH... | — | — | **PARTIAL** |
| SG-001 | FSR-001 | TSR-001 | SSR-CVC-002 | SWR-CVC-001 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | test/sil/scenarios/sil_002_... | COVERED |
| SG-001 | FSR-001 | TSR-001 | SSR-CVC-002 | SWR-CVC-002 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-001 | TSR-002 | HSR-CVC-001 | SWR-BSW-019 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-001 | TSR-002 | HSR-CVC-001 | SWR-CVC-003 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-001 | TSR-002 | HSR-CVC-001 | SWR-CVC-004 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-001 | TSR-002 | SSR-CVC-003 | SWR-BSW-019 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-001 | TSR-002 | SSR-CVC-003 | SWR-CVC-003 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-001 | TSR-002 | SSR-CVC-003 | SWR-CVC-004 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-001 | TSR-002 | SSR-CVC-004 | SWR-BSW-019 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-001 | TSR-002 | SSR-CVC-004 | SWR-CVC-003 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-001 | TSR-002 | SSR-CVC-004 | SWR-CVC-004 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-002 | TSR-002 | HSR-CVC-001 | SWR-BSW-019 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-002 | TSR-002 | HSR-CVC-001 | SWR-CVC-003 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-002 | TSR-002 | HSR-CVC-001 | SWR-CVC-004 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-002 | TSR-002 | SSR-CVC-003 | SWR-BSW-019 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-002 | TSR-002 | SSR-CVC-003 | SWR-CVC-003 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-002 | TSR-002 | SSR-CVC-003 | SWR-CVC-004 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-002 | TSR-002 | SSR-CVC-004 | SWR-BSW-019 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-002 | TSR-002 | SSR-CVC-004 | SWR-CVC-003 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | test/sil/scenarios/sil_003_... | COVERED |
| SG-001 | FSR-002 | TSR-002 | SSR-CVC-004 | SWR-CVC-004 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-002 | TSR-003 | SSR-CVC-005 | SWR-CVC-005 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-003 | TSR-004 | SSR-CVC-006 | SWR-BSW-018 | firmware/bsw/services/Dem/include/Dem... | test/framework/src/test_int_dem_to_dc... | — | COVERED |
| SG-001 | FSR-003 | TSR-004 | SSR-CVC-006 | SWR-CVC-006 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | test/sil/scenarios/sil_015_... | COVERED |
| SG-001 | FSR-003 | TSR-004 | SSR-CVC-006 | SWR-CVC-007 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-003 | TSR-004 | SSR-CVC-006 | SWR-CVC-008 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-003 | TSR-004 | SSR-CVC-007 | SWR-BSW-018 | firmware/bsw/services/Dem/include/Dem... | test/framework/src/test_int_dem_to_dc... | — | COVERED |
| SG-001 | FSR-003 | TSR-004 | SSR-CVC-007 | SWR-CVC-006 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | test/sil/scenarios/sil_015_... | COVERED |
| SG-001 | FSR-003 | TSR-004 | SSR-CVC-007 | SWR-CVC-007 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-003 | TSR-004 | SSR-CVC-007 | SWR-CVC-008 | — | firmware/ecu/cvc/test/test_Swc_Pedal_... | — | COVERED |
| SG-001 | FSR-003 | TSR-005 | HSR-RZC-004 | SWR-BSW-008 | firmware/bsw/mcal/Pwm/include/Pwm.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-003 | TSR-005 | HSR-RZC-004 | SWR-BSW-009 | firmware/bsw/mcal/Dio/include/Dio.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-003 | TSR-005 | HSR-RZC-004 | SWR-RZC-001 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | test/sil/scenarios/sil_002_... | COVERED |
| SG-001 | FSR-003 | TSR-005 | HSR-RZC-004 | SWR-RZC-002 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-003 | TSR-005 | SSR-RZC-001 | SWR-BSW-008 | firmware/bsw/mcal/Pwm/include/Pwm.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-003 | TSR-005 | SSR-RZC-001 | SWR-BSW-009 | firmware/bsw/mcal/Dio/include/Dio.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-003 | TSR-005 | SSR-RZC-001 | SWR-RZC-001 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | test/sil/scenarios/sil_002_... | COVERED |
| SG-001 | FSR-003 | TSR-005 | SSR-RZC-001 | SWR-RZC-002 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-003 | TSR-005 | SSR-RZC-002 | SWR-BSW-008 | firmware/bsw/mcal/Pwm/include/Pwm.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-003 | TSR-005 | SSR-RZC-002 | SWR-BSW-009 | firmware/bsw/mcal/Dio/include/Dio.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-003 | TSR-005 | SSR-RZC-002 | SWR-RZC-001 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | test/sil/scenarios/sil_002_... | COVERED |
| SG-001 | FSR-003 | TSR-005 | SSR-RZC-002 | SWR-RZC-002 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-CVC-014 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-FZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-RZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-001 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-001 | FSR-016 | TSR-028 | SSR-SC-005 | SWR-SC-006 | firmware/ecu/sc/include/sc_heartbeat.... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-001 | FSR-016 | TSR-029 | HSR-SC-001 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-001 | FSR-016 | TSR-029 | HSR-SC-001 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-001 | FSR-016 | TSR-029 | HSR-SC-006 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-001 | FSR-016 | TSR-029 | HSR-SC-006 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-001 | FSR-016 | TSR-029 | SSR-SC-006 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-001 | FSR-016 | TSR-029 | SSR-SC-006 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-001 | FSR-016 | TSR-030 | HSR-SC-006 | SWR-SC-011 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-001 | FSR-016 | TSR-030 | HSR-SC-006 | SWR-SC-014 | firmware/ecu/sc/include/sc_esm.h, fir... | firmware/ecu/sc/test/test_sc_esm_asilc.c | — | COVERED |
| SG-001 | FSR-016 | TSR-030 | SSR-SC-007 | SWR-SC-011 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-001 | FSR-016 | TSR-030 | SSR-SC-007 | SWR-SC-014 | firmware/ecu/sc/include/sc_esm.h, fir... | firmware/ecu/sc/test/test_sc_esm_asilc.c | — | COVERED |
| SG-001 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-016 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-001 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-017 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-001 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-018 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-001 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-019 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-001 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-016 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-001 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-017 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-001 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-018 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-001 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-019 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-001 | FSR-018 | TSR-033 | HSR-CVC-003 | SWR-BSW-009 | firmware/bsw/mcal/Dio/include/Dio.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-018 | TSR-033 | HSR-CVC-003 | SWR-CVC-018 | — | firmware/ecu/cvc/test/test_Swc_EStop_... | — | COVERED |
| SG-001 | FSR-018 | TSR-033 | SSR-CVC-014 | SWR-BSW-009 | firmware/bsw/mcal/Dio/include/Dio.h, ... | — | — | **PARTIAL** |
| SG-001 | FSR-018 | TSR-033 | SSR-CVC-014 | SWR-CVC-018 | — | firmware/ecu/cvc/test/test_Swc_EStop_... | — | COVERED |
| SG-001 | FSR-018 | TSR-034 | SSR-CVC-015 | SWR-CVC-019 | — | firmware/ecu/cvc/test/test_Swc_EStop_... | — | COVERED |
| SG-001 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-001 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-001 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-001 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-001 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-001 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-001 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-001 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-001 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-001 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-CVC-011 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-001 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-BSW-020 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_001_... | COVERED |
| SG-001 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-012 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-001 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-013 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-001 | FSR-021 | TSR-040 | HSR-RZC-004 | SWR-RZC-014 | — | firmware/ecu/rzc/test/test_Swc_Encode... | — | COVERED |
| SG-001 | FSR-021 | TSR-040 | HSR-RZC-004 | SWR-RZC-015 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | — | COVERED |
| SG-001 | FSR-021 | TSR-040 | HSR-RZC-006 | SWR-RZC-014 | — | firmware/ecu/rzc/test/test_Swc_Encode... | — | COVERED |
| SG-001 | FSR-021 | TSR-040 | HSR-RZC-006 | SWR-RZC-015 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | — | COVERED |
| SG-001 | FSR-021 | TSR-040 | SSR-RZC-015 | SWR-RZC-014 | — | firmware/ecu/rzc/test/test_Swc_Encode... | — | COVERED |
| SG-001 | FSR-021 | TSR-040 | SSR-RZC-015 | SWR-RZC-015 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | — | COVERED |
| SG-001 | FSR-021 | TSR-040 | SSR-RZC-016 | SWR-RZC-014 | — | firmware/ecu/rzc/test/test_Swc_Encode... | — | COVERED |
| SG-001 | FSR-021 | TSR-040 | SSR-RZC-016 | SWR-RZC-015 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | — | COVERED |
| SG-001 | FSR-022 | TSR-007 | SSR-RZC-005 | SWR-RZC-007 | — | firmware/ecu/rzc/test/test_Swc_Curren... | — | COVERED |
| SG-001 | FSR-022 | TSR-041 | SSR-SC-010 | SWR-SC-007 | firmware/ecu/sc/include/sc_plausibili... | firmware/ecu/sc/test/test_sc_plausibi... | — | COVERED |
| SG-001 | FSR-022 | TSR-041 | SSR-SC-010 | SWR-SC-008 | firmware/ecu/sc/include/sc_plausibili... | firmware/ecu/sc/test/test_sc_plausibi... | — | COVERED |
| SG-001 | FSR-022 | TSR-041 | SSR-SC-011 | SWR-SC-007 | firmware/ecu/sc/include/sc_plausibili... | firmware/ecu/sc/test/test_sc_plausibi... | — | COVERED |
| SG-001 | FSR-022 | TSR-041 | SSR-SC-011 | SWR-SC-008 | firmware/ecu/sc/include/sc_plausibili... | firmware/ecu/sc/test/test_sc_plausibi... | — | COVERED |
| SG-001 | FSR-022 | TSR-042 | SSR-SC-012 | SWR-SC-009 | firmware/ecu/sc/include/sc_plausibili... | firmware/ecu/sc/test/test_sc_plausibi... | — | COVERED |
| SG-001 | FSR-023 | TSR-043 | HSR-CVC-005 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-001 | FSR-023 | TSR-043 | SSR-CVC-022 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-001 | FSR-023 | TSR-044 | HSR-FZC-007 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-001 | FSR-023 | TSR-044 | SSR-FZC-022 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-001 | FSR-023 | TSR-045 | HSR-SC-003 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-001 | FSR-023 | TSR-045 | SSR-SC-013 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-SC-025 | firmware/ecu/rzc/src/main.c, firmware... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-CVC-032 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-FZC-029 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-RZC-028 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-001 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-001 | FSR-026 | TSR-052 | SSR-SC-018 | — | firmware/ecu/sc/src/sc_main.c, firmwa... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-CVC-014 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-FZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-RZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-002 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-002 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-002 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-002 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-002 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-002 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-002 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-002 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-002 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-002 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-002 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-CVC-011 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-002 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-BSW-020 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_001_... | COVERED |
| SG-002 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-012 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-002 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-013 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-002 | FSR-023 | TSR-043 | HSR-CVC-005 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-002 | FSR-023 | TSR-043 | SSR-CVC-022 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-002 | FSR-023 | TSR-044 | HSR-FZC-007 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-002 | FSR-023 | TSR-044 | SSR-FZC-022 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-002 | FSR-023 | TSR-045 | HSR-SC-003 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-002 | FSR-023 | TSR-045 | SSR-SC-013 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-SC-025 | firmware/ecu/rzc/src/main.c, firmware... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-CVC-032 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-FZC-029 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-RZC-028 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-002 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-003 | FSR-006 | TSR-010 | HSR-FZC-001 | SWR-BSW-006 | firmware/bsw/mcal/Spi/include/Spi.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-006 | TSR-010 | HSR-FZC-001 | SWR-BSW-014 | firmware/bsw/ecual/IoHwAb/include/IoH... | — | — | **PARTIAL** |
| SG-003 | FSR-006 | TSR-010 | HSR-FZC-001 | SWR-FZC-001 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | — | COVERED |
| SG-003 | FSR-006 | TSR-010 | SSR-FZC-001 | SWR-BSW-006 | firmware/bsw/mcal/Spi/include/Spi.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-006 | TSR-010 | SSR-FZC-001 | SWR-BSW-014 | firmware/bsw/ecual/IoHwAb/include/IoH... | — | — | **PARTIAL** |
| SG-003 | FSR-006 | TSR-010 | SSR-FZC-001 | SWR-FZC-001 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | — | COVERED |
| SG-003 | FSR-006 | TSR-011 | SSR-FZC-002 | SWR-BSW-018 | firmware/bsw/services/Dem/include/Dem... | test/framework/src/test_int_dem_to_dc... | — | COVERED |
| SG-003 | FSR-006 | TSR-011 | SSR-FZC-002 | SWR-FZC-002 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | test/sil/scenarios/sil_008_... | COVERED |
| SG-003 | FSR-006 | TSR-011 | SSR-FZC-002 | SWR-FZC-003 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | test/sil/scenarios/sil_011_... | COVERED |
| SG-003 | FSR-006 | TSR-011 | SSR-FZC-003 | SWR-BSW-018 | firmware/bsw/services/Dem/include/Dem... | test/framework/src/test_int_dem_to_dc... | — | COVERED |
| SG-003 | FSR-006 | TSR-011 | SSR-FZC-003 | SWR-FZC-002 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | test/sil/scenarios/sil_008_... | COVERED |
| SG-003 | FSR-006 | TSR-011 | SSR-FZC-003 | SWR-FZC-003 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | test/sil/scenarios/sil_011_... | COVERED |
| SG-003 | FSR-007 | TSR-012 | HSR-FZC-006 | SWR-BSW-008 | firmware/bsw/mcal/Pwm/include/Pwm.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-007 | TSR-012 | HSR-FZC-006 | SWR-FZC-004 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | — | COVERED |
| SG-003 | FSR-007 | TSR-012 | HSR-FZC-006 | SWR-FZC-005 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | test/sil/scenarios/sil_004_... | COVERED |
| SG-003 | FSR-007 | TSR-012 | SSR-FZC-004 | SWR-BSW-008 | firmware/bsw/mcal/Pwm/include/Pwm.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-007 | TSR-012 | SSR-FZC-004 | SWR-FZC-004 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | — | COVERED |
| SG-003 | FSR-007 | TSR-012 | SSR-FZC-004 | SWR-FZC-005 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | test/sil/scenarios/sil_004_... | COVERED |
| SG-003 | FSR-007 | TSR-012 | SSR-FZC-005 | SWR-BSW-008 | firmware/bsw/mcal/Pwm/include/Pwm.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-007 | TSR-012 | SSR-FZC-005 | SWR-FZC-004 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | — | COVERED |
| SG-003 | FSR-007 | TSR-012 | SSR-FZC-005 | SWR-FZC-005 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | test/sil/scenarios/sil_004_... | COVERED |
| SG-003 | FSR-007 | TSR-013 | HSR-FZC-006 | SWR-FZC-006 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | — | COVERED |
| SG-003 | FSR-007 | TSR-013 | SSR-FZC-006 | SWR-FZC-006 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | — | COVERED |
| SG-003 | FSR-008 | TSR-014 | SSR-FZC-007 | SWR-FZC-007 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-CVC-014 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-FZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-RZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-003 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-003 | FSR-016 | TSR-028 | SSR-SC-005 | SWR-SC-006 | firmware/ecu/sc/include/sc_heartbeat.... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-003 | FSR-016 | TSR-029 | HSR-SC-001 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-003 | FSR-016 | TSR-029 | HSR-SC-001 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-003 | FSR-016 | TSR-029 | HSR-SC-006 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-003 | FSR-016 | TSR-029 | HSR-SC-006 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-003 | FSR-016 | TSR-029 | SSR-SC-006 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-003 | FSR-016 | TSR-029 | SSR-SC-006 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-003 | FSR-016 | TSR-030 | HSR-SC-006 | SWR-SC-011 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-003 | FSR-016 | TSR-030 | HSR-SC-006 | SWR-SC-014 | firmware/ecu/sc/include/sc_esm.h, fir... | firmware/ecu/sc/test/test_sc_esm_asilc.c | — | COVERED |
| SG-003 | FSR-016 | TSR-030 | SSR-SC-007 | SWR-SC-011 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-003 | FSR-016 | TSR-030 | SSR-SC-007 | SWR-SC-014 | firmware/ecu/sc/include/sc_esm.h, fir... | firmware/ecu/sc/test/test_sc_esm_asilc.c | — | COVERED |
| SG-003 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-016 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-003 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-017 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-003 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-018 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-003 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-019 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-003 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-016 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-003 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-017 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-003 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-018 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-003 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-019 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-003 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-003 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-003 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-003 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-003 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-003 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-003 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-003 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-003 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-003 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-CVC-011 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-003 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-BSW-020 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_001_... | COVERED |
| SG-003 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-012 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-003 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-013 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-003 | FSR-023 | TSR-043 | HSR-CVC-005 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-003 | FSR-023 | TSR-043 | SSR-CVC-022 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-003 | FSR-023 | TSR-044 | HSR-FZC-007 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-003 | FSR-023 | TSR-044 | SSR-FZC-022 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-003 | FSR-023 | TSR-045 | HSR-SC-003 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-003 | FSR-023 | TSR-045 | SSR-SC-013 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-SC-025 | firmware/ecu/rzc/src/main.c, firmware... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-CVC-032 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-FZC-029 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-RZC-028 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-003 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-004 | FSR-009 | TSR-015 | HSR-FZC-002 | SWR-FZC-009 | — | firmware/ecu/fzc/test/test_Swc_Brake_... | — | COVERED |
| SG-004 | FSR-009 | TSR-015 | SSR-FZC-008 | SWR-FZC-009 | — | firmware/ecu/fzc/test/test_Swc_Brake_... | — | COVERED |
| SG-004 | FSR-009 | TSR-016 | SSR-FZC-009 | SWR-FZC-010 | — | firmware/ecu/fzc/test/test_Swc_Brake_... | — | COVERED |
| SG-004 | FSR-010 | TSR-017 | SSR-FZC-010 | SWR-FZC-011 | — | firmware/ecu/fzc/test/test_Swc_Brake_... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-CVC-014 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-FZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-RZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-004 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-004 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-004 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-004 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-004 | FSR-016 | TSR-028 | SSR-SC-005 | SWR-SC-006 | firmware/ecu/sc/include/sc_heartbeat.... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-004 | FSR-016 | TSR-029 | HSR-SC-001 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-004 | FSR-016 | TSR-029 | HSR-SC-001 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-004 | FSR-016 | TSR-029 | HSR-SC-006 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-004 | FSR-016 | TSR-029 | HSR-SC-006 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-004 | FSR-016 | TSR-029 | SSR-SC-006 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-004 | FSR-016 | TSR-029 | SSR-SC-006 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-004 | FSR-016 | TSR-030 | HSR-SC-006 | SWR-SC-011 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-004 | FSR-016 | TSR-030 | HSR-SC-006 | SWR-SC-014 | firmware/ecu/sc/include/sc_esm.h, fir... | firmware/ecu/sc/test/test_sc_esm_asilc.c | — | COVERED |
| SG-004 | FSR-016 | TSR-030 | SSR-SC-007 | SWR-SC-011 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-004 | FSR-016 | TSR-030 | SSR-SC-007 | SWR-SC-014 | firmware/ecu/sc/include/sc_esm.h, fir... | firmware/ecu/sc/test/test_sc_esm_asilc.c | — | COVERED |
| SG-004 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-016 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-004 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-017 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-004 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-018 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-004 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-019 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-004 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-016 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-004 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-017 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-004 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-018 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-004 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-019 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-004 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-004 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-004 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-004 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-004 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-004 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-004 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-004 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-004 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-004 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-CVC-011 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-004 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-BSW-020 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_001_... | COVERED |
| SG-004 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-012 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-004 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-013 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-004 | FSR-023 | TSR-043 | HSR-CVC-005 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-004 | FSR-023 | TSR-043 | SSR-CVC-022 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-004 | FSR-023 | TSR-044 | HSR-FZC-007 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-004 | FSR-023 | TSR-044 | SSR-FZC-022 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-004 | FSR-023 | TSR-045 | HSR-SC-003 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-004 | FSR-023 | TSR-045 | SSR-SC-013 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-SC-025 | firmware/ecu/rzc/src/main.c, firmware... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-CVC-032 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-FZC-029 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-RZC-028 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-004 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-004 | FSR-025 | TSR-016 | SSR-FZC-009 | SWR-FZC-010 | — | firmware/ecu/fzc/test/test_Swc_Brake_... | — | COVERED |
| SG-004 | FSR-025 | TSR-048 | SSR-FZC-024 | SWR-FZC-012 | firmware/ecu/fzc/src/main.c | firmware/ecu/fzc/test/test_Swc_Brake_... | — | COVERED |
| SG-004 | FSR-025 | TSR-049 | SSR-SC-015 | SWR-SC-024 | firmware/ecu/sc/include/sc_plausibili... | firmware/ecu/sc/test/test_sc_plausibi... | — | COVERED |
| SG-005 | FSR-009 | TSR-015 | HSR-FZC-002 | SWR-FZC-009 | — | firmware/ecu/fzc/test/test_Swc_Brake_... | — | COVERED |
| SG-005 | FSR-009 | TSR-015 | SSR-FZC-008 | SWR-FZC-009 | — | firmware/ecu/fzc/test/test_Swc_Brake_... | — | COVERED |
| SG-005 | FSR-009 | TSR-016 | SSR-FZC-009 | SWR-FZC-010 | — | firmware/ecu/fzc/test/test_Swc_Brake_... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-CVC-014 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-FZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-RZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-005 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-005 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-005 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-005 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-005 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-005 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-005 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-005 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-005 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-005 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-005 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-005 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-005 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-005 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-CVC-011 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-005 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-BSW-020 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_001_... | COVERED |
| SG-005 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-012 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-005 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-013 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-005 | FSR-023 | TSR-043 | HSR-CVC-005 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-005 | FSR-023 | TSR-043 | SSR-CVC-022 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-005 | FSR-023 | TSR-044 | HSR-FZC-007 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-005 | FSR-023 | TSR-044 | SSR-FZC-022 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-005 | FSR-023 | TSR-045 | HSR-SC-003 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-005 | FSR-023 | TSR-045 | SSR-SC-013 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-SC-025 | firmware/ecu/rzc/src/main.c, firmware... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-CVC-032 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-FZC-029 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-RZC-028 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-005 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-006 | FSR-004 | TSR-006 | HSR-RZC-001 | SWR-BSW-007 | firmware/bsw/mcal/Adc/include/Adc.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-004 | TSR-006 | HSR-RZC-001 | SWR-BSW-014 | firmware/bsw/ecual/IoHwAb/include/IoH... | — | — | **PARTIAL** |
| SG-006 | FSR-004 | TSR-006 | HSR-RZC-001 | SWR-BSW-019 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_003_... | COVERED |
| SG-006 | FSR-004 | TSR-006 | HSR-RZC-001 | SWR-RZC-005 | — | firmware/ecu/rzc/test/test_Swc_Curren... | — | COVERED |
| SG-006 | FSR-004 | TSR-006 | HSR-RZC-001 | SWR-RZC-006 | — | firmware/ecu/rzc/test/test_Swc_Curren... | — | COVERED |
| SG-006 | FSR-004 | TSR-006 | SSR-RZC-003 | SWR-BSW-007 | firmware/bsw/mcal/Adc/include/Adc.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-004 | TSR-006 | SSR-RZC-003 | SWR-BSW-014 | firmware/bsw/ecual/IoHwAb/include/IoH... | — | — | **PARTIAL** |
| SG-006 | FSR-004 | TSR-006 | SSR-RZC-003 | SWR-BSW-019 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_003_... | COVERED |
| SG-006 | FSR-004 | TSR-006 | SSR-RZC-003 | SWR-RZC-005 | — | firmware/ecu/rzc/test/test_Swc_Curren... | — | COVERED |
| SG-006 | FSR-004 | TSR-006 | SSR-RZC-003 | SWR-RZC-006 | — | firmware/ecu/rzc/test/test_Swc_Curren... | — | COVERED |
| SG-006 | FSR-004 | TSR-006 | SSR-RZC-004 | SWR-BSW-007 | firmware/bsw/mcal/Adc/include/Adc.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-004 | TSR-006 | SSR-RZC-004 | SWR-BSW-014 | firmware/bsw/ecual/IoHwAb/include/IoH... | — | — | **PARTIAL** |
| SG-006 | FSR-004 | TSR-006 | SSR-RZC-004 | SWR-BSW-019 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_003_... | COVERED |
| SG-006 | FSR-004 | TSR-006 | SSR-RZC-004 | SWR-RZC-005 | — | firmware/ecu/rzc/test/test_Swc_Curren... | — | COVERED |
| SG-006 | FSR-004 | TSR-006 | SSR-RZC-004 | SWR-RZC-006 | — | firmware/ecu/rzc/test/test_Swc_Curren... | — | COVERED |
| SG-006 | FSR-004 | TSR-007 | SSR-RZC-005 | SWR-RZC-007 | — | firmware/ecu/rzc/test/test_Swc_Curren... | — | COVERED |
| SG-006 | FSR-005 | TSR-008 | HSR-RZC-002 | SWR-BSW-007 | firmware/bsw/mcal/Adc/include/Adc.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-005 | TSR-008 | HSR-RZC-002 | SWR-BSW-014 | firmware/bsw/ecual/IoHwAb/include/IoH... | — | — | **PARTIAL** |
| SG-006 | FSR-005 | TSR-008 | HSR-RZC-002 | SWR-RZC-009 | — | firmware/ecu/rzc/test/test_Swc_TempMo... | — | COVERED |
| SG-006 | FSR-005 | TSR-008 | SSR-RZC-006 | SWR-BSW-007 | firmware/bsw/mcal/Adc/include/Adc.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-005 | TSR-008 | SSR-RZC-006 | SWR-BSW-014 | firmware/bsw/ecual/IoHwAb/include/IoH... | — | — | **PARTIAL** |
| SG-006 | FSR-005 | TSR-008 | SSR-RZC-006 | SWR-RZC-009 | — | firmware/ecu/rzc/test/test_Swc_TempMo... | — | COVERED |
| SG-006 | FSR-005 | TSR-009 | HSR-RZC-007 | SWR-RZC-010 | — | firmware/ecu/rzc/test/test_Swc_TempMo... | — | COVERED |
| SG-006 | FSR-005 | TSR-009 | SSR-RZC-007 | SWR-RZC-010 | — | firmware/ecu/rzc/test/test_Swc_TempMo... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-CVC-014 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-FZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-RZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-006 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-006 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-006 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-006 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-006 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-006 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-006 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-006 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-006 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-006 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-006 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-CVC-011 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-006 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-BSW-020 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_001_... | COVERED |
| SG-006 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-012 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-006 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-013 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-006 | FSR-023 | TSR-043 | HSR-CVC-005 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-006 | FSR-023 | TSR-043 | SSR-CVC-022 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-006 | FSR-023 | TSR-044 | HSR-FZC-007 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-006 | FSR-023 | TSR-044 | SSR-FZC-022 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-006 | FSR-023 | TSR-045 | HSR-SC-003 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-006 | FSR-023 | TSR-045 | SSR-SC-013 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-SC-025 | firmware/ecu/rzc/src/main.c, firmware... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-CVC-032 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-FZC-029 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-RZC-028 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-006 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-007 | FSR-011 | TSR-018 | HSR-FZC-003 | SWR-FZC-013 | — | firmware/ecu/fzc/test/test_Swc_Lidar_... | — | COVERED |
| SG-007 | FSR-011 | TSR-018 | SSR-FZC-011 | SWR-FZC-013 | — | firmware/ecu/fzc/test/test_Swc_Lidar_... | — | COVERED |
| SG-007 | FSR-011 | TSR-019 | SSR-FZC-012 | SWR-FZC-014 | — | firmware/ecu/fzc/test/test_Swc_Lidar_... | — | COVERED |
| SG-007 | FSR-012 | TSR-018 | HSR-FZC-003 | SWR-FZC-013 | — | firmware/ecu/fzc/test/test_Swc_Lidar_... | — | COVERED |
| SG-007 | FSR-012 | TSR-018 | SSR-FZC-011 | SWR-FZC-013 | — | firmware/ecu/fzc/test/test_Swc_Lidar_... | — | COVERED |
| SG-007 | FSR-012 | TSR-020 | SSR-FZC-013 | SWR-BSW-018 | firmware/bsw/services/Dem/include/Dem... | test/framework/src/test_int_dem_to_dc... | — | COVERED |
| SG-007 | FSR-012 | TSR-020 | SSR-FZC-013 | SWR-FZC-015 | — | firmware/ecu/fzc/test/test_Swc_Lidar_... | — | COVERED |
| SG-007 | FSR-012 | TSR-021 | SSR-FZC-014 | SWR-FZC-016 | — | firmware/ecu/fzc/test/test_Swc_Lidar_... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-CVC-014 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-FZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-RZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-007 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-007 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-007 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-007 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-007 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-007 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-007 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-007 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-007 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-007 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-007 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-007 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-007 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-007 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-CVC-011 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-007 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-BSW-020 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_001_... | COVERED |
| SG-007 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-012 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-007 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-013 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-007 | FSR-023 | TSR-043 | HSR-CVC-005 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-007 | FSR-023 | TSR-043 | SSR-CVC-022 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-007 | FSR-023 | TSR-044 | HSR-FZC-007 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-007 | FSR-023 | TSR-044 | SSR-FZC-022 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-007 | FSR-023 | TSR-045 | HSR-SC-003 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-007 | FSR-023 | TSR-045 | SSR-SC-013 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-SC-025 | firmware/ecu/rzc/src/main.c, firmware... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-CVC-032 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-FZC-029 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-RZC-028 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-007 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-CVC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-FZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-RZC-005 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | HSR-SC-004 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-CVC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-FZC-015 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-RZC-008 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, ... | — | test/sil/scenarios/sil_001_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_can_matri... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-CVC-014 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-FZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-RZC-019 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-022 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-CVC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-015 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-FZC-016 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-008 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-CVC-014 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-FZC-019 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-023 | SSR-RZC-009 | SWR-RZC-019 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-CVC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-FZC-017 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-CVC-015 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-FZC-020 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-RZC-020 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-RZC-010 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-001 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanI... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-015 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_busof... | test/sil/scenarios/sil_005_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-016 | firmware/bsw/services/Com/include/Com... | test/framework/src/test_int_can_matri... | test/sil/scenarios/sil_007_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E... | test/framework/src/test_int_e2e_chain... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-CVC-015 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-FZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-RZC-020 | firmware/ecu/sc/src/sc_can.c | firmware/ecu/rzc/test/test_Swc_RzcCom... | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_004_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | test/sil/scenarios/sil_012_... | COVERED |
| SG-008 | FSR-013 | TSR-024 | SSR-SC-002 | SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, fir... | firmware/ecu/sc/test/test_sc_e2e_asil... | — | COVERED |
| SG-008 | FSR-014 | TSR-025 | SSR-CVC-011 | SWR-CVC-020 | — | firmware/ecu/cvc/test/test_Swc_EStop_... | — | COVERED |
| SG-008 | FSR-014 | TSR-025 | SSR-CVC-011 | SWR-FZC-021 | — | firmware/ecu/fzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-025 | SSR-CVC-011 | SWR-RZC-021 | — | firmware/ecu/rzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-025 | SSR-FZC-018 | SWR-CVC-020 | — | firmware/ecu/cvc/test/test_Swc_EStop_... | — | COVERED |
| SG-008 | FSR-014 | TSR-025 | SSR-FZC-018 | SWR-FZC-021 | — | firmware/ecu/fzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-025 | SSR-FZC-018 | SWR-RZC-021 | — | firmware/ecu/rzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-025 | SSR-RZC-011 | SWR-CVC-020 | — | firmware/ecu/cvc/test/test_Swc_EStop_... | — | COVERED |
| SG-008 | FSR-014 | TSR-025 | SSR-RZC-011 | SWR-FZC-021 | — | firmware/ecu/fzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-025 | SSR-RZC-011 | SWR-RZC-021 | — | firmware/ecu/rzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-026 | SSR-CVC-012 | SWR-CVC-021 | — | firmware/ecu/cvc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-026 | SSR-CVC-012 | SWR-FZC-022 | — | firmware/ecu/fzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-026 | SSR-CVC-012 | SWR-RZC-022 | — | firmware/ecu/rzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-026 | SSR-FZC-019 | SWR-CVC-021 | firmware/ecu/fzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-026 | SSR-FZC-019 | SWR-FZC-022 | firmware/ecu/fzc/src/main.c | firmware/ecu/fzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-026 | SSR-FZC-019 | SWR-RZC-022 | firmware/ecu/fzc/src/main.c | firmware/ecu/rzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-026 | SSR-RZC-012 | SWR-CVC-021 | — | firmware/ecu/cvc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-026 | SSR-RZC-012 | SWR-FZC-022 | — | firmware/ecu/fzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-014 | TSR-026 | SSR-RZC-012 | SWR-RZC-022 | — | firmware/ecu/rzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-015 | TSR-027 | SSR-SC-003 | SWR-SC-004 | firmware/ecu/sc/include/sc_heartbeat.... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-008 | FSR-015 | TSR-027 | SSR-SC-003 | SWR-SC-005 | firmware/ecu/sc/include/sc_heartbeat.... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-008 | FSR-015 | TSR-027 | SSR-SC-003 | SWR-SC-027 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-008 | FSR-015 | TSR-027 | SSR-SC-003 | SWR-SC-028 | firmware/ecu/sc/src/sc_e2e.c, firmwar... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-008 | FSR-015 | TSR-027 | SSR-SC-004 | SWR-SC-004 | firmware/ecu/sc/include/sc_heartbeat.... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-008 | FSR-015 | TSR-027 | SSR-SC-004 | SWR-SC-005 | firmware/ecu/sc/include/sc_heartbeat.... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-008 | FSR-015 | TSR-027 | SSR-SC-004 | SWR-SC-027 | firmware/ecu/sc/src/sc_can.c, firmwar... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-008 | FSR-015 | TSR-027 | SSR-SC-004 | SWR-SC-028 | firmware/ecu/sc/src/sc_heartbeat.c | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-008 | FSR-015 | TSR-028 | SSR-SC-005 | SWR-SC-006 | firmware/ecu/sc/include/sc_heartbeat.... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-008 | FSR-016 | TSR-028 | SSR-SC-005 | SWR-SC-006 | firmware/ecu/sc/include/sc_heartbeat.... | firmware/ecu/sc/test/test_sc_heartbea... | — | COVERED |
| SG-008 | FSR-016 | TSR-029 | HSR-SC-001 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-008 | FSR-016 | TSR-029 | HSR-SC-001 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-008 | FSR-016 | TSR-029 | HSR-SC-006 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-008 | FSR-016 | TSR-029 | HSR-SC-006 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-008 | FSR-016 | TSR-029 | SSR-SC-006 | SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-008 | FSR-016 | TSR-029 | SSR-SC-006 | SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-008 | FSR-016 | TSR-030 | HSR-SC-006 | SWR-SC-011 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-008 | FSR-016 | TSR-030 | HSR-SC-006 | SWR-SC-014 | firmware/ecu/sc/include/sc_esm.h, fir... | firmware/ecu/sc/test/test_sc_esm_asilc.c | — | COVERED |
| SG-008 | FSR-016 | TSR-030 | SSR-SC-007 | SWR-SC-011 | firmware/ecu/sc/include/sc_relay.h, f... | firmware/ecu/sc/test/test_sc_relay_as... | — | COVERED |
| SG-008 | FSR-016 | TSR-030 | SSR-SC-007 | SWR-SC-014 | firmware/ecu/sc/include/sc_esm.h, fir... | firmware/ecu/sc/test/test_sc_esm_asilc.c | — | COVERED |
| SG-008 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-016 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-017 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-018 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-016 | TSR-050 | HSR-SC-005 | SWR-SC-019 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-016 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-017 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-018 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-016 | TSR-050 | SSR-SC-016 | SWR-SC-019 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-026 | SSR-CVC-012 | SWR-CVC-021 | — | firmware/ecu/cvc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-017 | TSR-026 | SSR-CVC-012 | SWR-FZC-022 | — | firmware/ecu/fzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-017 | TSR-026 | SSR-CVC-012 | SWR-RZC-022 | — | firmware/ecu/rzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-017 | TSR-026 | SSR-FZC-019 | SWR-CVC-021 | firmware/ecu/fzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-017 | TSR-026 | SSR-FZC-019 | SWR-FZC-022 | firmware/ecu/fzc/src/main.c | firmware/ecu/fzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-017 | TSR-026 | SSR-FZC-019 | SWR-RZC-022 | firmware/ecu/fzc/src/main.c | firmware/ecu/rzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-017 | TSR-026 | SSR-RZC-012 | SWR-CVC-021 | — | firmware/ecu/cvc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-017 | TSR-026 | SSR-RZC-012 | SWR-FZC-022 | — | firmware/ecu/fzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-017 | TSR-026 | SSR-RZC-012 | SWR-RZC-022 | — | firmware/ecu/rzc/test/test_Swc_Heartb... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-CVC-013 | SWR-BSW-021 | — | — | — | **UNCOVERED** |
| SG-008 | FSR-017 | TSR-031 | SSR-CVC-013 | SWR-CVC-023 | — | firmware/ecu/cvc/test/test_Swc_Watchd... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-CVC-013 | SWR-FZC-023 | — | firmware/ecu/fzc/test/test_Swc_FzcSaf... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-CVC-013 | SWR-RZC-023 | — | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-CVC-013 | SWR-SC-021 | firmware/ecu/sc/src/sc_selftest.c | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-CVC-013 | SWR-SC-022 | firmware/ecu/sc/include/sc_watchdog.h... | firmware/ecu/sc/test/test_sc_watchdog... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-FZC-020 | SWR-BSW-021 | — | — | — | **UNCOVERED** |
| SG-008 | FSR-017 | TSR-031 | SSR-FZC-020 | SWR-CVC-023 | — | firmware/ecu/cvc/test/test_Swc_Watchd... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-FZC-020 | SWR-FZC-023 | — | firmware/ecu/fzc/test/test_Swc_FzcSaf... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-FZC-020 | SWR-RZC-023 | — | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-FZC-020 | SWR-SC-021 | firmware/ecu/sc/src/sc_selftest.c | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-FZC-020 | SWR-SC-022 | firmware/ecu/sc/include/sc_watchdog.h... | firmware/ecu/sc/test/test_sc_watchdog... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-RZC-013 | SWR-BSW-021 | firmware/ecu/rzc/src/main.c | — | — | **PARTIAL** |
| SG-008 | FSR-017 | TSR-031 | SSR-RZC-013 | SWR-CVC-023 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Watchd... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-RZC-013 | SWR-FZC-023 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSaf... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-RZC-013 | SWR-RZC-023 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-RZC-013 | SWR-SC-021 | firmware/ecu/rzc/src/main.c, firmware... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-RZC-013 | SWR-SC-022 | firmware/ecu/rzc/src/main.c, firmware... | firmware/ecu/sc/test/test_sc_watchdog... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-SC-008 | SWR-BSW-021 | firmware/ecu/sc/src/sc_plausibility.c | — | — | **PARTIAL** |
| SG-008 | FSR-017 | TSR-031 | SSR-SC-008 | SWR-CVC-023 | firmware/ecu/sc/src/sc_plausibility.c | firmware/ecu/cvc/test/test_Swc_Watchd... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-SC-008 | SWR-FZC-023 | firmware/ecu/sc/src/sc_plausibility.c | firmware/ecu/fzc/test/test_Swc_FzcSaf... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-SC-008 | SWR-RZC-023 | firmware/ecu/sc/src/sc_plausibility.c | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-SC-008 | SWR-SC-021 | firmware/ecu/sc/src/sc_plausibility.c... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-031 | SSR-SC-008 | SWR-SC-022 | firmware/ecu/sc/include/sc_watchdog.h... | firmware/ecu/sc/test/test_sc_watchdog... | — | COVERED |
| SG-008 | FSR-017 | TSR-032 | HSR-CVC-002 | SWR-BSW-021 | — | — | — | **UNCOVERED** |
| SG-008 | FSR-017 | TSR-032 | HSR-FZC-004 | SWR-BSW-021 | — | — | — | **UNCOVERED** |
| SG-008 | FSR-017 | TSR-032 | HSR-RZC-003 | SWR-BSW-021 | — | — | — | **UNCOVERED** |
| SG-008 | FSR-017 | TSR-032 | HSR-SC-002 | SWR-BSW-021 | — | — | — | **UNCOVERED** |
| SG-008 | FSR-017 | TSR-050 | HSR-SC-005 | SWR-SC-016 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-050 | HSR-SC-005 | SWR-SC-017 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-050 | HSR-SC-005 | SWR-SC-018 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-050 | HSR-SC-005 | SWR-SC-019 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-050 | SSR-SC-016 | SWR-SC-016 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-050 | SSR-SC-016 | SWR-SC-017 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-050 | SSR-SC-016 | SWR-SC-018 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-050 | SSR-SC-016 | SWR-SC-019 | firmware/ecu/sc/include/sc_selftest.h... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-051 | HSR-SC-005 | SWR-SC-020 | firmware/ecu/sc/src/sc_selftest.c | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-017 | TSR-051 | SSR-SC-017 | SWR-SC-020 | firmware/ecu/sc/src/sc_selftest.c, fi... | firmware/ecu/sc/test/test_sc_selftest... | — | COVERED |
| SG-008 | FSR-018 | TSR-033 | HSR-CVC-003 | SWR-BSW-009 | firmware/bsw/mcal/Dio/include/Dio.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-018 | TSR-033 | HSR-CVC-003 | SWR-CVC-018 | — | firmware/ecu/cvc/test/test_Swc_EStop_... | — | COVERED |
| SG-008 | FSR-018 | TSR-033 | SSR-CVC-014 | SWR-BSW-009 | firmware/bsw/mcal/Dio/include/Dio.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-018 | TSR-033 | SSR-CVC-014 | SWR-CVC-018 | — | firmware/ecu/cvc/test/test_Swc_EStop_... | — | COVERED |
| SG-008 | FSR-018 | TSR-034 | SSR-CVC-015 | SWR-CVC-019 | — | firmware/ecu/cvc/test/test_Swc_EStop_... | — | COVERED |
| SG-008 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-008 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-008 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-008 | FSR-019 | TSR-035 | SSR-CVC-016 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-008 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-008 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_heartbeat... | — | COVERED |
| SG-008 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-009 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-008 | FSR-019 | TSR-035 | SSR-CVC-017 | SWR-CVC-010 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-008 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-BSW-022 | firmware/bsw/services/BswM/include/Bs... | test/framework/src/test_int_bswm_mode... | — | COVERED |
| SG-008 | FSR-019 | TSR-036 | SSR-CVC-018 | SWR-CVC-011 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-008 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-BSW-020 | firmware/bsw/services/WdgM/include/Wd... | test/framework/src/test_int_heartbeat... | test/sil/scenarios/sil_001_... | COVERED |
| SG-008 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-012 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-008 | FSR-019 | TSR-037 | SSR-CVC-019 | SWR-CVC-013 | — | firmware/ecu/cvc/test/test_Swc_Vehicl... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-CVC-004 | SWR-BSW-004 | firmware/bsw/mcal/Can/include/Can.h, ... | test/framework/src/test_int_can_busof... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-CVC-004 | SWR-BSW-005 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | HSR-CVC-004 | SWR-BSW-012 | firmware/bsw/ecual/CanIf/include/CanI... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | HSR-CVC-004 | SWR-CVC-024 | — | firmware/ecu/cvc/test/test_Swc_CanMon... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-CVC-004 | SWR-FZC-024 | — | firmware/ecu/fzc/test/test_Swc_FzcCan... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-CVC-004 | SWR-RZC-024 | — | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-CVC-004 | SWR-SC-023 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-FZC-005 | SWR-BSW-004 | firmware/bsw/mcal/Can/include/Can.h, ... | test/framework/src/test_int_can_busof... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-FZC-005 | SWR-BSW-005 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | HSR-FZC-005 | SWR-BSW-012 | firmware/bsw/ecual/CanIf/include/CanI... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | HSR-FZC-005 | SWR-CVC-024 | — | firmware/ecu/cvc/test/test_Swc_CanMon... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-FZC-005 | SWR-FZC-024 | — | firmware/ecu/fzc/test/test_Swc_FzcCan... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-FZC-005 | SWR-RZC-024 | — | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-FZC-005 | SWR-SC-023 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-RZC-005 | SWR-BSW-004 | firmware/bsw/mcal/Can/include/Can.h, ... | test/framework/src/test_int_can_busof... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-RZC-005 | SWR-BSW-005 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | HSR-RZC-005 | SWR-BSW-012 | firmware/bsw/ecual/CanIf/include/CanI... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | HSR-RZC-005 | SWR-CVC-024 | — | firmware/ecu/cvc/test/test_Swc_CanMon... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-RZC-005 | SWR-FZC-024 | — | firmware/ecu/fzc/test/test_Swc_FzcCan... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-RZC-005 | SWR-RZC-024 | — | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-RZC-005 | SWR-SC-023 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-SC-004 | SWR-BSW-004 | firmware/bsw/mcal/Can/include/Can.h, ... | test/framework/src/test_int_can_busof... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-SC-004 | SWR-BSW-005 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | HSR-SC-004 | SWR-BSW-012 | firmware/bsw/ecual/CanIf/include/CanI... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | HSR-SC-004 | SWR-CVC-024 | — | firmware/ecu/cvc/test/test_Swc_CanMon... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-SC-004 | SWR-FZC-024 | — | firmware/ecu/fzc/test/test_Swc_FzcCan... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-SC-004 | SWR-RZC-024 | — | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | HSR-SC-004 | SWR-SC-023 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-CVC-020 | SWR-BSW-004 | firmware/bsw/mcal/Can/include/Can.h, ... | test/framework/src/test_int_can_busof... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-CVC-020 | SWR-BSW-005 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | SSR-CVC-020 | SWR-BSW-012 | firmware/bsw/ecual/CanIf/include/CanI... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | SSR-CVC-020 | SWR-CVC-024 | — | firmware/ecu/cvc/test/test_Swc_CanMon... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-CVC-020 | SWR-FZC-024 | — | firmware/ecu/fzc/test/test_Swc_FzcCan... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-CVC-020 | SWR-RZC-024 | — | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-CVC-020 | SWR-SC-023 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-FZC-021 | SWR-BSW-004 | firmware/bsw/mcal/Can/include/Can.h, ... | test/framework/src/test_int_can_busof... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-FZC-021 | SWR-BSW-005 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | SSR-FZC-021 | SWR-BSW-012 | firmware/bsw/ecual/CanIf/include/CanI... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | SSR-FZC-021 | SWR-CVC-024 | — | firmware/ecu/cvc/test/test_Swc_CanMon... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-FZC-021 | SWR-FZC-024 | — | firmware/ecu/fzc/test/test_Swc_FzcCan... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-FZC-021 | SWR-RZC-024 | — | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-FZC-021 | SWR-SC-023 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-RZC-014 | SWR-BSW-004 | firmware/bsw/mcal/Can/include/Can.h, ... | test/framework/src/test_int_can_busof... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-RZC-014 | SWR-BSW-005 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | SSR-RZC-014 | SWR-BSW-012 | firmware/bsw/ecual/CanIf/include/CanI... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | SSR-RZC-014 | SWR-CVC-024 | — | firmware/ecu/cvc/test/test_Swc_CanMon... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-RZC-014 | SWR-FZC-024 | — | firmware/ecu/fzc/test/test_Swc_FzcCan... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-RZC-014 | SWR-RZC-024 | — | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-RZC-014 | SWR-SC-023 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-SC-009 | SWR-BSW-004 | firmware/bsw/mcal/Can/include/Can.h, ... | test/framework/src/test_int_can_busof... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-SC-009 | SWR-BSW-005 | firmware/bsw/mcal/Can/include/Can.h, ... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | SSR-SC-009 | SWR-BSW-012 | firmware/bsw/ecual/CanIf/include/CanI... | — | — | **PARTIAL** |
| SG-008 | FSR-020 | TSR-038 | SSR-SC-009 | SWR-CVC-024 | firmware/ecu/sc/src/sc_plausibility.c | firmware/ecu/cvc/test/test_Swc_CanMon... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-SC-009 | SWR-FZC-024 | firmware/ecu/sc/src/sc_plausibility.c | firmware/ecu/fzc/test/test_Swc_FzcCan... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-SC-009 | SWR-RZC-024 | firmware/ecu/sc/src/sc_plausibility.c | firmware/ecu/rzc/test/test_Swc_RzcSaf... | — | COVERED |
| SG-008 | FSR-020 | TSR-038 | SSR-SC-009 | SWR-SC-023 | firmware/ecu/sc/include/sc_can.h, fir... | firmware/ecu/sc/test/test_sc_can_asild.c | — | COVERED |
| SG-008 | FSR-020 | TSR-039 | SSR-CVC-021 | SWR-BSW-004 | firmware/bsw/mcal/Can/include/Can.h, ... | test/framework/src/test_int_can_busof... | — | COVERED |
| SG-008 | FSR-020 | TSR-039 | SSR-CVC-021 | SWR-CVC-025 | — | firmware/ecu/cvc/test/test_Swc_CanMon... | — | COVERED |
| SG-008 | FSR-023 | TSR-043 | HSR-CVC-005 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-008 | FSR-023 | TSR-043 | SSR-CVC-022 | SWR-CVC-027 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| SG-008 | FSR-023 | TSR-044 | HSR-FZC-007 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-008 | FSR-023 | TSR-044 | SSR-FZC-022 | SWR-FZC-017 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| SG-008 | FSR-023 | TSR-045 | HSR-SC-003 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-008 | FSR-023 | TSR-045 | SSR-SC-013 | SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, fir... | firmware/ecu/sc/test/test_sc_led_qm.c | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-CVC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-FZC-023 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-RZC-017 | SWR-SC-025 | firmware/ecu/rzc/src/main.c, firmware... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-CVC-032 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-FZC-029 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-RZC-028 | firmware/ecu/sc/src/sc_esm.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-046 | SSR-SC-014 | SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firm... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-CVC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-CVC-032 | — | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-FZC-029 | — | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-FZC-023 | SWR-RZC-028 | — | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, ... | — | test/sil/scenarios/sil_009_... | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmw... | test/framework/src/test_int_overcurre... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-CVC-032 | firmware/ecu/rzc/src/main.c | firmware/ecu/cvc/test/test_Swc_Schedu... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-FZC-029 | firmware/ecu/rzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSch... | — | COVERED |
| SG-008 | FSR-024 | TSR-047 | SSR-RZC-017 | SWR-RZC-028 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSch... | — | COVERED |
| — | — | — | — | SWR-ALL-001 | — | — | — | **UNCOVERED** |
| — | — | — | — | SWR-ALL-002 | — | — | — | **UNCOVERED** |
| — | — | — | — | SWR-ALL-003 | — | — | — | **UNCOVERED** |
| — | — | — | — | SWR-ALL-004 | — | — | — | **UNCOVERED** |
| — | — | — | — | SWR-BCM-001 | firmware/ecu/bcm/src/bcm_main.c | firmware/ecu/bcm/test/test_Swc_BcmCan... | — | COVERED |
| — | — | — | — | SWR-BCM-002 | — | firmware/ecu/bcm/test/test_Swc_BcmCan... | — | COVERED |
| — | — | — | — | SWR-BCM-003 | — | firmware/ecu/bcm/test/test_Swc_Lights... | — | COVERED |
| — | — | — | — | SWR-BCM-004 | — | firmware/ecu/bcm/test/test_Swc_Lights... | — | COVERED |
| — | — | — | — | SWR-BCM-005 | — | firmware/ecu/bcm/test/test_Swc_Lights... | — | COVERED |
| — | — | — | — | SWR-BCM-006 | — | firmware/ecu/bcm/test/test_Swc_Indica... | — | COVERED |
| — | — | — | — | SWR-BCM-007 | — | firmware/ecu/bcm/test/test_Swc_Indica... | — | COVERED |
| — | — | — | — | SWR-BCM-008 | — | firmware/ecu/bcm/test/test_Swc_Indica... | — | COVERED |
| — | — | — | — | SWR-BCM-009 | — | firmware/ecu/bcm/test/test_Swc_DoorLo... | — | COVERED |
| — | — | — | — | SWR-BCM-010 | — | firmware/ecu/bcm/test/test_Swc_BcmCan... | — | COVERED |
| — | — | — | — | SWR-BCM-011 | — | firmware/ecu/bcm/test/test_Swc_BcmCan... | — | COVERED |
| — | — | — | — | SWR-BCM-012 | firmware/ecu/bcm/src/bcm_main.c | firmware/ecu/bcm/test/test_Swc_BcmMai... | — | COVERED |
| — | — | — | — | SWR-BSW-013 | firmware/bsw/ecual/PduR/include/PduR.... | test/framework/src/test_int_can_busof... | — | COVERED |
| — | — | — | — | SWR-BSW-017 | firmware/bsw/services/Dcm/include/Dcm... | test/framework/src/test_int_dem_to_dc... | test/sil/scenarios/sil_006_... | COVERED |
| — | — | — | — | SWR-CVC-016 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| — | — | — | — | SWR-CVC-017 | — | firmware/ecu/cvc/test/test_Swc_CvcCom... | — | COVERED |
| — | — | — | — | SWR-CVC-022 | — | firmware/ecu/cvc/test/test_Swc_Heartb... | — | COVERED |
| — | — | — | — | SWR-CVC-026 | firmware/ecu/cvc/include/Ssd1306.h, f... | firmware/ecu/cvc/test/test_Ssd1306_qm.c | — | COVERED |
| — | — | — | — | SWR-CVC-028 | — | firmware/ecu/cvc/test/test_Swc_Dashbo... | — | COVERED |
| — | — | — | — | SWR-CVC-029 | firmware/ecu/cvc/src/main.c | firmware/ecu/cvc/test/test_Swc_SelfTe... | — | COVERED |
| — | — | — | — | SWR-CVC-030 | — | firmware/ecu/cvc/test/test_Swc_Nvm_as... | — | COVERED |
| — | — | — | — | SWR-CVC-031 | — | firmware/ecu/cvc/test/test_Swc_Nvm_as... | — | COVERED |
| — | — | — | — | SWR-CVC-033 | — | firmware/ecu/cvc/test/test_Swc_CvcDcm... | — | COVERED |
| — | — | — | — | SWR-CVC-034 | — | firmware/ecu/cvc/test/test_Swc_CvcDcm... | — | COVERED |
| — | — | — | — | SWR-CVC-035 | firmware/ecu/cvc/src/main.c | firmware/ecu/cvc/test/test_Swc_CvcDcm... | — | COVERED |
| — | — | — | — | SWR-FZC-008 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | — | COVERED |
| — | — | — | — | SWR-FZC-018 | — | firmware/ecu/fzc/test/test_Swc_Buzzer... | — | COVERED |
| — | — | — | — | SWR-FZC-025 | firmware/ecu/fzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcSaf... | — | COVERED |
| — | — | — | — | SWR-FZC-026 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| — | — | — | — | SWR-FZC-027 | — | firmware/ecu/fzc/test/test_Swc_FzcCom... | — | COVERED |
| — | — | — | — | SWR-FZC-028 | — | firmware/ecu/fzc/test/test_Swc_Steeri... | — | COVERED |
| — | — | — | — | SWR-FZC-030 | — | firmware/ecu/fzc/test/test_Swc_FzcDcm... | — | COVERED |
| — | — | — | — | SWR-FZC-031 | — | firmware/ecu/fzc/test/test_Swc_FzcNvm... | — | COVERED |
| — | — | — | — | SWR-FZC-032 | firmware/ecu/fzc/src/main.c | firmware/ecu/fzc/test/test_Swc_FzcNvm... | — | COVERED |
| — | — | — | — | SWR-GW-001 | — | — | test/sil/scenarios/sil_016_... | COVERED |
| — | — | — | — | SWR-GW-002 | — | — | test/sil/scenarios/sil_016_... | COVERED |
| — | — | — | — | SWR-GW-003 | — | — | test/sil/scenarios/sil_017_... | COVERED |
| — | — | — | — | SWR-GW-004 | — | — | test/sil/scenarios/sil_017_... | COVERED |
| — | — | — | — | SWR-GW-005 | — | — | test/sil/scenarios/sil_018_... | COVERED |
| — | — | — | — | SWR-GW-006 | — | — | test/sil/scenarios/sil_018_... | COVERED |
| — | — | — | — | SWR-ICU-001 | firmware/ecu/icu/src/icu_main.c | firmware/ecu/icu/test/test_Swc_IcuCan... | — | COVERED |
| — | — | — | — | SWR-ICU-002 | — | firmware/ecu/icu/test/test_Swc_Dashbo... | — | COVERED |
| — | — | — | — | SWR-ICU-003 | — | firmware/ecu/icu/test/test_Swc_Dashbo... | — | COVERED |
| — | — | — | — | SWR-ICU-004 | — | firmware/ecu/icu/test/test_Swc_Dashbo... | — | COVERED |
| — | — | — | — | SWR-ICU-005 | — | firmware/ecu/icu/test/test_Swc_Dashbo... | — | COVERED |
| — | — | — | — | SWR-ICU-006 | — | firmware/ecu/icu/test/test_Swc_Dashbo... | — | COVERED |
| — | — | — | — | SWR-ICU-007 | — | firmware/ecu/icu/test/test_Swc_Dashbo... | — | COVERED |
| — | — | — | — | SWR-ICU-008 | — | firmware/ecu/icu/test/test_Swc_DtcDis... | — | COVERED |
| — | — | — | — | SWR-ICU-009 | — | firmware/ecu/icu/test/test_Swc_Dashbo... | — | COVERED |
| — | — | — | — | SWR-ICU-010 | firmware/ecu/icu/src/icu_main.c | firmware/ecu/icu/test/test_Swc_IcuMai... | — | COVERED |
| — | — | — | — | SWR-RZC-003 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | test/sil/scenarios/sil_010_... | COVERED |
| — | — | — | — | SWR-RZC-004 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | test/sil/scenarios/sil_006_... | COVERED |
| — | — | — | — | SWR-RZC-008 | — | firmware/ecu/rzc/test/test_Swc_Curren... | — | COVERED |
| — | — | — | — | SWR-RZC-011 | — | firmware/ecu/rzc/test/test_Swc_TempMo... | — | COVERED |
| — | — | — | — | SWR-RZC-012 | — | firmware/ecu/rzc/test/test_Swc_Encode... | — | COVERED |
| — | — | — | — | SWR-RZC-013 | — | firmware/ecu/rzc/test/test_Swc_Encode... | — | COVERED |
| — | — | — | — | SWR-RZC-016 | — | firmware/ecu/rzc/test/test_Swc_Motor_... | — | COVERED |
| — | — | — | — | SWR-RZC-017 | — | firmware/ecu/rzc/test/test_Swc_Batter... | — | COVERED |
| — | — | — | — | SWR-RZC-018 | — | firmware/ecu/rzc/test/test_Swc_Batter... | — | COVERED |
| — | — | — | — | SWR-RZC-025 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcSel... | — | COVERED |
| — | — | — | — | SWR-RZC-026 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| — | — | — | — | SWR-RZC-027 | — | firmware/ecu/rzc/test/test_Swc_RzcCom... | — | COVERED |
| — | — | — | — | SWR-RZC-029 | — | firmware/ecu/rzc/test/test_Swc_RzcDcm... | — | COVERED |
| — | — | — | — | SWR-RZC-030 | firmware/ecu/rzc/src/main.c | firmware/ecu/rzc/test/test_Swc_RzcNvm... | — | COVERED |
| — | — | — | — | SWR-SC-015 | firmware/ecu/sc/include/sc_esm.h, fir... | firmware/ecu/sc/test/test_sc_esm_asilc.c | — | COVERED |
| — | — | — | — | SWR-SC-026 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, ... | firmware/ecu/sc/test/test_sc_main_asi... | — | COVERED |
| — | — | — | — | SWR-SC-029 | firmware/ecu/sc/include/sc_monitoring... | firmware/ecu/sc/test/test_sc_can_asild.c | — | COVERED |
| — | — | — | — | SWR-SC-030 | firmware/ecu/sc/include/sc_monitoring... | firmware/ecu/sc/test/test_sc_can_asild.c | — | COVERED |
| — | — | — | — | SWR-TCU-001 | firmware/ecu/tcu/src/tcu_main.c | firmware/ecu/tcu/test/test_Swc_TcuCan... | — | COVERED |
| — | — | — | — | SWR-TCU-002 | — | firmware/ecu/tcu/test/test_Swc_UdsSer... | — | COVERED |
| — | — | — | — | SWR-TCU-003 | — | firmware/ecu/tcu/test/test_Swc_UdsSer... | — | COVERED |
| — | — | — | — | SWR-TCU-004 | — | firmware/ecu/tcu/test/test_Swc_UdsSer... | — | COVERED |
| — | — | — | — | SWR-TCU-005 | — | firmware/ecu/tcu/test/test_Swc_UdsSer... | — | COVERED |
| — | — | — | — | SWR-TCU-006 | — | firmware/ecu/tcu/test/test_Swc_UdsSer... | — | COVERED |
| — | — | — | — | SWR-TCU-007 | — | firmware/ecu/tcu/test/test_Swc_UdsSer... | — | COVERED |
| — | — | — | — | SWR-TCU-008 | — | firmware/ecu/tcu/test/test_Swc_DtcSto... | — | COVERED |
| — | — | — | — | SWR-TCU-009 | — | firmware/ecu/tcu/test/test_Swc_DtcSto... | — | COVERED |
| — | — | — | — | SWR-TCU-010 | — | firmware/ecu/tcu/test/test_Swc_Obd2Pi... | — | COVERED |
| — | — | — | — | SWR-TCU-011 | — | firmware/ecu/tcu/test/test_Swc_UdsSer... | — | COVERED |
| — | — | — | — | SWR-TCU-012 | — | firmware/ecu/tcu/test/test_Swc_UdsSer... | — | COVERED |
| — | — | — | — | SWR-TCU-013 | — | firmware/ecu/tcu/test/test_Swc_UdsSer... | — | COVERED |
| — | — | — | — | SWR-TCU-014 | — | firmware/ecu/tcu/test/test_Swc_DataAg... | — | COVERED |
| — | — | — | — | SWR-TCU-015 | firmware/ecu/tcu/src/tcu_main.c | firmware/ecu/tcu/test/test_Swc_TcuMai... | — | COVERED |

## Requirement Index by Level

### STK (32 requirements)

| ID | Title | ASIL | Parents | Children | Tested |
|---|---|---|---|---|---|
| STK-001 | ISO 26262 Full Safety Lifecycle Demonstration | — | — | — | — |
| STK-002 | ASPICE Level 2 Process Maturity Demonstration | — | — | — | — |
| STK-003 | AUTOSAR-like BSW Architecture Demonstration | — | — | SYS-030 | — |
| STK-004 | Diverse Redundancy and Multi-Vendor Safety Arch... | — | — | SYS-025, SYS-026 | — |
| STK-005 | Drive-by-Wire Function | — | — | SYS-001, SYS-002, SYS-003... | — |
| STK-006 | Steering Control Function | — | — | SYS-010, SYS-011, SYS-012... | — |
| STK-007 | Braking Control Function | — | — | SYS-014, SYS-015, SYS-016... | — |
| STK-008 | Forward Obstacle Detection Function | — | — | SYS-017, SYS-018, SYS-019... | — |
| STK-009 | Independent Safety Monitoring Function | — | — | SYS-021, SYS-022, SYS-023... | — |
| STK-010 | Emergency Stop Function | — | — | SYS-028 | — |
| STK-011 | Body Control Function | — | — | SYS-035, SYS-036 | — |
| STK-012 | UDS Diagnostic Services | — | — | SYS-037, SYS-038, SYS-039... | — |
| STK-013 | Diagnostic Trouble Code Management | — | — | SYS-038, SYS-041 | — |
| STK-014 | Cloud Telemetry and Dashboarding | — | — | SYS-009, SYS-042 | — |
| STK-015 | Edge ML Anomaly Detection | — | — | SYS-043 | — |
| STK-016 | ASIL D Compliance for Drive-by-Wire, Steering, ... | — | — | SYS-001, SYS-002, SYS-005... | — |
| STK-017 | Defined Safe States for All Safety Goals | — | — | SYS-012, SYS-016, SYS-029... | — |
| STK-018 | Fault Tolerant Time Interval Compliance | — | — | SYS-002, SYS-011, SYS-015... | — |
| STK-019 | Independent Safety Monitoring with Kill Relay | — | — | SYS-021, SYS-022, SYS-024... | — |
| STK-020 | E2E Protection on Safety-Critical CAN Messages | — | — | SYS-032 | — |
| STK-021 | Control Loop Cycle Time | — | — | SYS-053, SYS-054 | — |
| STK-022 | CAN Bus Timing and Throughput | — | — | SYS-004, SYS-031, SYS-033 | — |
| STK-023 | Sensor Update Rates | — | — | SYS-047, SYS-048, SYS-049 | — |
| STK-024 | Safe State Transition Times | — | — | — | — |
| STK-025 | Demonstration Scenario Coverage | — | — | — | — |
| STK-026 | Visual and Audible Operator Feedback | — | — | SYS-036, SYS-044, SYS-045... | — |
| STK-027 | Reproducible Build and Demo Process | — | — | — | — |
| STK-028 | ISO 26262:2018 Compliance | — | — | — | — |
| STK-029 | ASPICE Level 2 Minimum Process Maturity | — | — | — | — |
| STK-030 | MISRA C:2012/2023 Coding Compliance | — | — | SYS-051, SYS-052 | — |
| STK-031 | Bidirectional Traceability | — | — | SYS-055 | — |
| STK-032 | SAP QM Integration Demonstration | — | — | SYS-056 | — |

### SYS (56 requirements)

| ID | Title | ASIL | Parents | Children | Tested |
|---|---|---|---|---|---|
| SYS-001 | Dual Pedal Position Sensing | D | STK-005, STK-016 | TSR-001, TSR-002, SWR-CVC-001... | — |
| SYS-002 | Pedal Sensor Plausibility Monitoring | D | STK-005, STK-016, STK-018 | TSR-003, TSR-004, SWR-CVC-003... | — |
| SYS-003 | Pedal-to-Torque Mapping | D | STK-005 | SWR-CVC-001, SWR-CVC-007 | — |
| SYS-004 | Motor Torque Control via CAN | D | STK-005, STK-022 | TSR-005, SWR-CVC-008, SWR-CVC-016... | — |
| SYS-005 | Motor Current Monitoring | A | STK-005, STK-016 | TSR-006, TSR-007, SWR-RZC-005... | — |
| SYS-006 | Motor Temperature Monitoring and Derating | A | STK-005, STK-016 | TSR-008, TSR-009, SWR-ICU-004... | — |
| SYS-007 | Motor Direction Control and Plausibility | C | STK-005, STK-016 | TSR-040, SWR-RZC-002, SWR-RZC-012... | — |
| SYS-008 | Battery Voltage Monitoring | QM | STK-005 | SWR-ICU-005, SWR-RZC-003, SWR-RZC-017... | — |
| SYS-009 | Encoder Feedback for Speed Measurement | QM | STK-005, STK-014 | SWR-ICU-002, SWR-RZC-004, SWR-RZC-012 | — |
| SYS-010 | Steering Command Reception and Servo Control | D | STK-006 | TSR-012, SWR-FZC-001, SWR-FZC-008... | — |
| SYS-011 | Steering Angle Feedback Monitoring | D | STK-006, STK-016, STK-018 | TSR-010, TSR-011, SWR-FZC-001... | — |
| SYS-012 | Steering Return-to-Center on Fault | D | STK-006, STK-017 | TSR-012, TSR-013, SWR-FZC-004... | — |
| SYS-013 | Steering Rate and Angle Limiting | C | STK-006, STK-016 | TSR-014, SWR-FZC-007 | — |
| SYS-014 | Brake Command Reception and Servo Control | D | STK-007 | TSR-015, SWR-FZC-002, SWR-FZC-009... | — |
| SYS-015 | Brake System Monitoring | D | STK-007, STK-016, STK-018 | TSR-015, TSR-016, SWR-FZC-010... | — |
| SYS-016 | Auto-Brake on CAN Communication Loss | D | STK-007, STK-017 | TSR-017, SWR-FZC-011 | — |
| SYS-017 | Emergency Braking from Obstacle Detection | C | STK-007, STK-008 | TSR-018, SWR-FZC-014 | — |
| SYS-018 | Lidar Distance Sensing | C | STK-008 | TSR-018, TSR-019, SWR-FZC-013 | — |
| SYS-019 | Graduated Obstacle Response | C | STK-008 | TSR-018, TSR-019, SWR-FZC-004... | — |
| SYS-020 | Lidar Sensor Plausibility Checking | C | STK-008, STK-016 | TSR-020, TSR-021, SWR-FZC-015... | — |
| SYS-021 | Heartbeat Transmission by Zone ECUs | C | STK-009, STK-019 | TSR-025, TSR-026, SWR-CVC-020... | — |
| SYS-022 | Heartbeat Timeout Detection by Safety Controller | C | STK-009, STK-019 | TSR-027, TSR-028, SWR-CVC-022... | — |
| SYS-023 | Cross-Plausibility Check — Torque vs. Current | C | STK-009, STK-016 | TSR-041, TSR-042, SWR-SC-007... | — |
| SYS-024 | Kill Relay Control — Energize-to-Run | D | STK-009, STK-019 | TSR-029, TSR-030, SWR-SC-006... | — |
| SYS-025 | Safety Controller CAN Listen-Only Mode | C | STK-009, STK-004 | SWR-SC-001, SWR-SC-002, SWR-SC-026 | — |
| SYS-026 | Safety Controller Lockstep CPU Monitoring | D | STK-004, STK-009 | SWR-SC-002, SWR-SC-014, SWR-SC-015... | — |
| SYS-027 | External Watchdog Monitoring (All Physical ECUs) | D | STK-009, STK-019 | TSR-031, TSR-032, SWR-BSW-021... | — |
| SYS-028 | E-Stop Detection and Broadcast | B | STK-010, STK-016 | TSR-033, TSR-034, SWR-CVC-018... | — |
| SYS-029 | Vehicle State Machine | D | STK-005, STK-006, STK-007... | TSR-035, TSR-036, TSR-037... | — |
| SYS-030 | Coordinated Mode Management via BswM | D | STK-005, STK-006, STK-007... | SWR-BCM-002, SWR-BSW-001, SWR-BSW-022... | — |
| SYS-031 | CAN Bus Configuration | D | STK-022 | SWR-BCM-001, SWR-BSW-001, SWR-BSW-002... | — |
| SYS-032 | E2E Protection on Safety-Critical Messages | D | STK-020 | TSR-022, TSR-023, TSR-024... | — |
| SYS-033 | CAN Message Priority Assignment | D | STK-022 | SWR-BSW-003, SWR-CVC-016, SWR-CVC-017 | — |
| SYS-034 | CAN Bus Loss Detection per ECU | C | STK-009, STK-017 | TSR-038, TSR-039, SWR-BSW-004... | — |
| SYS-035 | Automatic Headlight Control | QM | STK-011 | SWR-BCM-001, SWR-BCM-002, SWR-BCM-003... | — |
| SYS-036 | Turn Indicator and Hazard Light Control | QM | STK-011, STK-026 | SWR-BCM-002, SWR-BCM-006, SWR-BCM-007... | — |
| SYS-037 | UDS Diagnostic Session Control | QM | STK-012 | SWR-BSW-013, SWR-BSW-017, SWR-CVC-033... | — |
| SYS-038 | UDS Read and Clear DTC Services | QM | STK-012, STK-013 | SWR-BSW-017, SWR-CVC-033, SWR-CVC-034... | — |
| SYS-039 | UDS Read/Write Data by Identifier | QM | STK-012 | SWR-BSW-017, SWR-CVC-031, SWR-CVC-033... | — |
| SYS-040 | UDS Security Access | QM | STK-012 | SWR-BSW-017, SWR-CVC-033, SWR-TCU-005... | — |
| SYS-041 | DTC Storage and Persistence | QM | STK-013 | SWR-BSW-004, SWR-BSW-018, SWR-BSW-019... | — |
| SYS-042 | MQTT Telemetry to AWS IoT Core | QM | STK-014 | SWR-GW-001, SWR-GW-002 | — |
| SYS-043 | Edge ML Anomaly Detection | QM | STK-015 | SWR-GW-003, SWR-GW-004 | — |
| SYS-044 | OLED Status Display | QM | STK-026 | SWR-CVC-002, SWR-CVC-026, SWR-CVC-027... | — |
| SYS-045 | Audible Warning via Buzzer | QM | STK-026 | SWR-FZC-005, SWR-FZC-017, SWR-FZC-018 | — |
| SYS-046 | Fault LED Panel on Safety Controller | C | STK-026, STK-019 | SWR-ICU-006, SWR-ICU-009, SWR-SC-003... | — |
| SYS-047 | SPI Interface — Pedal and Steering Sensors | D | STK-005, STK-006, STK-023 | SWR-BSW-005, SWR-BSW-006, SWR-BSW-014... | — |
| SYS-048 | UART Interface — Lidar Sensor | C | STK-008, STK-023 | SWR-BSW-006, SWR-FZC-013 | — |
| SYS-049 | ADC Interface — Current, Temperature, Voltage | A | STK-005, STK-023 | SWR-BSW-007, SWR-BSW-014, SWR-RZC-005... | — |
| SYS-050 | PWM Interface — Motor and Servos | D | STK-005, STK-006, STK-007 | SWR-BSW-008, SWR-BSW-009, SWR-BSW-014... | — |
| SYS-051 | MISRA C Compliance | D | STK-030 | SWR-ALL-001 | — |
| SYS-052 | Static RAM Only — No Dynamic Allocation | D | STK-016, STK-030 | SWR-ALL-002 | — |
| SYS-053 | WCET Within Deadline Margin | D | STK-021, STK-018 | SWR-ALL-003, SWR-BSW-010, SWR-BSW-027... | — |
| SYS-054 | Flash Memory Utilization | QM | STK-021 | SWR-ALL-004 | — |
| SYS-055 | Bidirectional Traceability Chain | D | STK-031 | — | — |
| SYS-056 | SAP QM Mock Integration | QM | STK-032 | SWR-GW-005, SWR-GW-006 | — |

### SG (8 requirements)

| ID | Title | ASIL | Parents | Children | Tested |
|---|---|---|---|---|---|
| SG-001 | Prevent Unintended Acceleration -- FTTI = 50 ms | — | — | FSR-001, FSR-002, FSR-003... | — |
| SG-002 | Prevent Unintended Loss of Drive Torque -- FTTI... | — | — | FSR-013, FSR-019, FSR-023... | — |
| SG-003 | Prevent Unintended Steering Movement -- FTTI = ... | — | — | FSR-006, FSR-007, FSR-008... | — |
| SG-004 | Prevent Unintended Loss of Braking -- FTTI = 50 ms | — | — | FSR-009, FSR-010, FSR-013... | — |
| SG-005 | Prevent Unintended Braking -- FTTI = 200 ms | — | — | FSR-009, FSR-013, FSR-019... | — |
| SG-006 | Ensure Motor Protection -- FTTI = 500 ms | — | — | FSR-004, FSR-005, FSR-013... | — |
| SG-007 | Ensure Obstacle Detection -- FTTI = 200 ms | — | — | FSR-011, FSR-012, FSR-013... | — |
| SG-008 | Ensure Independent Safety Monitoring Availabili... | — | — | FSR-013, FSR-014, FSR-015... | — |

### FSR (26 requirements)

| ID | Title | ASIL | Parents | Children | Tested |
|---|---|---|---|---|---|
| FSR-001 | Dual Pedal Sensor Plausibility Monitoring | D | SG-001 | TSR-001, TSR-002 | — |
| FSR-002 | Pedal Sensor Fault Detection Within 20 ms | D | SG-001 | TSR-003, TSR-002 | — |
| FSR-003 | Motor Torque Cutoff on Pedal Plausibility Failure | D | SG-001 | TSR-004, TSR-005 | — |
| FSR-004 | Motor Current Monitoring and Overcurrent Cutoff | A | SG-006 | TSR-006, TSR-007 | — |
| FSR-005 | Motor Temperature Monitoring and Derating | A | SG-006 | TSR-008, TSR-009 | — |
| FSR-006 | Steering Angle Feedback Monitoring | D | SG-003 | TSR-010, TSR-011 | — |
| FSR-007 | Steering Return-to-Center on Fault | D | SG-003 | TSR-012, TSR-013 | — |
| FSR-008 | Steering Rate Limiting | C | SG-003 | TSR-014 | — |
| FSR-009 | Brake Command Monitoring | D | SG-004, SG-005 | TSR-015, TSR-016 | — |
| FSR-010 | Auto-Brake on CAN Timeout | D | SG-004 | TSR-017 | — |
| FSR-011 | Lidar Distance Monitoring and Obstacle Detection | C | SG-007 | TSR-018, TSR-019 | — |
| FSR-012 | Lidar Sensor Plausibility Check | C | SG-007 | TSR-020, TSR-021, TSR-018 | — |
| FSR-013 | CAN E2E Protection on Safety Messages | D | SG-001, SG-002, SG-003... | TSR-022, TSR-023, TSR-024 | — |
| FSR-014 | Heartbeat Transmission by All Zone ECUs | C | SG-008 | TSR-025, TSR-026 | — |
| FSR-015 | Heartbeat Timeout Detection by Safety Controller | C | SG-008 | TSR-027, TSR-028 | — |
| FSR-016 | Kill Relay Activation on Safety Goal Violation | D | SG-001, SG-003, SG-004... | TSR-029, TSR-030, TSR-028... | — |
| FSR-017 | External Watchdog Monitoring Per ECU | D | SG-008 | TSR-031, TSR-032, TSR-026... | — |
| FSR-018 | E-Stop Broadcast and Immediate Motor Cutoff | B | SG-001, SG-008 | TSR-033, TSR-034 | — |
| FSR-019 | Vehicle State Machine Management | D | SG-001, SG-002, SG-003... | TSR-035, TSR-036, TSR-037 | — |
| FSR-020 | CAN Bus Loss Detection | C | SG-008 | TSR-038, TSR-039 | — |
| FSR-021 | Motor Direction Control Plausibility | C | SG-001 | TSR-040 | — |
| FSR-022 | Cross-Plausibility — Torque Request vs. Actual ... | C | SG-001 | TSR-041, TSR-042, TSR-007 | — |
| FSR-023 | Operator Warning on Degradation | B | SG-001, SG-002, SG-003... | TSR-043, TSR-044, TSR-045 | — |
| FSR-024 | Safe State Transition Within FTTI | D | SG-001, SG-002, SG-003... | TSR-046, TSR-047 | — |
| FSR-025 | Loss of Braking Detection and Motor Cutoff | D | SG-004 | TSR-048, TSR-049, TSR-016 | — |
| FSR-026 | Standstill Motor Current Cross-Plausibility | D | SG-001 | TSR-052, SSR-SC-018 | — |

### TSR (52 requirements)

| ID | Title | ASIL | Parents | Children | Tested |
|---|---|---|---|---|---|
| TSR-001 | CVC Dual Pedal Sensor SPI Acquisition | D | FSR-001, SG-001, SYS-001 | SSR-CVC-001, SSR-CVC-002, HSR-CVC-001... | — |
| TSR-002 | CVC Dual Pedal Sensor Plausibility Comparison | D | FSR-001, FSR-002, SG-001... | SSR-CVC-003, SSR-CVC-004, HSR-CVC-001... | — |
| TSR-003 | CVC Pedal Sensor Individual Diagnostics | D | FSR-002, SG-001, SYS-002 | SSR-CVC-005, SWR-CVC-005 | — |
| TSR-004 | CVC Zero-Torque Command on Pedal Fault | D | FSR-003, SG-001, SYS-002 | SSR-CVC-006, SSR-CVC-007, SWR-BSW-018... | — |
| TSR-005 | RZC Motor Disable on Zero-Torque Command | D | FSR-003, SG-001, SYS-004 | SSR-RZC-001, SSR-RZC-002, HSR-RZC-004... | — |
| TSR-006 | RZC Motor Current Sampling and Overcurrent Dete... | A | FSR-004, SG-006, SYS-005 | SSR-RZC-003, SSR-RZC-004, HSR-RZC-001... | — |
| TSR-007 | RZC Motor Current Broadcast to SC | C | FSR-004, FSR-022, SG-001... | SSR-RZC-005, SWR-RZC-007 | — |
| TSR-008 | RZC Motor Temperature Sampling | A | FSR-005, SG-006, SYS-006 | SSR-RZC-006, HSR-RZC-002, SWR-BSW-007... | — |
| TSR-009 | RZC Motor Temperature Derating Enforcement | A | FSR-005, SG-006, SYS-006 | SSR-RZC-007, HSR-RZC-007, SWR-RZC-010 | — |
| TSR-010 | FZC Steering Angle Sensor Acquisition | D | FSR-006, SG-003, SYS-011 | SSR-FZC-001, HSR-FZC-001, SWR-BSW-006... | — |
| TSR-011 | FZC Steering Angle Feedback Plausibility | D | FSR-006, SG-003, SYS-011 | SSR-FZC-002, SSR-FZC-003, SWR-BSW-018... | — |
| TSR-012 | FZC Steering Return-to-Center Command | D | FSR-007, SG-003, SYS-010... | SSR-FZC-004, SSR-FZC-005, HSR-FZC-006... | — |
| TSR-013 | FZC Steering Servo PWM Disable on Persistent Fault | D | FSR-007, SG-003, SYS-012 | SSR-FZC-006, HSR-FZC-006, SWR-FZC-006 | — |
| TSR-014 | FZC Steering Rate Limiting | C | FSR-008, SG-003, SYS-013 | SSR-FZC-007, SWR-FZC-007 | — |
| TSR-015 | FZC Brake Servo PWM Command Verification | D | FSR-009, SG-004, SG-005... | SSR-FZC-008, HSR-FZC-002, SWR-FZC-009 | — |
| TSR-016 | FZC Brake System Fault Notification | D | FSR-009, FSR-025, SG-004... | SSR-FZC-009, SWR-FZC-010 | — |
| TSR-017 | FZC Auto-Brake on CAN Command Timeout | D | FSR-010, SG-004, SYS-016 | SSR-FZC-010, SWR-FZC-011 | — |
| TSR-018 | FZC Lidar Sensor UART Acquisition | C | FSR-011, FSR-012, SG-007... | SSR-FZC-011, HSR-FZC-003, SWR-FZC-013 | — |
| TSR-019 | FZC Lidar Graduated Response | C | FSR-011, SG-007, SYS-018... | SSR-FZC-012, SWR-FZC-014 | — |
| TSR-020 | FZC Lidar Range and Stuck Sensor Check | C | FSR-012, SG-007, SYS-020 | SSR-FZC-013, SWR-BSW-018, SWR-FZC-015 | — |
| TSR-021 | FZC Lidar Sensor Fault Safe Default | C | FSR-012, SG-007, SYS-020 | SSR-FZC-014, SWR-FZC-016 | — |
| TSR-022 | E2E Protection Header Format | D | FSR-013, SG-001, SG-008... | SSR-CVC-008, SSR-FZC-015, SSR-RZC-008... | — |
| TSR-023 | E2E Transmitter Behavior | D | FSR-013, SG-001, SG-008... | SSR-CVC-009, SSR-FZC-016, SSR-RZC-009... | — |
| TSR-024 | E2E Receiver Behavior | D | FSR-013, SG-001, SG-008... | SSR-CVC-010, SSR-FZC-017, SSR-RZC-010... | — |
| TSR-025 | Zone ECU Heartbeat Transmission | C | FSR-014, SG-008, SYS-021 | SSR-CVC-011, SSR-FZC-018, SSR-RZC-011... | — |
| TSR-026 | Zone ECU Heartbeat Conditioning on Self-Check | C | FSR-014, FSR-017, SG-008... | SSR-CVC-012, SSR-FZC-019, SSR-RZC-012... | — |
| TSR-027 | SC Heartbeat Timeout Detection | C | FSR-015, SG-008, SYS-022 | SSR-SC-003, SSR-SC-004, SWR-SC-004... | — |
| TSR-028 | SC Heartbeat Timeout Confirmation and Kill Relay | D | FSR-015, FSR-016, SG-008... | SSR-SC-005, SWR-SC-006 | — |
| TSR-029 | SC Kill Relay GPIO Control | D | FSR-016, SG-001, SG-003... | SSR-SC-006, HSR-SC-001, HSR-SC-006... | — |
| TSR-030 | SC Kill Relay De-energize Conditions | D | FSR-016, SG-001, SG-003... | SSR-SC-007, HSR-SC-006, SWR-SC-011... | — |
| TSR-031 | External Watchdog Feed Conditioning | D | FSR-017, SG-008, SYS-027 | SSR-CVC-013, SSR-FZC-020, SSR-RZC-013... | — |
| TSR-032 | External Watchdog Hardware Configuration | D | FSR-017, SG-008, SYS-027 | HSR-CVC-002, HSR-FZC-004, HSR-RZC-003... | — |
| TSR-033 | CVC E-Stop GPIO Detection | B | FSR-018, SG-001, SG-008... | SSR-CVC-014, HSR-CVC-003, SWR-BSW-009... | — |
| TSR-034 | CVC E-Stop CAN Broadcast and Local Reaction | B | FSR-018, SG-001, SG-008... | SSR-CVC-015, SWR-CVC-019 | — |
| TSR-035 | CVC Vehicle State Machine Implementation | D | FSR-019, SG-001, SG-008... | SSR-CVC-016, SSR-CVC-017, SWR-BSW-022... | — |
| TSR-036 | CVC Vehicle State CAN Broadcast | D | FSR-019, SG-001, SG-008... | SSR-CVC-018, SWR-BSW-022, SWR-CVC-011 | — |
| TSR-037 | CVC State Persistence in Non-Volatile Memory | D | FSR-019, SG-001, SG-008... | SSR-CVC-019, SWR-BSW-020, SWR-CVC-012... | — |
| TSR-038 | Per-ECU CAN Bus Loss Detection | C | FSR-020, SG-008, SYS-034 | SSR-CVC-020, SSR-FZC-021, SSR-RZC-014... | — |
| TSR-039 | CVC CAN Bus Recovery | C | FSR-020, SG-008, SYS-034 | SSR-CVC-021, SWR-BSW-004, SWR-CVC-025 | — |
| TSR-040 | RZC Motor Direction Plausibility and Shoot-Thro... | C | FSR-021, SG-001, SYS-007 | SSR-RZC-015, SSR-RZC-016, HSR-RZC-004... | — |
| TSR-041 | SC Torque-Current Cross-Plausibility Check | C | FSR-022, SG-001, SYS-023 | SSR-SC-010, SSR-SC-011, SWR-SC-007... | — |
| TSR-042 | SC Cross-Plausibility Fault Reaction | C | FSR-022, SG-001, SYS-023 | SSR-SC-012, SWR-SC-009 | — |
| TSR-043 | CVC OLED Warning Display | B | FSR-023, SG-001, SG-008 | SSR-CVC-022, HSR-CVC-005, SWR-CVC-027 | — |
| TSR-044 | FZC Buzzer Warning Patterns | B | FSR-023, SG-001, SG-008 | SSR-FZC-022, HSR-FZC-007, SWR-FZC-017 | — |
| TSR-045 | SC Fault LED Indication | B | FSR-023, SG-001, SG-008 | SSR-SC-013, HSR-SC-003, SWR-SC-013 | — |
| TSR-046 | FTTI Compliance Verification per Safety Goal | D | FSR-024, SG-001, SG-008 | SSR-CVC-023, SSR-FZC-023, SSR-RZC-017... | — |
| TSR-047 | Diagnostic Cycle Time Configuration | D | FSR-024, SG-001, SG-008 | SSR-CVC-023, SSR-FZC-023, SSR-RZC-017... | — |
| TSR-048 | FZC Loss-of-Braking Detection and Motor Cutoff ... | D | FSR-025, SG-004 | SSR-FZC-024, SWR-FZC-012 | — |
| TSR-049 | SC Backup Motor Cutoff on Loss of Braking | D | FSR-025, SG-004 | SSR-SC-015, SWR-SC-024 | — |
| TSR-050 | SC Startup Self-Test Sequence | C | FSR-016, FSR-017, SG-008 | SSR-SC-016, HSR-SC-005, SWR-SC-016... | — |
| TSR-051 | SC Runtime Periodic Self-Test | C | FSR-017, SG-008 | SSR-SC-017, HSR-SC-005, SWR-SC-020 | — |
| TSR-052 | SC Standstill Motor Current Cross-Plausibility | D | FSR-026, SG-001 | SSR-SC-018 | — |

### SSR (82 requirements)

| ID | Title | ASIL | Parents | Children | Tested |
|---|---|---|---|---|---|
| SSR-CVC-001 | SPI1 Initialization for Dual Pedal Sensors | D | TSR-001 | SWR-BSW-006, SWR-CVC-001 | — |
| SSR-CVC-002 | Pedal Sensor SPI Read Cycle | D | TSR-001 | SWR-CVC-002 | — |
| SSR-CVC-003 | Dual Pedal Sensor Plausibility Comparison | D | TSR-002 | SWR-CVC-003 | — |
| SSR-CVC-004 | Pedal Sensor Stuck-at Detection | D | TSR-002 | SWR-CVC-004 | — |
| SSR-CVC-005 | Pedal Sensor Individual Diagnostics | D | TSR-003 | SWR-CVC-005 | — |
| SSR-CVC-006 | Zero-Torque Latching on Pedal Fault | D | TSR-004 | SWR-CVC-006 | — |
| SSR-CVC-007 | Zero-Torque CAN Transmission | D | TSR-004 | SWR-BSW-015, SWR-CVC-007, SWR-CVC-008 | — |
| SSR-CVC-008 | CAN E2E Protection — Transmit Side | D | TSR-022, TSR-023 | SWR-BSW-001, SWR-BSW-023, SWR-CVC-014 | — |
| SSR-CVC-009 | CAN E2E Protection — Transmit Counter Management | D | TSR-023 | SWR-CVC-014 | — |
| SSR-CVC-010 | CAN E2E Protection — Receive Side | D | TSR-024 | SWR-CVC-015 | — |
| SSR-CVC-011 | CVC Heartbeat Transmission | C | TSR-025 | SWR-CVC-020 | — |
| SSR-CVC-012 | CVC Heartbeat Conditioning on Self-Check | C | TSR-026 | SWR-CVC-021 | — |
| SSR-CVC-013 | CVC External Watchdog Feed | D | TSR-031 | SWR-BSW-021, SWR-CVC-023 | — |
| SSR-CVC-014 | CVC E-Stop GPIO Interrupt Handler | B | TSR-033 | SWR-BSW-009, SWR-CVC-018 | — |
| SSR-CVC-015 | CVC E-Stop CAN Broadcast | B | TSR-034 | SWR-CVC-019 | — |
| SSR-CVC-016 | Vehicle State Machine Transition Logic | D | TSR-035 | SWR-BSW-022, SWR-CVC-009 | — |
| SSR-CVC-017 | Vehicle State Machine Entry Actions | D | TSR-035 | SWR-BSW-022, SWR-CVC-010 | — |
| SSR-CVC-018 | Vehicle State CAN Broadcast | D | TSR-036 | SWR-BSW-015, SWR-CVC-011 | — |
| SSR-CVC-019 | Vehicle State NVM Persistence | D | TSR-037 | SWR-CVC-012 | — |
| SSR-CVC-020 | CVC CAN Bus Loss Detection | C | TSR-038 | SWR-CVC-024 | — |
| SSR-CVC-021 | CVC CAN Bus Recovery Attempt | C | TSR-039 | SWR-CVC-025 | — |
| SSR-CVC-022 | CVC OLED Warning Display Update | B | TSR-043 | SWR-CVC-027 | — |
| SSR-CVC-023 | CVC WCET Compliance for Safety Runnables | D | TSR-046, TSR-047 | SWR-BSW-027, SWR-CVC-032 | — |
| SSR-FZC-001 | Steering Angle Sensor SPI Read | D | TSR-010 | SWR-BSW-006, SWR-FZC-001 | — |
| SSR-FZC-002 | Steering Command-vs-Feedback Comparison | D | TSR-011 | SWR-FZC-002 | — |
| SSR-FZC-003 | Steering Sensor Range and Rate Check | D | TSR-011 | SWR-FZC-003 | — |
| SSR-FZC-004 | Steering Return-to-Center Execution | D | TSR-012 | SWR-BSW-008, SWR-FZC-004 | — |
| SSR-FZC-005 | Steering Return-to-Center CAN Notification | D | TSR-012 | SWR-FZC-005 | — |
| SSR-FZC-006 | Steering Servo PWM Disable | D | TSR-013 | SWR-FZC-006 | — |
| SSR-FZC-007 | Steering Rate Limiter | C | TSR-014 | SWR-FZC-007 | — |
| SSR-FZC-008 | Brake Servo PWM Output and Feedback | D | TSR-015 | SWR-FZC-009 | — |
| SSR-FZC-009 | Brake Fault CAN Notification | D | TSR-016 | SWR-FZC-010 | — |
| SSR-FZC-010 | Auto-Brake on CAN Command Timeout | D | TSR-017 | SWR-FZC-011 | — |
| SSR-FZC-011 | Lidar UART Frame Reception | C | TSR-018 | SWR-FZC-013 | — |
| SSR-FZC-012 | Lidar Graduated Response Logic | C | TSR-019 | SWR-FZC-014 | — |
| SSR-FZC-013 | Lidar Range and Stuck Sensor Detection | C | TSR-020 | SWR-FZC-015 | — |
| SSR-FZC-014 | Lidar Fault Safe Default Substitution | C | TSR-021 | SWR-FZC-016 | — |
| SSR-FZC-015 | FZC CAN E2E Transmit | D | TSR-022, TSR-023 | SWR-BSW-023, SWR-FZC-019 | — |
| SSR-FZC-016 | FZC CAN E2E Transmit Counter Management | D | TSR-023 | SWR-FZC-019 | — |
| SSR-FZC-017 | FZC CAN E2E Receive | D | TSR-024 | SWR-FZC-020 | — |
| SSR-FZC-018 | FZC Heartbeat Transmission | C | TSR-025 | SWR-FZC-021 | — |
| SSR-FZC-019 | FZC Heartbeat Conditioning | C | TSR-026 | SWR-FZC-022 | — |
| SSR-FZC-020 | FZC External Watchdog Feed | D | TSR-031 | SWR-BSW-021, SWR-FZC-023 | — |
| SSR-FZC-021 | FZC CAN Bus Loss Detection | C | TSR-038 | SWR-FZC-024 | — |
| SSR-FZC-022 | FZC Buzzer Pattern Generation | B | TSR-044 | SWR-FZC-017 | — |
| SSR-FZC-023 | FZC WCET Compliance for Safety Runnables | D | TSR-046, TSR-047 | SWR-BSW-027, SWR-FZC-029 | — |
| SSR-FZC-024 | FZC Loss-of-Braking Motor Cutoff Request | D | TSR-048 | SWR-FZC-012 | — |
| SSR-RZC-001 | RZC Motor Driver Disable on Zero-Torque | D | TSR-005 | SWR-BSW-008, SWR-BSW-009, SWR-RZC-001 | — |
| SSR-RZC-002 | RZC Motor Driver State Validation | D | TSR-005 | SWR-RZC-002 | — |
| SSR-RZC-003 | Motor Current ADC Sampling | A | TSR-006 | SWR-BSW-007, SWR-RZC-005 | — |
| SSR-RZC-004 | Motor Overcurrent Detection and Cutoff | A | TSR-006 | SWR-RZC-006 | — |
| SSR-RZC-005 | Motor Current CAN Broadcast | C | TSR-007 | SWR-RZC-007 | — |
| SSR-RZC-006 | Motor Temperature ADC Sampling | A | TSR-008 | SWR-BSW-007, SWR-RZC-009 | — |
| SSR-RZC-007 | Motor Temperature Derating Logic | A | TSR-009 | SWR-RZC-010 | — |
| SSR-RZC-008 | RZC CAN E2E Transmit | D | TSR-022, TSR-023 | SWR-BSW-023, SWR-RZC-019 | — |
| SSR-RZC-009 | RZC CAN E2E Transmit Counter | D | TSR-023 | SWR-RZC-019 | — |
| SSR-RZC-010 | RZC CAN E2E Receive | D | TSR-024 | SWR-RZC-020 | — |
| SSR-RZC-011 | RZC Heartbeat Transmission | C | TSR-025 | SWR-RZC-021 | — |
| SSR-RZC-012 | RZC Heartbeat Conditioning | C | TSR-026 | SWR-RZC-022 | — |
| SSR-RZC-013 | RZC External Watchdog Feed | D | TSR-031 | SWR-BSW-021, SWR-RZC-023 | — |
| SSR-RZC-014 | RZC CAN Bus Loss Detection | C | TSR-038 | SWR-RZC-024 | — |
| SSR-RZC-015 | Motor Direction Plausibility Check | C | TSR-040 | SWR-RZC-014 | — |
| SSR-RZC-016 | Motor Shoot-Through Protection | C | TSR-040 | SWR-RZC-015 | — |
| SSR-RZC-017 | RZC WCET Compliance for Safety Runnables | D | TSR-046, TSR-047 | SWR-BSW-027, SWR-RZC-028 | — |
| SSR-SC-001 | SC CAN E2E Receive — Listen-Only | D | TSR-022, TSR-024 | SWR-SC-001, SWR-SC-003 | — |
| SSR-SC-002 | SC CAN Message Filtering | D | TSR-024 | SWR-SC-002 | — |
| SSR-SC-003 | SC Heartbeat Timeout Counter Management | C | TSR-027 | SWR-SC-004, SWR-SC-027, SWR-SC-028 | — |
| SSR-SC-004 | SC Fault LED Activation on Heartbeat Timeout | C | TSR-027 | SWR-SC-005 | — |
| SSR-SC-005 | SC Heartbeat Timeout Confirmation and Relay De-... | D | TSR-028 | SWR-SC-006 | — |
| SSR-SC-006 | SC Kill Relay GPIO Control | D | TSR-029 | SWR-SC-010 | — |
| SSR-SC-007 | SC Kill Relay De-energize Trigger Logic | D | TSR-030 | SWR-SC-011 | — |
| SSR-SC-008 | SC External Watchdog Feed | D | TSR-031 | SWR-SC-022 | — |
| SSR-SC-009 | SC CAN Bus Loss Detection | C | TSR-038 | SWR-SC-023 | — |
| SSR-SC-010 | SC Torque-Current Cross-Plausibility Lookup | C | TSR-041 | SWR-SC-007 | — |
| SSR-SC-011 | SC Cross-Plausibility Debounce and Fault Detection | C | TSR-041 | SWR-SC-008 | — |
| SSR-SC-012 | SC Cross-Plausibility Fault Reaction | C | TSR-042 | SWR-SC-009 | — |
| SSR-SC-013 | SC Fault LED Management | B | TSR-045 | SWR-SC-013 | — |
| SSR-SC-014 | SC FTTI Monitoring | D | TSR-046 | SWR-SC-025 | — |
| SSR-SC-015 | SC Backup Motor Cutoff via Kill Relay | D | TSR-049 | SWR-SC-024 | — |
| SSR-SC-016 | SC Startup Self-Test Sequence | C | TSR-050 | SWR-SC-016, SWR-SC-017, SWR-SC-018... | — |
| SSR-SC-017 | SC Runtime Periodic Self-Test | C | TSR-051 | SWR-SC-020 | — |
| SSR-SC-018 | SC Standstill Torque Cross-Plausibility Monitor | D | TSR-052, FSR-026 | — | — |

### HSR (25 requirements)

| ID | Title | ASIL | Parents | Children | Tested |
|---|---|---|---|---|---|
| HSR-CVC-001 | Dual AS5048A Pedal Sensor SPI Bus | D | TSR-001, TSR-002 | — | — |
| HSR-CVC-002 | CVC External Watchdog (TPS3823) | D | TSR-032 | — | — |
| HSR-CVC-003 | CVC E-Stop Input Circuit | B | TSR-033 | — | — |
| HSR-CVC-004 | CVC CAN Transceiver | D | TSR-022, TSR-038 | — | — |
| HSR-CVC-005 | CVC OLED Display Interface | B | TSR-043 | — | — |
| HSR-FZC-001 | FZC Steering Angle Sensor SPI Bus | D | TSR-010 | — | — |
| HSR-FZC-002 | FZC Brake Servo PWM Circuit | D | TSR-015 | — | — |
| HSR-FZC-003 | FZC Lidar UART Interface | C | TSR-018 | — | — |
| HSR-FZC-004 | FZC External Watchdog (TPS3823) | D | TSR-032 | — | — |
| HSR-FZC-005 | FZC CAN Transceiver | D | TSR-022, TSR-038 | — | — |
| HSR-FZC-006 | FZC Steering Servo PWM Circuit | D | TSR-012, TSR-013 | — | — |
| HSR-FZC-007 | FZC Buzzer Circuit | B | TSR-044 | — | — |
| HSR-RZC-001 | RZC ACS723 Motor Current Sensing Circuit | A | TSR-006 | — | — |
| HSR-RZC-002 | RZC NTC Temperature Sensing Circuit | A | TSR-008 | — | — |
| HSR-RZC-003 | RZC External Watchdog (TPS3823) | D | TSR-032 | — | — |
| HSR-RZC-004 | RZC BTS7960 Motor Driver Interface | C | TSR-005, TSR-040 | — | — |
| HSR-RZC-005 | RZC CAN Transceiver | D | TSR-022, TSR-038 | — | — |
| HSR-RZC-006 | RZC Motor Encoder Interface | C | TSR-040 | — | — |
| HSR-RZC-007 | RZC Battery Voltage Monitoring | A | TSR-009 | — | — |
| HSR-SC-001 | SC Kill Relay Circuit | D | TSR-029 | — | — |
| HSR-SC-002 | SC External Watchdog (TPS3823) | D | TSR-032 | — | — |
| HSR-SC-003 | SC Fault LED Circuit | B | TSR-045 | — | — |
| HSR-SC-004 | SC DCAN1 Listen-Only Mode Configuration | D | TSR-022, TSR-038 | — | — |
| HSR-SC-005 | SC TMS570LC43x Lockstep CPU Configuration | D | TSR-050, TSR-051 | — | — |
| HSR-SC-006 | SC Power Supply Independence | D | TSR-029, TSR-030 | — | — |

### SWR (201 requirements)

| ID | Title | ASIL | Parents | Children | Tested |
|---|---|---|---|---|---|
| SWR-ALL-001 | MISRA C:2012 Compliance | D | SYS-051 | — | — |
| SWR-ALL-002 | Static RAM Only — No Dynamic Allocation | D | SYS-052 | — | — |
| SWR-ALL-003 | WCET Within Deadline Margin | D | SYS-053 | — | — |
| SWR-ALL-004 | Flash Memory Utilization Below 80% | A | SYS-054 | — | — |
| SWR-BCM-001 | SocketCAN Interface Initialization | A | SYS-031, SYS-035 | — | Yes |
| SWR-BCM-002 | Vehicle State CAN Reception | A | SYS-029, SYS-030, SYS-035... | — | Yes |
| SWR-BCM-003 | Headlight Auto-On Control | A | SYS-035 | — | Yes |
| SWR-BCM-004 | Headlight Auto-Off Control | A | SYS-035 | — | Yes |
| SWR-BCM-005 | Headlight Manual Override | A | SYS-035 | — | Yes |
| SWR-BCM-006 | Turn Indicator Control | A | SYS-036 | — | Yes |
| SWR-BCM-007 | Hazard Light Activation | A | SYS-036 | — | Yes |
| SWR-BCM-008 | Hazard Light Deactivation | A | SYS-036 | — | Yes |
| SWR-BCM-009 | Door Lock Simulation | A | SYS-035 | — | Yes |
| SWR-BCM-010 | Body Status CAN Transmission | A | SYS-035, SYS-036 | — | Yes |
| SWR-BCM-011 | Body Control Command Reception | A | SYS-035, SYS-036 | — | Yes |
| SWR-BCM-012 | BCM Main Loop Execution | A | SYS-035 | — | Yes |
| SWR-BSW-001 | CAN Driver Initialization | D | SYS-031, TSR-022, SSR-CVC-008... | — | Yes |
| SWR-BSW-002 | CAN Driver Write | D | SYS-031, SYS-032, TSR-023 | — | — |
| SWR-BSW-003 | CAN Driver MainFunction_Read | D | SYS-031, SYS-032, TSR-024... | — | — |
| SWR-BSW-004 | CAN Driver Bus-Off Recovery | D | SYS-034, TSR-038, TSR-039... | — | Yes |
| SWR-BSW-005 | CAN Driver Error Callback | D | SYS-034, TSR-038, SYS-047 | — | — |
| SWR-BSW-006 | SPI Driver for AS5048A Sensors | D | SYS-047, TSR-001, TSR-010... | — | — |
| SWR-BSW-007 | ADC Driver for Analog Sensing | A | SYS-049, TSR-006, TSR-008... | — | — |
| SWR-BSW-008 | PWM Driver for Motor and Servo Control | D | SYS-050, TSR-005, TSR-012... | — | — |
| SWR-BSW-009 | DIO Driver for Digital I/O | D | SYS-050, TSR-005, TSR-033... | — | — |
| SWR-BSW-010 | GPT Driver for Timing | D | SYS-053, TSR-046, TSR-047 | — | Yes |
| SWR-BSW-011 | CanIf PDU Routing | D | SYS-031, SYS-032, TSR-022... | — | Yes |
| SWR-BSW-012 | CanIf Controller Mode Management | D | SYS-034, TSR-038 | — | — |
| SWR-BSW-013 | PduR Routing between CanIf, Com, and Dcm | D | SYS-031, SYS-032, SYS-037 | — | Yes |
| SWR-BSW-014 | IoHwAb Sensor and Actuator Abstraction | D | SYS-047, SYS-049, SYS-050... | — | — |
| SWR-BSW-015 | Com Signal Packing and Unpacking | D | SYS-032, TSR-022, TSR-023... | — | Yes |
| SWR-BSW-016 | Com E2E Integration | D | SYS-032, TSR-022, TSR-023... | — | Yes |
| SWR-BSW-017 | Dcm UDS Service Dispatch | A | SYS-037, SYS-038, SYS-039... | — | Yes |
| SWR-BSW-018 | Dem DTC Event Management | B | SYS-041, TSR-004, TSR-011... | — | Yes |
| SWR-BSW-019 | Dem Counter-Based Debouncing | B | SYS-041, TSR-002, TSR-006 | — | Yes |
| SWR-BSW-020 | Dem NVM Persistence | B | SYS-041, TSR-037 | — | Yes |
| SWR-BSW-021 | WdgM Alive Supervision | D | SYS-027, TSR-031, TSR-032... | — | — |
| SWR-BSW-022 | BswM Mode Management Rules | D | SYS-029, SYS-030, TSR-035... | — | Yes |
| SWR-BSW-023 | E2E CRC-8 Calculation | D | SYS-032, TSR-022, SSR-CVC-008... | — | Yes |
| SWR-BSW-024 | E2E Alive Counter and Data ID Management | D | SYS-032, TSR-022, TSR-023... | — | Yes |
| SWR-BSW-025 | E2E Per-PDU Configuration | D | SYS-032, TSR-022 | — | Yes |
| SWR-BSW-026 | Rte Port-Based Communication | D | SYS-029, SYS-030, TSR-035 | — | Yes |
| SWR-BSW-027 | Rte Runnable Scheduling | D | SYS-053, TSR-046, TSR-047... | — | Yes |
| SWR-CVC-001 | SPI1 Initialization for Dual Pedal Sensors | D | SYS-001, SYS-047, TSR-001... | — | Yes |
| SWR-CVC-002 | Pedal Sensor SPI Read Cycle | D | SYS-001, TSR-001, SSR-CVC-002... | — | Yes |
| SWR-CVC-003 | Dual Pedal Sensor Plausibility Comparison | D | SYS-002, TSR-002, SSR-CVC-003 | — | Yes |
| SWR-CVC-004 | Pedal Sensor Stuck-at Detection | D | SYS-002, TSR-002, SSR-CVC-004 | — | Yes |
| SWR-CVC-005 | Pedal Sensor Individual Diagnostics | D | SYS-002, TSR-003, SSR-CVC-005 | — | Yes |
| SWR-CVC-006 | Zero-Torque Latching on Pedal Fault | D | SYS-002, TSR-004, SSR-CVC-006 | — | Yes |
| SWR-CVC-007 | Pedal-to-Torque Mapping | D | SYS-003, TSR-004, SSR-CVC-007 | — | Yes |
| SWR-CVC-008 | Torque Request CAN Transmission | D | SYS-004, TSR-004, SSR-CVC-007 | — | Yes |
| SWR-CVC-009 | Vehicle State Machine Transition Logic | D | SYS-029, TSR-035, SSR-CVC-016 | — | Yes |
| SWR-CVC-010 | Vehicle State Machine Entry Actions | D | SYS-029, SYS-030, TSR-035... | — | Yes |
| SWR-CVC-011 | Vehicle State CAN Broadcast | D | SYS-029, TSR-036, SSR-CVC-018 | — | Yes |
| SWR-CVC-012 | Vehicle State NVM Persistence | D | SYS-029, TSR-037, SSR-CVC-019 | — | Yes |
| SWR-CVC-013 | State Recovery Logic on Startup | D | SYS-029, TSR-037 | — | Yes |
| SWR-CVC-014 | CAN E2E Protection -- Transmit Side | D | SYS-032, TSR-022, TSR-023... | — | Yes |
| SWR-CVC-015 | CAN E2E Protection -- Receive Side | D | SYS-032, TSR-024, SSR-CVC-010 | — | Yes |
| SWR-CVC-016 | CAN Message Reception and Routing | D | SYS-004, SYS-031, SYS-033 | — | Yes |
| SWR-CVC-017 | CAN Transmit Scheduling | D | SYS-004, SYS-031, SYS-033 | — | Yes |
| SWR-CVC-018 | E-Stop GPIO Interrupt Handler | B | SYS-028, TSR-033, SSR-CVC-014 | — | Yes |
| SWR-CVC-019 | E-Stop CAN Broadcast | B | SYS-028, TSR-034, SSR-CVC-015 | — | Yes |
| SWR-CVC-020 | CVC Heartbeat Transmission | C | SYS-021, TSR-025, SSR-CVC-011 | — | Yes |
| SWR-CVC-021 | CVC Heartbeat Conditioning on Self-Check | C | SYS-021, TSR-026, SSR-CVC-012 | — | Yes |
| SWR-CVC-022 | ECU Heartbeat Reception Monitoring | C | SYS-022, SYS-034 | — | Yes |
| SWR-CVC-023 | External Watchdog Feed | D | SYS-027, TSR-031, SSR-CVC-013 | — | Yes |
| SWR-CVC-024 | CAN Bus Loss Detection | C | SYS-034, TSR-038, SSR-CVC-020 | — | Yes |
| SWR-CVC-025 | CAN Bus Recovery Attempt | C | SYS-034, TSR-039, SSR-CVC-021 | — | Yes |
| SWR-CVC-026 | OLED Display Initialization | A | SYS-044 | — | Yes |
| SWR-CVC-027 | OLED Warning Display Update | A | SYS-044, TSR-043, SSR-CVC-022 | — | Yes |
| SWR-CVC-028 | OLED Display Speed and Telemetry in RUN Mode | A | SYS-044 | — | Yes |
| SWR-CVC-029 | Startup Self-Test Sequence | D | SYS-027, SYS-029 | — | Yes |
| SWR-CVC-030 | DTC Persistence in NVM | A | SYS-041 | — | Yes |
| SWR-CVC-031 | Calibration Data Storage | A | SYS-039 | — | Yes |
| SWR-CVC-032 | RTOS Task Configuration and WCET Compliance | D | SYS-053, TSR-046, TSR-047... | — | Yes |
| SWR-CVC-033 | UDS Service Routing via CAN | A | SYS-037, SYS-038, SYS-039... | — | Yes |
| SWR-CVC-034 | DTC Reporting via Dem | A | SYS-038, SYS-041 | — | Yes |
| SWR-CVC-035 | CVC Data Identifiers (DIDs) | A | SYS-039 | — | Yes |
| SWR-FZC-001 | Steering Angle Sensor SPI Read | D | SYS-011, SYS-047, TSR-010... | — | Yes |
| SWR-FZC-002 | Steering Command-vs-Feedback Comparison | D | SYS-011, TSR-011, SSR-FZC-002... | — | Yes |
| SWR-FZC-003 | Steering Sensor Range and Rate Check | D | SYS-011, TSR-011, SSR-FZC-003 | — | Yes |
| SWR-FZC-004 | Steering Return-to-Center Execution | D | SYS-012, TSR-012, SSR-FZC-004... | — | Yes |
| SWR-FZC-005 | Steering Return-to-Center CAN Notification | D | SYS-012, TSR-012, SSR-FZC-005... | — | Yes |
| SWR-FZC-006 | Steering Servo PWM Disable | D | SYS-012, TSR-013, SSR-FZC-006 | — | Yes |
| SWR-FZC-007 | Steering Rate Limiter | C | SYS-013, TSR-014, SSR-FZC-007 | — | Yes |
| SWR-FZC-008 | Steering Servo PWM Output | D | SYS-010, SYS-050 | — | Yes |
| SWR-FZC-009 | Brake Servo PWM Output and Feedback | D | SYS-014, SYS-050, TSR-015... | — | Yes |
| SWR-FZC-010 | Brake Fault CAN Notification | D | SYS-015, TSR-016, SSR-FZC-009 | — | Yes |
| SWR-FZC-011 | Auto-Brake on CAN Command Timeout | D | SYS-016, TSR-017, SSR-FZC-010 | — | Yes |
| SWR-FZC-012 | Loss-of-Braking Motor Cutoff Request | D | SYS-015, TSR-048, SSR-FZC-024 | — | Yes |
| SWR-FZC-013 | Lidar UART Frame Reception | C | SYS-018, SYS-048, TSR-018... | — | Yes |
| SWR-FZC-014 | Lidar Graduated Response Logic | C | SYS-017, SYS-019, TSR-019... | — | Yes |
| SWR-FZC-015 | Lidar Range and Stuck Sensor Detection | C | SYS-020, TSR-020, SSR-FZC-013 | — | Yes |
| SWR-FZC-016 | Lidar Fault Safe Default Substitution | C | SYS-020, TSR-021, SSR-FZC-014 | — | Yes |
| SWR-FZC-017 | Buzzer Pattern Generation | A | SYS-045, TSR-044, SSR-FZC-022 | — | Yes |
| SWR-FZC-018 | Buzzer Pattern from CAN State | A | SYS-045 | — | Yes |
| SWR-FZC-019 | FZC CAN E2E Transmit | D | SYS-032, TSR-022, TSR-023... | — | Yes |
| SWR-FZC-020 | FZC CAN E2E Receive | D | SYS-032, TSR-024, SSR-FZC-017 | — | Yes |
| SWR-FZC-021 | FZC Heartbeat Transmission | C | SYS-021, TSR-025, SSR-FZC-018 | — | Yes |
| SWR-FZC-022 | FZC Heartbeat Conditioning | C | SYS-021, TSR-026, SSR-FZC-019 | — | Yes |
| SWR-FZC-023 | FZC External Watchdog Feed | D | SYS-027, TSR-031, SSR-FZC-020 | — | Yes |
| SWR-FZC-024 | FZC CAN Bus Loss Detection | C | SYS-034, TSR-038, SSR-FZC-021 | — | Yes |
| SWR-FZC-025 | FZC Startup Self-Test Sequence | D | SYS-027, SYS-029 | — | Yes |
| SWR-FZC-026 | FZC CAN Message Reception | D | SYS-010, SYS-014, SYS-031 | — | Yes |
| SWR-FZC-027 | FZC CAN Message Transmission | D | SYS-015, SYS-031 | — | Yes |
| SWR-FZC-028 | Steering Command CAN Timeout | D | SYS-010, SYS-034 | — | Yes |
| SWR-FZC-029 | RTOS Task Configuration and WCET Compliance | D | SYS-053, TSR-046, TSR-047... | — | Yes |
| SWR-FZC-030 | UDS Service Support | A | SYS-037, SYS-038, SYS-039 | — | Yes |
| SWR-FZC-031 | FZC DTC Persistence | A | SYS-041 | — | Yes |
| SWR-FZC-032 | Servo Calibration Data Storage | A | SYS-010, SYS-014 | — | Yes |
| SWR-GW-001 | MQTT Telemetry Publishing | A | SYS-042 | — | Yes |
| SWR-GW-002 | CAN Bus Data Acquisition | A | SYS-042 | — | Yes |
| SWR-GW-003 | Edge ML Anomaly Detection | A | SYS-043 | — | Yes |
| SWR-GW-004 | ML Model Loading and Validation | A | SYS-043 | — | Yes |
| SWR-GW-005 | SAP QM DTC Forwarding | A | SYS-056 | — | Yes |
| SWR-GW-006 | 8D Report Template Generation | A | SYS-056 | — | Yes |
| SWR-ICU-001 | SocketCAN Interface Initialization | A | SYS-031 | — | Yes |
| SWR-ICU-002 | Speed Gauge Data Acquisition | A | SYS-009, SYS-044 | — | Yes |
| SWR-ICU-003 | Torque Gauge Data Acquisition | A | SYS-044 | — | Yes |
| SWR-ICU-004 | Temperature Gauge Data Acquisition | A | SYS-006, SYS-044 | — | Yes |
| SWR-ICU-005 | Battery Gauge Data Acquisition | A | SYS-008, SYS-044 | — | Yes |
| SWR-ICU-006 | Warning Indicator Management | A | SYS-044, SYS-046 | — | Yes |
| SWR-ICU-007 | Vehicle State Display | A | SYS-029, SYS-044 | — | Yes |
| SWR-ICU-008 | DTC Display | A | SYS-038, SYS-041, SYS-044 | — | Yes |
| SWR-ICU-009 | ECU Health Monitoring Display | A | SYS-021, SYS-044, SYS-046 | — | Yes |
| SWR-ICU-010 | ICU Main Loop Execution | A | SYS-044 | — | Yes |
| SWR-RZC-001 | Motor Driver Disable on Zero-Torque | D | SYS-004, TSR-005, SSR-RZC-001 | — | Yes |
| SWR-RZC-002 | Motor Driver State Validation | D | SYS-004, TSR-005, SSR-RZC-002... | — | Yes |
| SWR-RZC-003 | Torque-to-PWM Conversion | D | SYS-004, SYS-050, SYS-008 | — | Yes |
| SWR-RZC-004 | Motor PWM Output Configuration | D | SYS-050, SYS-009 | — | Yes |
| SWR-RZC-005 | Motor Current ADC Sampling | A | SYS-005, SYS-049, TSR-006... | — | Yes |
| SWR-RZC-006 | Motor Overcurrent Detection and Cutoff | A | SYS-005, TSR-006, SSR-RZC-004 | — | Yes |
| SWR-RZC-007 | Motor Current CAN Broadcast | C | SYS-005, TSR-007, SSR-RZC-005 | — | Yes |
| SWR-RZC-008 | ACS723 Zero-Current Calibration | A | SYS-005, SYS-049 | — | Yes |
| SWR-RZC-009 | Motor Temperature ADC Sampling | A | SYS-006, SYS-049, TSR-008... | — | Yes |
| SWR-RZC-010 | Motor Temperature Derating Logic | A | SYS-006, TSR-009, SSR-RZC-007 | — | Yes |
| SWR-RZC-011 | Temperature CAN Broadcast | A | SYS-006 | — | Yes |
| SWR-RZC-012 | Quadrature Encoder Speed Measurement | C | SYS-009, SYS-007 | — | Yes |
| SWR-RZC-013 | Motor Stall Detection | C | SYS-007 | — | Yes |
| SWR-RZC-014 | Motor Direction Plausibility Check | C | SYS-007, TSR-040, SSR-RZC-015 | — | Yes |
| SWR-RZC-015 | Motor Shoot-Through Protection | C | SYS-007, TSR-040, SSR-RZC-016 | — | Yes |
| SWR-RZC-016 | Motor CAN Command Timeout | D | SYS-004, SYS-034 | — | Yes |
| SWR-RZC-017 | Battery Voltage ADC Sampling | A | SYS-008, SYS-049 | — | Yes |
| SWR-RZC-018 | Battery Voltage Threshold Monitoring | A | SYS-008 | — | Yes |
| SWR-RZC-019 | RZC CAN E2E Transmit | D | SYS-032, TSR-022, TSR-023... | — | Yes |
| SWR-RZC-020 | RZC CAN E2E Receive | D | SYS-032, TSR-024, SSR-RZC-010 | — | Yes |
| SWR-RZC-021 | RZC Heartbeat Transmission | C | SYS-021, TSR-025, SSR-RZC-011 | — | Yes |
| SWR-RZC-022 | RZC Heartbeat Conditioning | C | SYS-021, TSR-026, SSR-RZC-012 | — | Yes |
| SWR-RZC-023 | RZC External Watchdog Feed | D | SYS-027, TSR-031, SSR-RZC-013 | — | Yes |
| SWR-RZC-024 | RZC CAN Bus Loss Detection | C | SYS-034, TSR-038, SSR-RZC-014 | — | Yes |
| SWR-RZC-025 | RZC Startup Self-Test Sequence | D | SYS-027, SYS-029 | — | Yes |
| SWR-RZC-026 | RZC CAN Message Reception | D | SYS-004, SYS-031 | — | Yes |
| SWR-RZC-027 | RZC CAN Message Transmission | D | SYS-005, SYS-031 | — | Yes |
| SWR-RZC-028 | RTOS Task Configuration and WCET Compliance | D | SYS-053, TSR-046, TSR-047... | — | Yes |
| SWR-RZC-029 | UDS Service Support | A | SYS-037, SYS-038, SYS-039 | — | Yes |
| SWR-RZC-030 | RZC DTC Persistence | A | SYS-041 | — | Yes |
| SWR-SC-001 | DCAN1 Silent Mode Configuration | C | SYS-025, TSR-024, SSR-SC-001 | — | Yes |
| SWR-SC-002 | CAN Message Acceptance Filtering | D | SYS-025, TSR-024, SSR-SC-002... | — | Yes |
| SWR-SC-003 | CAN E2E Receive Validation | D | SYS-032, TSR-022, TSR-024... | — | Yes |
| SWR-SC-004 | Per-ECU Heartbeat Timeout Counter | C | SYS-022, TSR-027, SSR-SC-003 | — | Yes |
| SWR-SC-005 | Fault LED Activation on Heartbeat Timeout | A | SYS-046, TSR-027, SSR-SC-004 | — | Yes |
| SWR-SC-006 | Heartbeat Timeout Confirmation and Kill Relay | D | SYS-022, SYS-024, TSR-028... | — | Yes |
| SWR-SC-007 | Torque-Current Lookup Table | C | SYS-023, TSR-041, SSR-SC-010 | — | Yes |
| SWR-SC-008 | Cross-Plausibility Debounce and Fault Detection | C | SYS-023, TSR-041, SSR-SC-011 | — | Yes |
| SWR-SC-009 | Cross-Plausibility Fault Reaction | C | SYS-023, TSR-042, SSR-SC-012 | — | Yes |
| SWR-SC-010 | Kill Relay GPIO Control | D | SYS-024, TSR-029, SSR-SC-006 | — | Yes |
| SWR-SC-011 | Kill Relay De-energize Trigger Logic | D | SYS-024, TSR-030, SSR-SC-007 | — | Yes |
| SWR-SC-012 | Kill Relay GPIO Readback Verification | D | SYS-024, TSR-029 | — | Yes |
| SWR-SC-013 | Fault LED Panel Management | A | SYS-046, TSR-045, SSR-SC-013 | — | Yes |
| SWR-SC-014 | Lockstep ESM Configuration | D | SYS-026, TSR-030 | — | Yes |
| SWR-SC-015 | Lockstep ESM Interrupt Handler | D | SYS-026 | — | Yes |
| SWR-SC-016 | SC Startup Self-Test -- Lockstep CPU | D | SYS-026, TSR-050, SSR-SC-016 | — | Yes |
| SWR-SC-017 | SC Startup Self-Test -- RAM PBIST | D | TSR-050, SSR-SC-016 | — | Yes |
| SWR-SC-018 | SC Startup Self-Test -- Flash CRC | C | TSR-050, SSR-SC-016 | — | Yes |
| SWR-SC-019 | SC Startup Self-Test Sequence | D | SYS-024, SYS-026, SYS-027... | — | Yes |
| SWR-SC-020 | SC Runtime Periodic Self-Test | D | TSR-051, SSR-SC-017 | — | Yes |
| SWR-SC-021 | SC Stack Canary Check | D | TSR-031 | — | Yes |
| SWR-SC-022 | SC External Watchdog Feed | D | SYS-027, TSR-031, SSR-SC-008 | — | Yes |
| SWR-SC-023 | SC CAN Bus Loss Detection | C | SYS-034, TSR-038, SSR-SC-009 | — | Yes |
| SWR-SC-024 | SC Backup Motor Cutoff via Kill Relay | D | TSR-049, SSR-SC-015 | — | Yes |
| SWR-SC-025 | SC Main Loop Structure and Timing | D | SYS-053, TSR-046, SSR-SC-014 | — | Yes |
| SWR-SC-026 | SC System Initialization | D | SYS-024, SYS-025, SYS-026... | — | Yes |
| SWR-SC-027 | Heartbeat OperatingMode Content Validation | C | SYS-022, TSR-027, SSR-SC-003 | — | Yes |
| SWR-SC-028 | Heartbeat FaultStatus Escalation | C | SYS-022, TSR-027, SSR-SC-003 | — | Yes |
| SWR-SC-029 | SC DCAN1 TX Configuration for Monitoring | C | — | — | Yes |
| SWR-SC-030 | SC Status Message Transmission | C | — | — | Yes |
| SWR-TCU-001 | SocketCAN Interface Initialization | A | SYS-031, SYS-037 | — | Yes |
| SWR-TCU-002 | UDS Diagnostic Session Control (0x10) | A | SYS-037, SYS-038 | — | Yes |
| SWR-TCU-003 | UDS Clear Diagnostic Information (0x14) | A | SYS-038 | — | Yes |
| SWR-TCU-004 | UDS Read DTC Information (0x19) | A | SYS-038, SYS-039 | — | Yes |
| SWR-TCU-005 | UDS Read Data By Identifier (0x22) | A | SYS-039, SYS-040 | — | Yes |
| SWR-TCU-006 | UDS Write Data By Identifier (0x2E) | A | SYS-039 | — | Yes |
| SWR-TCU-007 | UDS Security Access (0x27) | A | SYS-040 | — | Yes |
| SWR-TCU-008 | DTC Storage and Management | A | SYS-041 | — | Yes |
| SWR-TCU-009 | DTC Freeze-Frame Capture | A | SYS-041 | — | Yes |
| SWR-TCU-010 | OBD-II PID Support | A | SYS-038 | — | Yes |
| SWR-TCU-011 | UDS Transport Layer (ISO-TP) | A | SYS-037, SYS-038 | — | Yes |
| SWR-TCU-012 | Diagnostic Request Routing | A | SYS-037 | — | Yes |
| SWR-TCU-013 | Negative Response Code Handling | A | SYS-037 | — | Yes |
| SWR-TCU-014 | CAN Data Aggregation for DIDs | A | SYS-037, SYS-039 | — | Yes |
| SWR-TCU-015 | TCU Main Loop Execution | A | SYS-037 | — | Yes |

## Gap Analysis

### Broken Links

None found.

### Orphan Requirements

- `SWR-SC-029`
- `SWR-SC-030`

### Untested Requirements (SSR/SWR without test)

- `SWR-ALL-001`
- `SWR-ALL-002`
- `SWR-ALL-003`
- `SWR-ALL-004`
- `SWR-BSW-002`
- `SWR-BSW-003`
- `SWR-BSW-005`
- `SWR-BSW-006`
- `SWR-BSW-007`
- `SWR-BSW-008`
- `SWR-BSW-009`
- `SWR-BSW-012`
- `SWR-BSW-014`
- `SWR-BSW-021`

### ASIL Consistency Warnings

- HSR-RZC-004 (C) < TSR-005 (D)
- SWR-BCM-001 (A) < SYS-031 (D)
- SWR-BCM-002 (A) < SYS-029 (D)
- SWR-BCM-002 (A) < SYS-030 (D)
- SWR-BSW-018 (B) < TSR-004 (D)
- SWR-BSW-018 (B) < TSR-011 (D)
- SWR-BSW-018 (B) < TSR-020 (C)
- SWR-BSW-019 (B) < TSR-002 (D)
- SWR-BSW-020 (B) < TSR-037 (D)
- SWR-CVC-027 (A) < SSR-CVC-022 (B)
- SWR-CVC-027 (A) < TSR-043 (B)
- SWR-FZC-017 (A) < SSR-FZC-022 (B)
- SWR-FZC-017 (A) < TSR-044 (B)
- SWR-FZC-032 (A) < SYS-010 (D)
- SWR-FZC-032 (A) < SYS-014 (D)
- SWR-ICU-001 (A) < SYS-031 (D)
- SWR-ICU-006 (A) < SYS-046 (C)
- SWR-ICU-007 (A) < SYS-029 (D)
- SWR-ICU-009 (A) < SYS-021 (C)
- SWR-ICU-009 (A) < SYS-046 (C)
- SWR-SC-001 (C) < SSR-SC-001 (D)
- SWR-SC-001 (C) < TSR-024 (D)
- SWR-SC-005 (A) < SSR-SC-004 (C)
- SWR-SC-005 (A) < SYS-046 (C)
- SWR-SC-005 (A) < TSR-027 (C)
- SWR-SC-013 (A) < SSR-SC-013 (B)
- SWR-SC-013 (A) < SYS-046 (C)
- SWR-SC-013 (A) < TSR-045 (B)
- SWR-TCU-001 (A) < SYS-031 (D)
- TSR-026 (C) < FSR-017 (D)
- TSR-050 (C) < FSR-016 (D)
- TSR-050 (C) < FSR-017 (D)
- TSR-051 (C) < FSR-017 (D)

---

*Generated: 2026-03-10 21:37 UTC*
