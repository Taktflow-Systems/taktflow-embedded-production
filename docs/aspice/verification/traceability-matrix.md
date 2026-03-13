---
document_id: TM
title: "Bidirectional Traceability Matrix"
aspice_process: "SWE.4, SWE.5, SWE.6"
iso_reference: "ISO 26262 Part 6"
---

<!-- HITL-LOCK START:tm-header -->
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
<!-- HITL-LOCK END:tm-header -->


# Bidirectional Traceability Matrix

> **Auto-generated** by `scripts/gen-traceability.sh`
> Do not edit manually — regenerate after requirement or test changes.

**Standard references:**
- ISO 26262:2018 Part 6 — Software level product development
- Automotive SPICE 4.0 SWE.4 — Software unit verification
- Automotive SPICE 4.0 SWE.5 — Software component verification & integration
- Automotive SPICE 4.0 SWE.6 — Software verification

## Coverage Summary

| Metric | Count |
|--------|-------|
| Total requirements | 193 |
| Covered (has test) | 187 |
| Partial (code only) | 0 |
| Uncovered | 6 |
| **Coverage** | **96%** |

## Traceability Matrix

| Requirement ID | Requirement Source | Source Files | Unit Tests | Integration Tests | SIL Scenarios | Status |
|---|---|---|---|---|---|---|
| SWR-BCM-001 | SWR-BCM.md | bcm/src/bcm_main.c,bcm/src/Swc_BcmCan.c | bcm/test/test_Swc_BcmCan.c | — | — | COVERED |
| SWR-BCM-002 | SWR-BCM.md | bcm/src/Swc_BcmCan.c | bcm/test/test_Swc_BcmCan.c | — | — | COVERED |
| SWR-BCM-003 | SWR-BCM.md | bcm/src/Swc_Lights.c | bcm/test/test_Swc_Lights.c | — | — | COVERED |
| SWR-BCM-004 | SWR-BCM.md | bcm/src/Swc_Lights.c | bcm/test/test_Swc_Lights.c | — | — | COVERED |
| SWR-BCM-005 | SWR-BCM.md | bcm/src/Swc_Lights.c | bcm/test/test_Swc_Lights.c | — | — | COVERED |
| SWR-BCM-006 | SWR-BCM.md | bcm/src/Swc_Indicators.c | bcm/test/test_Swc_Indicators.c | — | — | COVERED |
| SWR-BCM-007 | SWR-BCM.md | bcm/src/Swc_Indicators.c | bcm/test/test_Swc_Indicators.c | — | — | COVERED |
| SWR-BCM-008 | SWR-BCM.md | bcm/src/Swc_Indicators.c | bcm/test/test_Swc_Indicators.c | — | — | COVERED |
| SWR-BCM-009 | SWR-BCM.md | bcm/src/Swc_DoorLock.c | bcm/test/test_Swc_DoorLock.c | — | — | COVERED |
| SWR-BCM-010 | SWR-BCM.md | bcm/src/Swc_BcmCan.c | bcm/test/test_Swc_BcmCan.c | — | — | COVERED |
| SWR-BCM-011 | SWR-BCM.md | bcm/src/Swc_BcmCan.c | bcm/test/test_Swc_BcmCan.c | — | — | COVERED |
| SWR-BCM-012 | SWR-BCM.md | bcm/src/bcm_main.c,bcm/src/Swc_BcmMain.c | bcm/test/test_Swc_BcmMain.c | — | — | COVERED |
| SWR-BSW-001 | SWR-BSW.md | mcal/Can.c,mcal/Can.h,mcal/posix/Can_Posix.c,mcal/posix... | test/test_Can.c,test/test_Can_Posix.c | — | sil_001_normal_startup.yaml,sil_014_long_duration.yaml,... | COVERED |
| SWR-BSW-002 | SWR-BSW.md | mcal/Can.c,mcal/Can.h,mcal/posix/Can_Posix.c,mcal/posix... | test/test_Can.c,test/test_Can_Posix.c | — | — | COVERED |
| SWR-BSW-003 | SWR-BSW.md | mcal/Can.c,mcal/Can.h,mcal/posix/Can_Posix.c,mcal/posix... | test/test_Can.c,test/test_Can_Posix.c | — | — | COVERED |
| SWR-BSW-004 | SWR-BSW.md | mcal/Can.c,mcal/Can.h,mcal/posix/Can_Posix.c | test/test_Can.c,test/test_Can_Posix.c | test_int_can_busoff.c | — | COVERED |
| SWR-BSW-005 | SWR-BSW.md | mcal/Can.c,mcal/Can.h,mcal/posix/Can_Posix.c | test/test_Can.c,test/test_Can_Posix.c | — | — | COVERED |
| SWR-BSW-006 | SWR-BSW.md | mcal/posix/Spi_Posix.c,mcal/Spi.c,mcal/Spi.h | test/test_Spi.c | — | — | COVERED |
| SWR-BSW-007 | SWR-BSW.md | mcal/Adc.c,mcal/Adc.h,mcal/posix/Adc_Posix.c | test/test_Adc.c | — | — | COVERED |
| SWR-BSW-008 | SWR-BSW.md | mcal/posix/Pwm_Posix.c,mcal/Pwm.c,mcal/Pwm.h | test/test_Pwm.c | — | — | COVERED |
| SWR-BSW-009 | SWR-BSW.md | mcal/Dio.c,mcal/Dio.h,mcal/posix/Dio_Posix.c | test/test_Dio.c | — | — | COVERED |
| SWR-BSW-010 | SWR-BSW.md | mcal/Gpt.c,mcal/Gpt.h,mcal/posix/Gpt_Posix.c,mcal/posix... | test/test_Gpt.c,test/test_Uart.c | — | sil_009_e2e_corruption.yaml | COVERED |
| SWR-BSW-011 | SWR-BSW.md | ecual/CanIf.c,ecual/CanIf.h | test/test_CanIf.c | test_int_can_busoff.c,test_int_can_matrix.c,test_int_de... | sil_009_e2e_corruption.yaml | COVERED |
| SWR-BSW-012 | SWR-BSW.md | ecual/CanIf.c,ecual/CanIf.h | test/test_CanIf.c | — | — | COVERED |
| SWR-BSW-013 | SWR-BSW.md | ecual/PduR.c,ecual/PduR.h | test/test_PduR.c | test_int_can_busoff.c,test_int_dem_to_dcm.c,test_int_e2... | — | COVERED |
| SWR-BSW-014 | SWR-BSW.md | ecual/IoHwAb.c,ecual/IoHwAb.h | test/test_IoHwAb.c | — | — | COVERED |
| SWR-BSW-015 | SWR-BSW.md | services/Com.c,services/Com.h | test/test_Com.c | test_int_can_busoff.c,test_int_can_matrix.c,test_int_e2... | sil_005_watchdog_timeout_cvc.yaml | COVERED |
| SWR-BSW-016 | SWR-BSW.md | services/Com.c,services/Com.h | test/test_Com.c | test_int_can_matrix.c,test_int_e2e_chain.c,test_int_sig... | sil_007_overcurrent_motor.yaml | COVERED |
| SWR-BSW-017 | SWR-BSW.md | services/Dcm.c,services/Dcm.h,services/Dem.c,services/D... | test/test_Dcm.c,test/test_Dem.c | test_int_dem_to_dcm.c | sil_006_battery_undervoltage.yaml | COVERED |
| SWR-BSW-018 | SWR-BSW.md | services/Dem.c,services/Dem.h | test/test_Dem.c | test_int_dem_to_dcm.c,test_int_heartbeat_loss.c,test_in... | — | COVERED |
| SWR-BSW-019 | SWR-BSW.md | services/WdgM.c,services/WdgM.h | test/test_WdgM.c | test_int_heartbeat_loss.c,test_int_safe_state.c,test_in... | sil_003_emergency_stop.yaml | COVERED |
| SWR-BSW-020 | SWR-BSW.md | services/WdgM.c,services/WdgM.h | test/test_WdgM.c | test_int_heartbeat_loss.c,test_int_safe_state.c,test_in... | sil_001_normal_startup.yaml,sil_013_recovery_from_safe.... | COVERED |
| SWR-BSW-021 | SWR-BSW.md | — | test/test_WdgM.c | — | — | COVERED |
| SWR-BSW-022 | SWR-BSW.md | services/BswM.c,services/BswM.h | test/test_BswM.c | test_int_bswm_mode.c,test_int_heartbeat_loss.c,test_int... | — | COVERED |
| SWR-BSW-023 | SWR-BSW.md | services/E2E.c,services/E2E.h | test/test_E2E.c | test_int_can_matrix.c,test_int_e2e_chain.c,test_int_e2e... | — | COVERED |
| SWR-BSW-024 | SWR-BSW.md | services/E2E.c,services/E2E.h | test/test_E2E.c | test_int_e2e_chain.c,test_int_e2e_faults.c | — | COVERED |
| SWR-BSW-025 | SWR-BSW.md | services/E2E.c,services/E2E.h | test/test_E2E.c | test_int_e2e_chain.c,test_int_e2e_faults.c | — | COVERED |
| SWR-BSW-026 | SWR-BSW.md | rte/Rte.c,rte/Rte.h | test/test_Rte.c | test_int_heartbeat_loss.c,test_int_overcurrent_chain.c,... | — | COVERED |
| SWR-BSW-027 | SWR-BSW.md | rte/Rte.c,rte/Rte.h | test/test_Rte.c | test_int_overcurrent_chain.c,test_int_safe_state.c | — | COVERED |
| SWR-CVC-001 | SWR-CVC.md | cvc/src/Swc_Pedal.c | cvc/test/test_Swc_Pedal.c | — | sil_002_pedal_ramp.yaml | COVERED |
| SWR-CVC-002 | SWR-CVC.md | — | cvc/test/test_Swc_Pedal.c | — | — | COVERED |
| SWR-CVC-003 | SWR-CVC.md | — | cvc/test/test_Swc_Pedal.c | — | sil_003_emergency_stop.yaml | COVERED |
| SWR-CVC-004 | SWR-CVC.md | — | cvc/test/test_Swc_Pedal.c | — | — | COVERED |
| SWR-CVC-005 | SWR-CVC.md | — | cvc/test/test_Swc_Pedal.c | — | — | COVERED |
| SWR-CVC-006 | SWR-CVC.md | — | cvc/test/test_Swc_Pedal.c | — | sil_015_power_cycle.yaml | COVERED |
| SWR-CVC-007 | SWR-CVC.md | — | cvc/test/test_Swc_Pedal.c | — | — | COVERED |
| SWR-CVC-008 | SWR-CVC.md | cvc/src/Swc_Pedal.c | cvc/test/test_Swc_Pedal.c | — | — | COVERED |
| SWR-CVC-009 | SWR-CVC.md | cvc/src/Swc_VehicleState.c | cvc/test/test_Swc_VehicleState.c | — | — | COVERED |
| SWR-CVC-010 | SWR-CVC.md | — | cvc/test/test_Swc_VehicleState.c | — | — | COVERED |
| SWR-CVC-011 | SWR-CVC.md | — | cvc/test/test_Swc_VehicleState.c | — | — | COVERED |
| SWR-CVC-012 | SWR-CVC.md | — | cvc/test/test_Swc_VehicleState.c | — | — | COVERED |
| SWR-CVC-013 | SWR-CVC.md | cvc/src/Swc_VehicleState.c | cvc/test/test_Swc_VehicleState.c | — | — | COVERED |
| SWR-CVC-014 | SWR-CVC.md | cvc/src/Swc_CvcCom.c | cvc/test/test_Swc_CvcCom.c | — | — | COVERED |
| SWR-CVC-015 | SWR-CVC.md | cvc/src/Swc_CvcCom.c | cvc/test/test_Swc_CvcCom.c | — | — | COVERED |
| SWR-CVC-016 | SWR-CVC.md | cvc/src/Swc_CvcCom.c | cvc/test/test_Swc_CvcCom.c | — | — | COVERED |
| SWR-CVC-017 | SWR-CVC.md | cvc/src/Swc_CvcCom.c | cvc/test/test_Swc_CvcCom.c | — | — | COVERED |
| SWR-CVC-018 | SWR-CVC.md | cvc/src/Swc_EStop.c | cvc/test/test_Swc_EStop.c | — | — | COVERED |
| SWR-CVC-019 | SWR-CVC.md | — | cvc/test/test_Swc_EStop.c | — | — | COVERED |
| SWR-CVC-020 | SWR-CVC.md | cvc/src/Swc_EStop.c | cvc/test/test_Swc_EStop.c | — | — | COVERED |
| SWR-CVC-021 | SWR-CVC.md | cvc/src/Swc_Heartbeat.c | cvc/test/test_Swc_Heartbeat.c | — | — | COVERED |
| SWR-CVC-022 | SWR-CVC.md | cvc/src/Swc_Heartbeat.c | cvc/test/test_Swc_Heartbeat.c | — | — | COVERED |
| SWR-CVC-023 | SWR-CVC.md | cvc/src/Swc_Watchdog.c | cvc/test/test_Swc_Watchdog.c | — | — | COVERED |
| SWR-CVC-024 | SWR-CVC.md | cvc/src/Swc_CanMonitor.c | cvc/test/test_Swc_CanMonitor.c | — | — | COVERED |
| SWR-CVC-025 | SWR-CVC.md | cvc/src/Swc_CanMonitor.c | cvc/test/test_Swc_CanMonitor.c | — | — | COVERED |
| SWR-CVC-026 | SWR-CVC.md | cvc/src/Ssd1306.c | cvc/test/test_Ssd1306.c | — | — | COVERED |
| SWR-CVC-027 | SWR-CVC.md | cvc/src/Swc_Dashboard.c | cvc/test/test_Swc_Dashboard.c | — | — | COVERED |
| SWR-CVC-028 | SWR-CVC.md | cvc/src/Swc_Dashboard.c | cvc/test/test_Swc_Dashboard.c | — | — | COVERED |
| SWR-CVC-029 | SWR-CVC.md | cvc/src/main.c,cvc/src/Swc_SelfTest.c | cvc/test/test_Swc_SelfTest.c | — | — | COVERED |
| SWR-CVC-030 | SWR-CVC.md | cvc/src/Swc_Nvm.c | cvc/test/test_Swc_Nvm.c | — | — | COVERED |
| SWR-CVC-031 | SWR-CVC.md | cvc/src/Swc_Nvm.c | cvc/test/test_Swc_Nvm.c | — | — | COVERED |
| SWR-CVC-032 | SWR-CVC.md | cvc/src/Swc_Scheduler.c | cvc/test/test_Swc_Scheduler.c | — | — | COVERED |
| SWR-CVC-033 | SWR-CVC.md | cvc/src/Swc_CvcDcm.c | cvc/test/test_Swc_CvcDcm.c | — | — | COVERED |
| SWR-CVC-034 | SWR-CVC.md | cvc/src/Swc_CvcDcm.c | cvc/test/test_Swc_CvcDcm.c | — | — | COVERED |
| SWR-CVC-035 | SWR-CVC.md | cvc/src/main.c,cvc/src/Swc_CvcDcm.c | cvc/test/test_Swc_CvcDcm.c | — | — | COVERED |
| SWR-FZC-001 | SWR-FZC.md | fzc/src/Swc_Steering.c | fzc/test/test_Swc_Steering.c | — | — | COVERED |
| SWR-FZC-002 | SWR-FZC.md | — | fzc/test/test_Swc_Steering.c | — | sil_008_sensor_disagreement.yaml | COVERED |
| SWR-FZC-003 | SWR-FZC.md | — | fzc/test/test_Swc_Steering.c | — | sil_011_steering_sensor_failure.yaml | COVERED |
| SWR-FZC-004 | SWR-FZC.md | — | fzc/test/test_Swc_Steering.c | — | — | COVERED |
| SWR-FZC-005 | SWR-FZC.md | — | fzc/test/test_Swc_Steering.c | — | sil_004_can_busoff_fzc.yaml | COVERED |
| SWR-FZC-006 | SWR-FZC.md | — | fzc/test/test_Swc_Steering.c | — | — | COVERED |
| SWR-FZC-007 | SWR-FZC.md | — | fzc/test/test_Swc_Steering.c | — | — | COVERED |
| SWR-FZC-008 | SWR-FZC.md | fzc/src/Swc_Steering.c | fzc/test/test_Swc_Steering.c | — | — | COVERED |
| SWR-FZC-009 | SWR-FZC.md | fzc/src/Swc_Brake.c | fzc/test/test_Swc_Brake.c | — | — | COVERED |
| SWR-FZC-010 | SWR-FZC.md | — | fzc/test/test_Swc_Brake.c | — | — | COVERED |
| SWR-FZC-011 | SWR-FZC.md | — | fzc/test/test_Swc_Brake.c | — | — | COVERED |
| SWR-FZC-012 | SWR-FZC.md | fzc/src/Swc_Brake.c | fzc/test/test_Swc_Brake.c | — | — | COVERED |
| SWR-FZC-013 | SWR-FZC.md | fzc/src/Swc_Lidar.c | fzc/test/test_Swc_Lidar.c | — | — | COVERED |
| SWR-FZC-014 | SWR-FZC.md | — | fzc/test/test_Swc_Lidar.c | — | — | COVERED |
| SWR-FZC-015 | SWR-FZC.md | — | fzc/test/test_Swc_Lidar.c | — | — | COVERED |
| SWR-FZC-016 | SWR-FZC.md | fzc/src/Swc_Lidar.c | fzc/test/test_Swc_Lidar.c | — | — | COVERED |
| SWR-FZC-017 | SWR-FZC.md | fzc/src/Swc_Buzzer.c | fzc/test/test_Swc_Buzzer.c | — | — | COVERED |
| SWR-FZC-018 | SWR-FZC.md | fzc/src/Swc_Buzzer.c | fzc/test/test_Swc_Buzzer.c | — | — | COVERED |
| SWR-FZC-019 | SWR-FZC.md | fzc/src/Swc_FzcCom.c | fzc/test/test_Swc_FzcCom.c | — | — | COVERED |
| SWR-FZC-020 | SWR-FZC.md | fzc/src/Swc_FzcCom.c | fzc/test/test_Swc_FzcCom.c | — | — | COVERED |
| SWR-FZC-021 | SWR-FZC.md | fzc/src/Swc_Heartbeat.c | fzc/test/test_Swc_Heartbeat.c | — | — | COVERED |
| SWR-FZC-022 | SWR-FZC.md | fzc/src/Swc_Heartbeat.c | fzc/test/test_Swc_Heartbeat.c | — | — | COVERED |
| SWR-FZC-023 | SWR-FZC.md | fzc/src/Swc_FzcSafety.c | fzc/test/test_Swc_FzcSafety.c | — | — | COVERED |
| SWR-FZC-024 | SWR-FZC.md | fzc/src/Swc_FzcCanMonitor.c | fzc/test/test_Swc_FzcCanMonitor.c | — | — | COVERED |
| SWR-FZC-025 | SWR-FZC.md | fzc/src/main.c,fzc/src/Swc_FzcSafety.c | fzc/test/test_Swc_FzcSafety.c | — | — | COVERED |
| SWR-FZC-026 | SWR-FZC.md | fzc/src/Swc_FzcCom.c | fzc/test/test_Swc_FzcCom.c | — | — | COVERED |
| SWR-FZC-027 | SWR-FZC.md | fzc/src/Swc_FzcCom.c | fzc/test/test_Swc_FzcCom.c | — | — | COVERED |
| SWR-FZC-028 | SWR-FZC.md | fzc/src/Swc_Steering.c | fzc/test/test_Swc_Steering.c | — | — | COVERED |
| SWR-FZC-029 | SWR-FZC.md | fzc/src/Swc_FzcScheduler.c | fzc/test/test_Swc_FzcScheduler.c | — | — | COVERED |
| SWR-FZC-030 | SWR-FZC.md | fzc/src/Swc_FzcDcm.c | fzc/test/test_Swc_FzcDcm.c | — | — | COVERED |
| SWR-FZC-031 | SWR-FZC.md | fzc/src/Swc_FzcNvm.c | fzc/test/test_Swc_FzcNvm.c | — | — | COVERED |
| SWR-FZC-032 | SWR-FZC.md | fzc/src/main.c,fzc/src/Swc_FzcNvm.c | fzc/test/test_Swc_FzcNvm.c | — | — | COVERED |
| SWR-GW-001 | SWR-GW.md | — | — | — | — | **UNCOVERED** |
| SWR-GW-002 | SWR-GW.md | — | — | — | — | **UNCOVERED** |
| SWR-GW-003 | SWR-GW.md | — | — | — | — | **UNCOVERED** |
| SWR-GW-004 | SWR-GW.md | — | — | — | — | **UNCOVERED** |
| SWR-GW-005 | SWR-GW.md | — | — | — | — | **UNCOVERED** |
| SWR-GW-006 | SWR-GW.md | — | — | — | — | **UNCOVERED** |
| SWR-ICU-001 | SWR-ICU.md | icu/src/icu_main.c | icu/test/test_Swc_IcuCan.c | — | — | COVERED |
| SWR-ICU-002 | SWR-ICU.md | icu/src/Swc_Dashboard.c | icu/test/test_Swc_Dashboard.c | — | — | COVERED |
| SWR-ICU-003 | SWR-ICU.md | icu/src/Swc_Dashboard.c | icu/test/test_Swc_Dashboard.c | — | — | COVERED |
| SWR-ICU-004 | SWR-ICU.md | icu/src/Swc_Dashboard.c | icu/test/test_Swc_Dashboard.c | — | — | COVERED |
| SWR-ICU-005 | SWR-ICU.md | icu/src/Swc_Dashboard.c | icu/test/test_Swc_Dashboard.c | — | — | COVERED |
| SWR-ICU-006 | SWR-ICU.md | icu/src/Swc_Dashboard.c | icu/test/test_Swc_Dashboard.c | — | — | COVERED |
| SWR-ICU-007 | SWR-ICU.md | icu/src/Swc_Dashboard.c | icu/test/test_Swc_Dashboard.c | — | — | COVERED |
| SWR-ICU-008 | SWR-ICU.md | icu/src/Swc_DtcDisplay.c | icu/test/test_Swc_DtcDisplay.c | — | — | COVERED |
| SWR-ICU-009 | SWR-ICU.md | icu/src/Swc_Dashboard.c | icu/test/test_Swc_Dashboard.c | — | — | COVERED |
| SWR-ICU-010 | SWR-ICU.md | icu/src/icu_main.c | icu/test/test_Swc_IcuMain.c | — | — | COVERED |
| SWR-RZC-001 | SWR-RZC.md | rzc/src/Swc_Motor.c | rzc/test/test_Swc_Motor.c | — | sil_002_pedal_ramp.yaml | COVERED |
| SWR-RZC-002 | SWR-RZC.md | — | rzc/test/test_Swc_Motor.c | — | sil_007_overcurrent_motor.yaml | COVERED |
| SWR-RZC-003 | SWR-RZC.md | — | rzc/test/test_Swc_Motor.c | — | sil_010_overtemp_motor.yaml | COVERED |
| SWR-RZC-004 | SWR-RZC.md | rzc/src/Swc_Motor.c | rzc/test/test_Swc_Motor.c | — | sil_006_battery_undervoltage.yaml | COVERED |
| SWR-RZC-005 | SWR-RZC.md | rzc/src/Swc_CurrentMonitor.c | rzc/test/test_Swc_CurrentMonitor.c | — | — | COVERED |
| SWR-RZC-006 | SWR-RZC.md | rzc/src/Swc_CurrentMonitor.c | rzc/test/test_Swc_CurrentMonitor.c | — | — | COVERED |
| SWR-RZC-007 | SWR-RZC.md | rzc/src/Swc_CurrentMonitor.c | rzc/test/test_Swc_CurrentMonitor.c | — | — | COVERED |
| SWR-RZC-008 | SWR-RZC.md | rzc/src/Swc_CurrentMonitor.c | rzc/test/test_Swc_CurrentMonitor.c | — | — | COVERED |
| SWR-RZC-009 | SWR-RZC.md | rzc/src/Swc_TempMonitor.c | rzc/test/test_Swc_TempMonitor.c | — | — | COVERED |
| SWR-RZC-010 | SWR-RZC.md | rzc/src/Swc_TempMonitor.c | rzc/test/test_Swc_TempMonitor.c | — | — | COVERED |
| SWR-RZC-011 | SWR-RZC.md | rzc/src/Swc_TempMonitor.c | rzc/test/test_Swc_TempMonitor.c | — | — | COVERED |
| SWR-RZC-012 | SWR-RZC.md | rzc/src/Swc_Encoder.c | rzc/test/test_Swc_Encoder.c | — | — | COVERED |
| SWR-RZC-013 | SWR-RZC.md | rzc/src/Swc_Encoder.c | rzc/test/test_Swc_Encoder.c | — | — | COVERED |
| SWR-RZC-014 | SWR-RZC.md | rzc/src/Swc_Encoder.c | rzc/test/test_Swc_Encoder.c | — | — | COVERED |
| SWR-RZC-015 | SWR-RZC.md | rzc/src/Swc_Motor.c | rzc/test/test_Swc_Motor.c | — | — | COVERED |
| SWR-RZC-016 | SWR-RZC.md | rzc/src/Swc_Motor.c | rzc/test/test_Swc_Motor.c | — | — | COVERED |
| SWR-RZC-017 | SWR-RZC.md | rzc/src/Swc_Battery.c | rzc/test/test_Swc_Battery.c | — | — | COVERED |
| SWR-RZC-018 | SWR-RZC.md | rzc/src/Swc_Battery.c | rzc/test/test_Swc_Battery.c | — | — | COVERED |
| SWR-RZC-019 | SWR-RZC.md | rzc/src/Swc_RzcCom.c | rzc/test/test_Swc_RzcCom.c | — | — | COVERED |
| SWR-RZC-020 | SWR-RZC.md | rzc/src/Swc_RzcCom.c | rzc/test/test_Swc_RzcCom.c | — | — | COVERED |
| SWR-RZC-021 | SWR-RZC.md | rzc/src/Swc_Heartbeat.c | rzc/test/test_Swc_Heartbeat.c | — | — | COVERED |
| SWR-RZC-022 | SWR-RZC.md | rzc/src/Swc_Heartbeat.c | rzc/test/test_Swc_Heartbeat.c | — | — | COVERED |
| SWR-RZC-023 | SWR-RZC.md | rzc/src/Swc_RzcSafety.c | rzc/test/test_Swc_RzcSafety.c | — | — | COVERED |
| SWR-RZC-024 | SWR-RZC.md | rzc/src/Swc_RzcSafety.c | rzc/test/test_Swc_RzcSafety.c | — | — | COVERED |
| SWR-RZC-025 | SWR-RZC.md | rzc/src/main.c,rzc/src/Swc_RzcSelfTest.c | rzc/test/test_Swc_RzcSelfTest.c | — | — | COVERED |
| SWR-RZC-026 | SWR-RZC.md | rzc/src/Swc_RzcCom.c | rzc/test/test_Swc_RzcCom.c | — | — | COVERED |
| SWR-RZC-027 | SWR-RZC.md | rzc/src/Swc_RzcCom.c | rzc/test/test_Swc_RzcCom.c | — | — | COVERED |
| SWR-RZC-028 | SWR-RZC.md | rzc/src/Swc_RzcScheduler.c | rzc/test/test_Swc_RzcScheduler.c | — | — | COVERED |
| SWR-RZC-029 | SWR-RZC.md | rzc/src/Swc_RzcDcm.c | rzc/test/test_Swc_RzcDcm.c | — | — | COVERED |
| SWR-RZC-030 | SWR-RZC.md | rzc/src/main.c,rzc/src/Swc_RzcNvm.c | rzc/test/test_Swc_RzcNvm.c | — | — | COVERED |
| SWR-SC-001 | SWR-SC.md | sc/src/sc_can.c | sc/test/test_sc_can.c | — | sil_004_can_busoff_fzc.yaml,sil_005_watchdog_timeout_cv... | COVERED |
| SWR-SC-002 | SWR-SC.md | sc/src/sc_can.c | sc/test/test_sc_can.c | — | sil_012_multiple_faults.yaml | COVERED |
| SWR-SC-003 | SWR-SC.md | sc/src/sc_e2e.c | sc/test/test_sc_e2e.c | — | — | COVERED |
| SWR-SC-004 | SWR-SC.md | sc/src/sc_heartbeat.c | sc/test/test_sc_heartbeat.c | — | — | COVERED |
| SWR-SC-005 | SWR-SC.md | sc/src/sc_heartbeat.c | sc/test/test_sc_heartbeat.c | — | — | COVERED |
| SWR-SC-006 | SWR-SC.md | sc/src/sc_heartbeat.c | sc/test/test_sc_heartbeat.c | — | — | COVERED |
| SWR-SC-007 | SWR-SC.md | sc/src/sc_plausibility.c | sc/test/test_sc_plausibility.c | — | — | COVERED |
| SWR-SC-008 | SWR-SC.md | sc/src/sc_plausibility.c | sc/test/test_sc_plausibility.c | — | — | COVERED |
| SWR-SC-009 | SWR-SC.md | sc/src/sc_plausibility.c | sc/test/test_sc_plausibility.c | — | — | COVERED |
| SWR-SC-010 | SWR-SC.md | sc/src/sc_relay.c | sc/test/test_sc_relay.c | — | — | COVERED |
| SWR-SC-011 | SWR-SC.md | sc/src/sc_relay.c | sc/test/test_sc_relay.c | — | — | COVERED |
| SWR-SC-012 | SWR-SC.md | sc/src/sc_relay.c | sc/test/test_sc_relay.c | — | — | COVERED |
| SWR-SC-013 | SWR-SC.md | sc/src/sc_led.c | sc/test/test_sc_led.c | — | — | COVERED |
| SWR-SC-014 | SWR-SC.md | sc/src/sc_esm.c | sc/test/test_sc_esm.c | — | — | COVERED |
| SWR-SC-015 | SWR-SC.md | sc/src/sc_esm.c | sc/test/test_sc_esm.c | — | — | COVERED |
| SWR-SC-016 | SWR-SC.md | sc/src/sc_selftest.c | sc/test/test_sc_selftest.c | — | — | COVERED |
| SWR-SC-017 | SWR-SC.md | sc/src/sc_selftest.c | sc/test/test_sc_selftest.c | — | — | COVERED |
| SWR-SC-018 | SWR-SC.md | sc/src/sc_selftest.c | sc/test/test_sc_selftest.c | — | — | COVERED |
| SWR-SC-019 | SWR-SC.md | sc/src/sc_selftest.c | sc/test/test_sc_selftest.c | — | — | COVERED |
| SWR-SC-020 | SWR-SC.md | sc/src/sc_selftest.c | sc/test/test_sc_selftest.c | — | — | COVERED |
| SWR-SC-021 | SWR-SC.md | sc/src/sc_selftest.c | sc/test/test_sc_selftest.c | — | — | COVERED |
| SWR-SC-022 | SWR-SC.md | sc/src/sc_watchdog.c | sc/test/test_sc_watchdog.c | — | — | COVERED |
| SWR-SC-023 | SWR-SC.md | sc/src/sc_can.c | sc/test/test_sc_can.c | — | — | COVERED |
| SWR-SC-024 | SWR-SC.md | sc/src/sc_plausibility.c | sc/test/test_sc_plausibility.c | — | — | COVERED |
| SWR-SC-025 | SWR-SC.md | sc/src/sc_main.c | sc/test/test_sc_main.c | — | — | COVERED |
| SWR-SC-026 | SWR-SC.md | sc/src/sc_main.c | sc/test/test_sc_main.c | — | — | COVERED |
| SWR-TCU-001 | SWR-TCU.md | tcu/src/tcu_main.c | tcu/test/test_Swc_TcuCan.c | — | — | COVERED |
| SWR-TCU-002 | SWR-TCU.md | tcu/src/Swc_UdsServer.c | tcu/test/test_Swc_UdsServer.c | — | — | COVERED |
| SWR-TCU-003 | SWR-TCU.md | tcu/src/Swc_UdsServer.c | tcu/test/test_Swc_UdsServer.c | — | — | COVERED |
| SWR-TCU-004 | SWR-TCU.md | tcu/src/Swc_UdsServer.c | tcu/test/test_Swc_UdsServer.c | — | — | COVERED |
| SWR-TCU-005 | SWR-TCU.md | tcu/src/Swc_UdsServer.c | tcu/test/test_Swc_UdsServer.c | — | — | COVERED |
| SWR-TCU-006 | SWR-TCU.md | tcu/src/Swc_UdsServer.c | tcu/test/test_Swc_UdsServer.c | — | — | COVERED |
| SWR-TCU-007 | SWR-TCU.md | tcu/src/Swc_UdsServer.c | tcu/test/test_Swc_UdsServer.c | — | — | COVERED |
| SWR-TCU-008 | SWR-TCU.md | tcu/src/Swc_DtcStore.c | tcu/test/test_Swc_DtcStore.c | — | — | COVERED |
| SWR-TCU-009 | SWR-TCU.md | tcu/src/Swc_DtcStore.c | tcu/test/test_Swc_DtcStore.c | — | — | COVERED |
| SWR-TCU-010 | SWR-TCU.md | tcu/src/Swc_Obd2Pids.c | tcu/test/test_Swc_Obd2Pids.c | — | — | COVERED |
| SWR-TCU-011 | SWR-TCU.md | tcu/src/Swc_UdsServer.c | tcu/test/test_Swc_UdsServer.c | — | — | COVERED |
| SWR-TCU-012 | SWR-TCU.md | tcu/src/Swc_UdsServer.c | tcu/test/test_Swc_UdsServer.c | — | — | COVERED |
| SWR-TCU-013 | SWR-TCU.md | tcu/src/Swc_UdsServer.c | tcu/test/test_Swc_UdsServer.c | — | — | COVERED |
| SWR-TCU-014 | SWR-TCU.md | tcu/src/Swc_DataAggregator.c | tcu/test/test_Swc_DataAggregator.c | — | — | COVERED |
| SWR-TCU-015 | SWR-TCU.md | tcu/src/tcu_main.c | tcu/test/test_Swc_TcuMain.c | — | — | COVERED |

## Summary

- **Covered**: 187 / 193 requirements have test coverage
- **Partial**: 0 requirements have source code but no test
- **Uncovered**: 6 requirements have no test and no source reference
- **Coverage**: 96%

### Uncovered Requirements

- `SWR-GW-001`
- `SWR-GW-002`
- `SWR-GW-003`
- `SWR-GW-004`
- `SWR-GW-005`
- `SWR-GW-006`

---

*Generated: 2026-02-25 08:08 UTC*
