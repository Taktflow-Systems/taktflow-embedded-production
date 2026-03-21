# Traceability Matrix

**Generated:** 2026-03-21 06:44
**Requirements found:** 353
**With implementation:** 306 (86%)
**With test:** 353 (100%)
**Full trace (impl+test):** 306 (86%)

| Requirement | Implementation | Config | Test | Status |
|-------------|---------------|--------|------|--------|
| SG-001 | — | firmware/ecu/bcm/cfg/Com_Cfg_Bcm.c, firmware/ecu/cvc/cfg/Com | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| SG-002 | — | firmware/ecu/bcm/cfg/Com_Cfg_Bcm.c, firmware/ecu/cvc/cfg/Com | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| SG-003 | — | firmware/ecu/bcm/cfg/Com_Cfg_Bcm.c, firmware/ecu/cvc/cfg/Com | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| SG-004 | — | firmware/ecu/bcm/cfg/Com_Cfg_Bcm.c, firmware/ecu/cvc/cfg/Com | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| SG-006 | — | firmware/ecu/bcm/cfg/Com_Cfg_Bcm.c, firmware/ecu/cvc/cfg/Com | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| SG-007 | — | firmware/ecu/bcm/cfg/Com_Cfg_Bcm.c, firmware/ecu/cvc/cfg/Com | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| SG-008 | — | firmware/ecu/bcm/cfg/Com_Cfg_Bcm.c, firmware/ecu/cvc/cfg/Com | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| SSR-BCM-001 | firmware/ecu/bcm/include/Swc_BcmCan.h, firmware/ecu/bcm/src/Swc_BcmCan.c, firmwa | — | firmware/ecu/bcm/test/test_Swc_BcmCan_qm.c | **TRACED** |
| SSR-CVC-001 | firmware/ecu/cvc/include/Swc_Pedal.h, firmware/ecu/cvc/src/Swc_Pedal.c | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c, test/sil/scena | **TRACED** |
| SSR-FZC-001 | firmware/ecu/fzc/include/Fzc_App.h, firmware/ecu/fzc/include/Swc_Steering.h, fir | firmware/ecu/fzc/cfg/Dcm_Cfg_Fzc.c | firmware/ecu/fzc/test/test_Swc_Steering_asild.c, test/sil/sc | **TRACED** |
| SSR-ICU-001 | firmware/ecu/icu/src/icu_main.c | — | firmware/ecu/cvc/test/test_Swc_Dashboard_qm.c, firmware/ecu/ | **TRACED** |
| SSR-RZC-001 | firmware/ecu/rzc/include/Rzc_App.h, firmware/ecu/rzc/include/Swc_Motor.h, firmwa | firmware/ecu/rzc/cfg/Dcm_Cfg_Rzc.c | firmware/ecu/rzc/test/test_Swc_Motor_asild.c, test/sil/scena | **TRACED** |
| SSR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, firmware/ecu/sc/include/sc_can.h, firmware/ | — | firmware/ecu/sc/test/test_sc_can_asild.c, test/sil/scenarios | **TRACED** |
| SSR-TCU-001 | — | — | firmware/ecu/tcu/test/test_Swc_UdsServer_qm.c | **TEST ONLY** |
| SSR-BCM-002 | firmware/ecu/bcm/include/Swc_BcmCan.h, firmware/ecu/bcm/src/Swc_BcmCan.c | — | firmware/ecu/bcm/test/test_Swc_BcmCan_qm.c | **TRACED** |
| SSR-ICU-002 | firmware/ecu/icu/include/Swc_Dashboard.h, firmware/ecu/icu/src/Swc_Dashboard.c | — | firmware/ecu/cvc/test/test_Swc_Dashboard_qm.c | **TRACED** |
| SSR-SC-002 | firmware/ecu/sc/include/sc_can.h, firmware/ecu/sc/src/sc_can.c | — | firmware/ecu/sc/test/test_sc_can_asild.c, firmware/ecu/sc/te | **TRACED** |
| SSR-CVC-002 | — | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c, test/sil/scena | **TEST ONLY** |
| SSR-FZC-002 | — | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TEST ONLY** |
| SSR-RZC-002 | — | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TEST ONLY** |
| SSR-TCU-002 | — | — | firmware/ecu/tcu/test/test_Swc_DtcStore_qm.c, firmware/ecu/t | **TEST ONLY** |
| SSR-BCM-003 | firmware/ecu/bcm/include/Swc_Lights.h, firmware/ecu/bcm/src/Swc_Lights.c | — | firmware/ecu/bcm/test/test_Swc_Lights_qm.c | **TRACED** |
| SSR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, firmware/ecu/sc/src/sc_e2e.c | — | firmware/ecu/sc/test/test_sc_e2e_asild.c, firmware/ecu/sc/te | **TRACED** |
| SSR-CVC-003 | — | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c | **TEST ONLY** |
| SSR-FZC-003 | — | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TEST ONLY** |
| SSR-RZC-003 | — | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c, test/sil/scena | **TEST ONLY** |
| SSR-BCM-004 | firmware/ecu/bcm/include/Swc_Lights.h, firmware/ecu/bcm/src/Swc_Lights.c | — | firmware/ecu/bcm/test/test_Swc_Lights_qm.c | **TRACED** |
| SSR-SC-004 | firmware/ecu/sc/include/sc_heartbeat.h, firmware/ecu/sc/src/sc_heartbeat.c | — | firmware/ecu/sc/test/test_sc_heartbeat_asilc.c, firmware/ecu | **TRACED** |
| SSR-BCM-005 | firmware/ecu/bcm/include/Swc_Lights.h, firmware/ecu/bcm/src/Swc_Lights.c | — | firmware/ecu/bcm/test/test_Swc_Lights_qm.c | **TRACED** |
| SSR-RZC-005 | firmware/ecu/rzc/include/Swc_CurrentMonitor.h, firmware/ecu/rzc/src/Swc_CurrentM | — | firmware/ecu/rzc/test/test_Swc_Encoder_asilc.c, test/sil/sce | **TRACED** |
| SSR-SC-005 | firmware/ecu/sc/include/sc_heartbeat.h, firmware/ecu/sc/src/sc_heartbeat.c | — | firmware/ecu/sc/test/test_sc_heartbeat_asilc.c, firmware/ecu | **TRACED** |
| SSR-FZC-005 | — | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TEST ONLY** |
| SSR-BCM-006 | firmware/ecu/bcm/include/Swc_Indicators.h, firmware/ecu/bcm/src/Swc_Indicators.c | — | firmware/ecu/bcm/test/test_Swc_Indicators_qm.c | **TRACED** |
| SSR-RZC-006 | firmware/ecu/rzc/include/Swc_CurrentMonitor.h, firmware/ecu/rzc/include/Swc_Moto | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-SC-006 | firmware/ecu/sc/include/sc_heartbeat.h, firmware/ecu/sc/src/sc_heartbeat.c | — | firmware/ecu/sc/test/test_sc_heartbeat_asilc.c | **TRACED** |
| SSR-FZC-006 | — | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TEST ONLY** |
| SSR-BCM-007 | firmware/ecu/bcm/include/Swc_Indicators.h, firmware/ecu/bcm/src/Swc_Indicators.c | — | firmware/ecu/bcm/test/test_Swc_Indicators_qm.c | **TRACED** |
| SSR-ICU-007 | firmware/ecu/icu/include/Swc_Dashboard.h, firmware/ecu/icu/src/Swc_Dashboard.c | — | firmware/ecu/cvc/test/test_Swc_Dashboard_qm.c | **TRACED** |
| SSR-RZC-007 | firmware/ecu/rzc/include/Swc_CurrentMonitor.h, firmware/ecu/rzc/src/Swc_CurrentM | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-SC-007 | firmware/ecu/sc/include/sc_plausibility.h, firmware/ecu/sc/src/sc_plausibility.c | — | firmware/ecu/sc/test/test_sc_plausibility_asilc.c | **TRACED** |
| SSR-FZC-007 | — | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TEST ONLY** |
| SSR-BCM-008 | firmware/ecu/bcm/include/Swc_Indicators.h, firmware/ecu/bcm/src/Swc_Indicators.c | — | firmware/ecu/bcm/test/test_Swc_Indicators_qm.c | **TRACED** |
| SSR-CVC-008 | firmware/bsw/services/E2E/include/E2E.h, firmware/ecu/cvc/include/Swc_Pedal.h, f | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c | **TRACED** |
| SSR-FZC-008 | firmware/ecu/fzc/include/Swc_Steering.h, firmware/ecu/fzc/src/Swc_Steering.c | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TRACED** |
| SSR-ICU-008 | firmware/ecu/icu/include/Swc_DtcDisplay.h, firmware/ecu/icu/src/Swc_DtcDisplay.c | — | firmware/ecu/cvc/test/test_Swc_Dashboard_qm.c | **TRACED** |
| SSR-RZC-008 | firmware/bsw/services/E2E/include/E2E.h, firmware/ecu/rzc/include/Swc_CurrentMon | — | firmware/ecu/rzc/test/test_Swc_RzcCom_asild.c, test/sil/scen | **TRACED** |
| SSR-SC-008 | firmware/ecu/sc/include/sc_plausibility.h, firmware/ecu/sc/src/sc_plausibility.c | — | firmware/ecu/sc/test/test_sc_plausibility_asilc.c | **TRACED** |
| SSR-BCM-009 | firmware/ecu/bcm/include/Swc_DoorLock.h, firmware/ecu/bcm/src/Swc_DoorLock.c | — | firmware/ecu/bcm/test/test_Swc_DoorLock_qm.c | **TRACED** |
| SSR-CVC-009 | firmware/ecu/cvc/include/Swc_VehicleState.h, firmware/ecu/cvc/src/Swc_VehicleSta | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c | **TRACED** |
| SSR-FZC-009 | firmware/ecu/fzc/include/Swc_Brake.h, firmware/ecu/fzc/src/Swc_Brake.c | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TRACED** |
| SSR-ICU-009 | firmware/ecu/icu/include/Swc_Dashboard.h, firmware/ecu/icu/src/Swc_Dashboard.c | — | firmware/ecu/cvc/test/test_Swc_Dashboard_qm.c | **TRACED** |
| SSR-RZC-009 | firmware/ecu/rzc/include/Swc_TempMonitor.h, firmware/ecu/rzc/src/Swc_TempMonitor | — | firmware/ecu/rzc/test/test_Swc_RzcCom_asild.c | **TRACED** |
| SSR-SC-009 | firmware/ecu/sc/include/sc_plausibility.h, firmware/ecu/sc/src/sc_plausibility.c | — | firmware/ecu/sc/test/test_sc_plausibility_asilc.c | **TRACED** |
| SSR-BCM-010 | firmware/ecu/bcm/include/Swc_BcmCan.h, firmware/ecu/bcm/src/Swc_BcmCan.c | — | firmware/ecu/bcm/test/test_Swc_BcmCan_qm.c, firmware/ecu/bcm | **TRACED** |
| SSR-ICU-010 | firmware/ecu/icu/src/icu_main.c | — | firmware/ecu/cvc/test/test_Swc_Dashboard_qm.c | **TRACED** |
| SSR-RZC-010 | firmware/ecu/rzc/include/Swc_TempMonitor.h, firmware/ecu/rzc/src/Swc_TempMonitor | — | firmware/ecu/cvc/test/test_Swc_Heartbeat_asilc.c | **TRACED** |
| SSR-SC-010 | firmware/ecu/sc/include/sc_relay.h, firmware/ecu/sc/include/sc_selftest.h, firmw | — | firmware/ecu/sc/test/test_sc_relay_asild.c | **TRACED** |
| SSR-CVC-010 | — | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TEST ONLY** |
| SSR-FZC-010 | — | — | firmware/ecu/fzc/test/test_Swc_Lidar_asilc.c | **TEST ONLY** |
| SSR-BCM-011 | firmware/ecu/bcm/include/Swc_BcmCan.h, firmware/ecu/bcm/src/Swc_BcmCan.c | — | firmware/ecu/bcm/test/test_Swc_BcmCan_qm.c, firmware/ecu/bcm | **TRACED** |
| SSR-RZC-011 | firmware/ecu/rzc/include/Swc_TempMonitor.h, firmware/ecu/rzc/src/Swc_TempMonitor | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-SC-011 | firmware/ecu/sc/include/sc_relay.h, firmware/ecu/sc/include/sc_selftest.h, firmw | — | firmware/ecu/sc/test/test_sc_relay_asild.c | **TRACED** |
| SSR-CVC-011 | — | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TEST ONLY** |
| SSR-FZC-011 | — | — | firmware/ecu/fzc/test/test_Swc_Lidar_asilc.c | **TEST ONLY** |
| SSR-BCM-012 | firmware/ecu/bcm/include/Swc_BcmMain.h, firmware/ecu/bcm/src/Swc_BcmMain.c, firm | — | firmware/ecu/bcm/test/test_Swc_BcmMain_qm.c | **TRACED** |
| SSR-FZC-012 | firmware/ecu/fzc/include/Swc_Brake.h, firmware/ecu/fzc/src/Swc_Brake.c | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TRACED** |
| SSR-RZC-012 | firmware/ecu/rzc/include/Swc_Encoder.h, firmware/ecu/rzc/src/Swc_Encoder.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-SC-012 | firmware/ecu/sc/include/sc_relay.h, firmware/ecu/sc/src/sc_relay.c | — | firmware/ecu/sc/test/test_sc_relay_asild.c | **TRACED** |
| SSR-CVC-012 | — | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TEST ONLY** |
| SSR-CVC-013 | firmware/ecu/cvc/include/Swc_VehicleState.h, firmware/ecu/cvc/src/Swc_VehicleSta | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SSR-FZC-013 | firmware/ecu/fzc/include/Swc_Lidar.h, firmware/ecu/fzc/src/Swc_Lidar.c | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TRACED** |
| SSR-RZC-013 | firmware/ecu/rzc/include/Swc_Encoder.h, firmware/ecu/rzc/include/Swc_RzcSelfTest | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-SC-013 | firmware/ecu/sc/include/sc_led.h, firmware/ecu/sc/src/sc_led.c | — | firmware/ecu/sc/test/test_sc_led_qm.c | **TRACED** |
| SSR-CVC-014 | firmware/ecu/cvc/include/Swc_CvcCom.h, firmware/ecu/cvc/src/Swc_CvcCom.c | — | firmware/ecu/cvc/test/test_Swc_CvcCom_asild.c | **TRACED** |
| SSR-RZC-014 | firmware/ecu/rzc/include/Swc_Encoder.h, firmware/ecu/rzc/src/Swc_Encoder.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-SC-014 | firmware/ecu/sc/include/sc_esm.h, firmware/ecu/sc/src/sc_esm.c | — | firmware/ecu/sc/test/test_sc_esm_asilc.c | **TRACED** |
| SSR-TCU-014 | firmware/ecu/tcu/include/Swc_DataAggregator.h, firmware/ecu/tcu/src/Swc_DataAggr | — | firmware/ecu/tcu/test/test_Swc_DataAggregator_qm.c, firmware | **TRACED** |
| SSR-CVC-015 | firmware/ecu/cvc/include/Swc_CvcCom.h, firmware/ecu/cvc/src/Swc_CvcCom.c | — | firmware/ecu/cvc/test/test_Swc_CvcCom_asild.c | **TRACED** |
| SSR-SC-015 | firmware/ecu/sc/include/sc_esm.h, firmware/ecu/sc/include/sc_watchdog.h, firmwar | — | firmware/ecu/sc/test/test_sc_esm_asilc.c | **TRACED** |
| SSR-FZC-015 | firmware/bsw/services/E2E/include/E2E.h | — | firmware/ecu/fzc/test/test_Swc_FzcSafety_asild.c, test/sil/s | **TRACED** |
| SSR-CVC-016 | firmware/ecu/cvc/include/Swc_CvcCom.h, firmware/ecu/cvc/src/Swc_CvcCom.c | — | firmware/ecu/cvc/test/test_Swc_CvcCom_asild.c, test/sil/scen | **TRACED** |
| SSR-FZC-016 | firmware/ecu/fzc/include/Swc_Lidar.h, firmware/ecu/fzc/src/Swc_Lidar.c | — | firmware/ecu/fzc/test/test_Swc_FzcSafety_asild.c | **TRACED** |
| SSR-SC-016 | firmware/ecu/sc/include/sc_selftest.h, firmware/ecu/sc/src/sc_selftest.c | — | firmware/ecu/sc/test/test_sc_selftest_asild.c | **TRACED** |
| SSR-CVC-017 | firmware/ecu/cvc/include/Swc_CvcCom.h, firmware/ecu/cvc/src/Swc_CvcCom.c | — | firmware/ecu/cvc/test/test_Swc_Heartbeat_asilc.c | **TRACED** |
| SSR-FZC-017 | firmware/ecu/fzc/include/Swc_Buzzer.h, firmware/ecu/fzc/src/Swc_Buzzer.c | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TRACED** |
| SSR-RZC-017 | firmware/ecu/rzc/include/Rzc_App.h, firmware/ecu/rzc/include/Swc_Battery.h, firm | firmware/ecu/rzc/cfg/Dcm_Cfg_Rzc.c | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-SC-017 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, firmware/ecu/sc/include/sc_selftest.h, firm | — | firmware/ecu/sc/test/test_sc_selftest_asild.c, firmware/ecu/ | **TRACED** |
| SSR-CVC-018 | firmware/ecu/cvc/include/Swc_EStop.h, firmware/ecu/cvc/src/Swc_EStop.c | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SSR-FZC-018 | firmware/ecu/fzc/include/Swc_Buzzer.h, firmware/ecu/fzc/src/Swc_Buzzer.c | — | firmware/ecu/cvc/test/test_Swc_Heartbeat_asilc.c, firmware/e | **TRACED** |
| SSR-RZC-018 | firmware/ecu/rzc/include/Swc_Battery.h, firmware/ecu/rzc/src/Swc_Battery.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-SC-018 | firmware/ecu/sc/include/sc_plausibility.h, firmware/ecu/sc/src/sc_main.c, firmwa | — | firmware/ecu/sc/test/test_sc_relay_asild.c | **TRACED** |
| SSR-FZC-019 | firmware/ecu/fzc/include/Swc_FzcCom.h, firmware/ecu/fzc/src/Swc_FzcCom.c, firmwa | — | firmware/ecu/fzc/test/test_Swc_FzcCom_asild.c, firmware/ecu/ | **TRACED** |
| SSR-RZC-019 | firmware/ecu/rzc/include/Swc_RzcCom.h, firmware/ecu/rzc/src/Swc_RzcCom.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-CVC-020 | firmware/ecu/cvc/include/Swc_EStop.h, firmware/ecu/cvc/src/Swc_EStop.c | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SSR-FZC-020 | firmware/ecu/fzc/include/Swc_FzcCom.h, firmware/ecu/fzc/src/Swc_FzcCom.c | — | firmware/ecu/fzc/test/test_Swc_FzcCom_asild.c | **TRACED** |
| SSR-RZC-020 | firmware/ecu/rzc/include/Swc_RzcCom.h, firmware/ecu/rzc/src/Swc_RzcCom.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-CVC-021 | firmware/ecu/cvc/include/Swc_Heartbeat.h, firmware/ecu/cvc/src/Swc_Heartbeat.c | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SSR-FZC-021 | firmware/ecu/fzc/include/Swc_Heartbeat.h, firmware/ecu/fzc/src/Swc_Heartbeat.c | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TRACED** |
| SSR-RZC-021 | firmware/ecu/rzc/include/Swc_Heartbeat.h, firmware/ecu/rzc/src/Swc_Heartbeat.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-CVC-022 | firmware/ecu/cvc/include/Swc_Heartbeat.h, firmware/ecu/cvc/src/Swc_Heartbeat.c | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SSR-FZC-022 | firmware/ecu/fzc/include/Swc_Heartbeat.h, firmware/ecu/fzc/src/Swc_Heartbeat.c | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TRACED** |
| SSR-RZC-022 | firmware/ecu/rzc/include/Swc_Heartbeat.h, firmware/ecu/rzc/src/Swc_Heartbeat.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-CVC-023 | firmware/ecu/cvc/include/Swc_Watchdog.h, firmware/ecu/cvc/src/Swc_Watchdog.c | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SSR-FZC-023 | firmware/ecu/fzc/include/Swc_FzcSafety.h, firmware/ecu/fzc/src/Swc_FzcSafety.c | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TRACED** |
| SSR-RZC-023 | firmware/ecu/rzc/include/Swc_RzcSafety.h, firmware/ecu/rzc/src/Swc_RzcSafety.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-CVC-024 | firmware/ecu/cvc/include/Swc_CanMonitor.h, firmware/ecu/cvc/src/Swc_CanMonitor.c | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SSR-FZC-024 | firmware/ecu/fzc/include/Fzc_App.h, firmware/ecu/fzc/include/Swc_FzcCanMonitor.h | firmware/ecu/fzc/cfg/Dcm_Cfg_Fzc.c | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TRACED** |
| SSR-RZC-024 | firmware/ecu/rzc/include/Swc_RzcSafety.h, firmware/ecu/rzc/src/Swc_RzcSafety.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SSR-CVC-025 | firmware/ecu/cvc/include/Swc_CanMonitor.h, firmware/ecu/cvc/src/Swc_CanMonitor.c | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SSR-FZC-025 | firmware/ecu/fzc/include/Swc_FzcSafety.h, firmware/ecu/fzc/src/Swc_FzcSafety.c | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TRACED** |
| SSR-CVC-026 | firmware/ecu/cvc/include/Ssd1306.h, firmware/ecu/cvc/src/Ssd1306.c | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SSR-FZC-026 | firmware/ecu/fzc/include/Swc_FzcCom.h, firmware/ecu/fzc/src/Swc_FzcCom.c | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TRACED** |
| SSR-RZC-026 | firmware/ecu/rzc/include/Swc_RzcCom.h, firmware/ecu/rzc/src/Swc_RzcCom.c | — | firmware/ecu/rzc/test/test_Swc_Encoder_asilc.c | **TRACED** |
| SSR-BSW-026 | firmware/bsw/services/E2E/include/E2E_Sm.h, firmware/bsw/services/E2E/src/E2E_Sm | — | test/sil/scenarios/sil_004_can_busoff_fzc.yaml | **TRACED** |
| SSR-CVC-027 | firmware/ecu/cvc/include/Swc_Dashboard.h, firmware/ecu/cvc/src/Swc_Dashboard.c | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SSR-FZC-027 | firmware/ecu/fzc/include/Swc_FzcCom.h, firmware/ecu/fzc/src/Swc_FzcCom.c | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TRACED** |
| SSR-RZC-027 | firmware/ecu/rzc/include/Swc_RzcCom.h, firmware/ecu/rzc/src/Swc_RzcCom.c | — | firmware/ecu/rzc/test/test_Swc_Encoder_asilc.c | **TRACED** |
| SSR-CVC-028 | firmware/ecu/cvc/include/Swc_Dashboard.h, firmware/ecu/cvc/src/Swc_Dashboard.c | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SSR-RZC-028 | firmware/ecu/rzc/include/Swc_RzcScheduler.h, firmware/ecu/rzc/src/Swc_RzcSchedul | — | firmware/ecu/rzc/test/test_Swc_Encoder_asilc.c | **TRACED** |
| SSR-CVC-029 | firmware/ecu/cvc/include/Swc_SelfTest.h, firmware/ecu/cvc/src/Swc_SelfTest.c, fi | — | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c, test/sil/scena | **TRACED** |
| SSR-FZC-029 | firmware/ecu/fzc/include/Swc_FzcScheduler.h, firmware/ecu/fzc/src/Swc_FzcSchedul | — | firmware/ecu/fzc/test/test_Swc_Lidar_asilc.c | **TRACED** |
| SSR-RZC-029 | firmware/ecu/rzc/include/Swc_RzcDcm.h, firmware/ecu/rzc/src/Swc_RzcDcm.c | — | firmware/ecu/rzc/test/test_Swc_Encoder_asilc.c | **TRACED** |
| SSR-CVC-030 | firmware/ecu/cvc/include/Swc_Nvm.h, firmware/ecu/cvc/src/Swc_Nvm.c | firmware/ecu/cvc/cfg/Dcm_Cfg_Cvc.c | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c | **TRACED** |
| SSR-FZC-030 | firmware/ecu/fzc/include/Swc_FzcDcm.h, firmware/ecu/fzc/src/Swc_FzcDcm.c | — | firmware/ecu/fzc/test/test_Swc_Lidar_asilc.c | **TRACED** |
| SSR-RZC-030 | firmware/ecu/rzc/include/Swc_RzcNvm.h, firmware/ecu/rzc/src/Swc_RzcNvm.c | — | firmware/ecu/rzc/test/test_Swc_Encoder_asilc.c | **TRACED** |
| SSR-CVC-031 | firmware/ecu/cvc/include/Swc_Nvm.h, firmware/ecu/cvc/src/Swc_Nvm.c | — | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c | **TRACED** |
| SSR-FZC-031 | firmware/ecu/fzc/include/Swc_FzcNvm.h, firmware/ecu/fzc/src/Swc_FzcNvm.c | — | firmware/ecu/fzc/test/test_Swc_Lidar_asilc.c | **TRACED** |
| SSR-CVC-032 | firmware/ecu/cvc/include/Swc_Scheduler.h, firmware/ecu/cvc/src/Swc_Scheduler.c | — | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c | **TRACED** |
| SSR-FZC-032 | firmware/ecu/fzc/include/Swc_FzcNvm.h, firmware/ecu/fzc/src/Swc_FzcNvm.c | — | firmware/ecu/fzc/test/test_Swc_Lidar_asilc.c | **TRACED** |
| SSR-CVC-033 | firmware/ecu/cvc/include/Swc_CvcDcm.h, firmware/ecu/cvc/src/Swc_CvcDcm.c | — | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c | **TRACED** |
| SSR-CVC-034 | firmware/ecu/cvc/include/Swc_CvcDcm.h, firmware/ecu/cvc/src/Swc_CvcDcm.c | — | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c | **TRACED** |
| SSR-CVC-035 | firmware/ecu/cvc/include/Swc_CvcDcm.h, firmware/ecu/cvc/src/Swc_CvcDcm.c, firmwa | firmware/ecu/cvc/cfg/Dcm_Cfg_Cvc.c | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c | **TRACED** |
| SWR-BCM-001 | firmware/ecu/bcm/include/Swc_BcmCan.h, firmware/ecu/bcm/src/Swc_BcmCan.c, firmwa | — | firmware/ecu/bcm/test/test_Swc_BcmCan_qm.c | **TRACED** |
| SWR-CVC-001 | firmware/ecu/cvc/include/Swc_Pedal.h, firmware/ecu/cvc/src/Swc_Pedal.c | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c | **TRACED** |
| SWR-FZC-001 | firmware/ecu/fzc/include/Fzc_App.h, firmware/ecu/fzc/include/Swc_Steering.h, fir | firmware/ecu/fzc/cfg/Dcm_Cfg_Fzc.c | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TRACED** |
| SWR-ICU-001 | firmware/ecu/icu/src/icu_main.c | — | firmware/ecu/cvc/test/test_Swc_Dashboard_qm.c, firmware/ecu/ | **TRACED** |
| SWR-RZC-001 | firmware/ecu/rzc/include/Rzc_App.h, firmware/ecu/rzc/include/Swc_Motor.h, firmwa | firmware/ecu/rzc/cfg/Dcm_Cfg_Rzc.c | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SWR-SC-001 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, firmware/ecu/sc/include/sc_can.h, firmware/ | — | firmware/ecu/sc/test/test_sc_can_asild.c | **TRACED** |
| SWR-TCU-001 | firmware/ecu/tcu/src/tcu_main.c | firmware/ecu/tcu/cfg/Dcm_Cfg_Tcu.c | firmware/ecu/tcu/test/test_Swc_TcuCan_qm.c | **TRACED** |
| SWR-BSW-001 | firmware/bsw/mcal/Can/include/Can.h, firmware/bsw/mcal/Can/src/Can.c | — | firmware/bsw/test/test_Can_Posix_asild.c, firmware/bsw/test/ | **TRACED** |
| SWR-BCM-002 | firmware/ecu/bcm/include/Swc_BcmCan.h, firmware/ecu/bcm/src/Swc_BcmCan.c | — | firmware/ecu/bcm/test/test_Swc_BcmCan_qm.c | **TRACED** |
| SWR-ICU-002 | firmware/ecu/icu/include/Swc_Dashboard.h, firmware/ecu/icu/src/Swc_Dashboard.c | — | firmware/ecu/cvc/test/test_Swc_Dashboard_qm.c, firmware/ecu/ | **TRACED** |
| SWR-SC-002 | firmware/ecu/sc/include/sc_can.h, firmware/ecu/sc/src/sc_can.c | — | firmware/ecu/sc/test/test_sc_can_asild.c | **TRACED** |
| SWR-TCU-002 | firmware/ecu/tcu/include/Swc_UdsServer.h, firmware/ecu/tcu/src/Swc_UdsServer.c | — | firmware/ecu/tcu/test/test_Swc_UdsServer_qm.c | **TRACED** |
| SWR-CVC-002 | — | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c | **TEST ONLY** |
| SWR-FZC-002 | — | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TEST ONLY** |
| SWR-RZC-002 | — | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TEST ONLY** |
| SWR-BSW-002 | firmware/bsw/mcal/Can/include/Can.h, firmware/bsw/mcal/Can/src/Can.c | — | firmware/bsw/test/test_Can_Posix_asild.c, firmware/bsw/test/ | **TRACED** |
| SWR-BCM-003 | firmware/ecu/bcm/include/Swc_Lights.h, firmware/ecu/bcm/src/Swc_Lights.c | — | firmware/ecu/bcm/test/test_Swc_Lights_qm.c | **TRACED** |
| SWR-ICU-003 | firmware/ecu/icu/include/Swc_Dashboard.h, firmware/ecu/icu/src/Swc_Dashboard.c | — | firmware/ecu/icu/test/test_Swc_Dashboard_qm.c | **TRACED** |
| SWR-SC-003 | firmware/ecu/sc/include/sc_e2e.h, firmware/ecu/sc/src/sc_e2e.c | — | firmware/ecu/sc/test/test_sc_e2e_asild.c, firmware/ecu/sc/te | **TRACED** |
| SWR-TCU-003 | firmware/ecu/tcu/include/Swc_UdsServer.h, firmware/ecu/tcu/src/Swc_UdsServer.c | — | firmware/ecu/tcu/test/test_Swc_UdsServer_qm.c | **TRACED** |
| SWR-CVC-003 | — | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c | **TEST ONLY** |
| SWR-FZC-003 | — | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TEST ONLY** |
| SWR-RZC-003 | — | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TEST ONLY** |
| SWR-BSW-003 | firmware/bsw/mcal/Can/include/Can.h, firmware/bsw/mcal/Can/src/Can.c | — | firmware/bsw/test/test_Can_Posix_asild.c, firmware/bsw/test/ | **TRACED** |
| SWR-BCM-004 | firmware/ecu/bcm/include/Swc_Lights.h, firmware/ecu/bcm/src/Swc_Lights.c | — | firmware/ecu/bcm/test/test_Swc_Lights_qm.c | **TRACED** |
| SWR-ICU-004 | firmware/ecu/icu/include/Swc_Dashboard.h, firmware/ecu/icu/src/Swc_Dashboard.c | — | firmware/ecu/icu/test/test_Swc_Dashboard_qm.c | **TRACED** |
| SWR-RZC-004 | firmware/ecu/rzc/include/Swc_Motor.h, firmware/ecu/rzc/src/Swc_Motor.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SWR-SC-004 | firmware/ecu/sc/include/sc_heartbeat.h, firmware/ecu/sc/src/sc_heartbeat.c | — | firmware/ecu/sc/test/test_sc_heartbeat_asilc.c | **TRACED** |
| SWR-TCU-004 | firmware/ecu/tcu/include/Swc_UdsServer.h, firmware/ecu/tcu/src/Swc_UdsServer.c | — | firmware/ecu/tcu/test/test_Swc_UdsServer_qm.c | **TRACED** |
| SWR-CVC-004 | — | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c | **TEST ONLY** |
| SWR-FZC-004 | — | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TEST ONLY** |
| SWR-BSW-004 | firmware/bsw/mcal/Can/include/Can.h, firmware/bsw/mcal/Can/src/Can.c | — | firmware/bsw/test/test_Can_Posix_asild.c, firmware/bsw/test/ | **TRACED** |
| SWR-BCM-005 | firmware/ecu/bcm/include/Swc_Lights.h, firmware/ecu/bcm/src/Swc_Lights.c | — | firmware/ecu/bcm/test/test_Swc_Lights_qm.c | **TRACED** |
| SWR-ICU-005 | firmware/ecu/icu/include/Swc_Dashboard.h, firmware/ecu/icu/src/Swc_Dashboard.c | — | firmware/ecu/icu/test/test_Swc_Dashboard_qm.c | **TRACED** |
| SWR-RZC-005 | firmware/ecu/rzc/include/Swc_CurrentMonitor.h, firmware/ecu/rzc/src/Swc_CurrentM | — | firmware/ecu/rzc/test/test_Swc_CurrentMonitor_asila.c, firmw | **TRACED** |
| SWR-SC-005 | firmware/ecu/sc/include/sc_heartbeat.h, firmware/ecu/sc/src/sc_heartbeat.c | — | firmware/ecu/sc/test/test_sc_heartbeat_asilc.c | **TRACED** |
| SWR-TCU-005 | firmware/ecu/tcu/include/Swc_UdsServer.h, firmware/ecu/tcu/src/Swc_UdsServer.c | — | firmware/ecu/tcu/test/test_Swc_UdsServer_qm.c | **TRACED** |
| SWR-CVC-005 | — | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c | **TEST ONLY** |
| SWR-FZC-005 | — | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c, firmware/ecu/f | **TEST ONLY** |
| SWR-BSW-005 | firmware/bsw/mcal/Can/include/Can.h, firmware/bsw/mcal/Can/src/Can.c | — | firmware/bsw/test/test_Can_Posix_asild.c, firmware/bsw/test/ | **TRACED** |
| SWR-BCM-006 | firmware/ecu/bcm/include/Swc_Indicators.h, firmware/ecu/bcm/src/Swc_Indicators.c | — | firmware/ecu/bcm/test/test_Swc_Indicators_qm.c | **TRACED** |
| SWR-RZC-006 | firmware/ecu/rzc/include/Swc_CurrentMonitor.h, firmware/ecu/rzc/src/Swc_CurrentM | — | firmware/ecu/rzc/test/test_Swc_CurrentMonitor_asila.c | **TRACED** |
| SWR-SC-006 | firmware/ecu/sc/include/sc_heartbeat.h, firmware/ecu/sc/src/sc_heartbeat.c | — | firmware/ecu/sc/test/test_sc_heartbeat_asilc.c | **TRACED** |
| SWR-TCU-006 | firmware/ecu/tcu/src/Swc_UdsServer.c | — | firmware/ecu/tcu/test/test_Swc_UdsServer_qm.c | **TRACED** |
| SWR-CVC-006 | — | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c | **TEST ONLY** |
| SWR-FZC-006 | — | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c, firmware/ecu/f | **TEST ONLY** |
| SWR-ICU-006 | — | — | firmware/ecu/icu/test/test_Swc_Dashboard_qm.c | **TEST ONLY** |
| SWR-BSW-006 | firmware/bsw/mcal/Spi/include/Spi.h, firmware/bsw/mcal/Spi/src/Spi.c | — | firmware/bsw/test/test_Spi_asild.c, test/unit/bsw/test_Spi_a | **TRACED** |
| SWR-BCM-007 | firmware/ecu/bcm/include/Swc_Indicators.h, firmware/ecu/bcm/src/Swc_Indicators.c | — | firmware/ecu/bcm/test/test_Swc_Indicators_qm.c | **TRACED** |
| SWR-RZC-007 | firmware/ecu/rzc/include/Swc_CurrentMonitor.h, firmware/ecu/rzc/src/Swc_CurrentM | — | firmware/ecu/rzc/test/test_Swc_CurrentMonitor_asila.c | **TRACED** |
| SWR-SC-007 | firmware/ecu/sc/include/sc_plausibility.h, firmware/ecu/sc/src/sc_plausibility.c | — | firmware/ecu/sc/test/test_sc_plausibility_asilc.c | **TRACED** |
| SWR-TCU-007 | firmware/ecu/tcu/src/Swc_UdsServer.c | — | firmware/ecu/tcu/test/test_Swc_UdsServer_qm.c | **TRACED** |
| SWR-CVC-007 | — | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c | **TEST ONLY** |
| SWR-FZC-007 | — | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c, firmware/ecu/f | **TEST ONLY** |
| SWR-ICU-007 | — | — | firmware/ecu/icu/test/test_Swc_Dashboard_qm.c | **TEST ONLY** |
| SWR-BSW-007 | firmware/bsw/mcal/Adc/include/Adc.h, firmware/bsw/mcal/Adc/src/Adc.c | — | firmware/bsw/test/test_Adc_asila.c, test/unit/bsw/test_Adc_a | **TRACED** |
| SWR-BCM-008 | firmware/ecu/bcm/include/Swc_Indicators.h, firmware/ecu/bcm/src/Swc_Indicators.c | — | firmware/ecu/bcm/test/test_Swc_Indicators_qm.c | **TRACED** |
| SWR-CVC-008 | firmware/ecu/cvc/include/Swc_Pedal.h, firmware/ecu/cvc/src/Swc_Pedal.c | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c | **TRACED** |
| SWR-FZC-008 | firmware/ecu/fzc/include/Swc_Steering.h, firmware/ecu/fzc/src/Swc_Steering.c | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TRACED** |
| SWR-ICU-008 | firmware/ecu/icu/include/Swc_DtcDisplay.h, firmware/ecu/icu/src/Swc_DtcDisplay.c | — | firmware/ecu/icu/test/test_Swc_DtcDisplay_qm.c | **TRACED** |
| SWR-RZC-008 | firmware/ecu/rzc/include/Swc_CurrentMonitor.h, firmware/ecu/rzc/src/Swc_CurrentM | — | firmware/ecu/rzc/test/test_Swc_CurrentMonitor_asila.c, firmw | **TRACED** |
| SWR-SC-008 | firmware/ecu/sc/include/sc_plausibility.h, firmware/ecu/sc/src/sc_plausibility.c | — | firmware/ecu/sc/test/test_sc_plausibility_asilc.c | **TRACED** |
| SWR-TCU-008 | firmware/ecu/tcu/include/Swc_DtcStore.h, firmware/ecu/tcu/src/Swc_DtcStore.c | — | firmware/ecu/tcu/test/test_Swc_DtcStore_qm.c | **TRACED** |
| SWR-BSW-008 | firmware/bsw/mcal/Pwm/include/Pwm.h, firmware/bsw/mcal/Pwm/src/Pwm.c | — | firmware/bsw/test/test_Pwm_asild.c, test/unit/bsw/test_IoHwA | **TRACED** |
| SWR-BCM-009 | firmware/ecu/bcm/include/Swc_DoorLock.h, firmware/ecu/bcm/src/Swc_DoorLock.c | — | firmware/ecu/bcm/test/test_Swc_DoorLock_qm.c | **TRACED** |
| SWR-CVC-009 | firmware/ecu/cvc/include/Swc_VehicleState.h, firmware/ecu/cvc/src/Swc_VehicleSta | — | firmware/ecu/cvc/test/test_Swc_Pedal_asild.c, firmware/ecu/c | **TRACED** |
| SWR-FZC-009 | firmware/ecu/fzc/include/Swc_Brake.h, firmware/ecu/fzc/src/Swc_Brake.c | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TRACED** |
| SWR-RZC-009 | firmware/ecu/rzc/include/Swc_TempMonitor.h, firmware/ecu/rzc/src/Swc_TempMonitor | — | firmware/ecu/rzc/test/test_Swc_RzcCom_asild.c, firmware/ecu/ | **TRACED** |
| SWR-SC-009 | firmware/ecu/sc/include/sc_plausibility.h, firmware/ecu/sc/src/sc_plausibility.c | — | firmware/ecu/sc/test/test_sc_plausibility_asilc.c | **TRACED** |
| SWR-TCU-009 | firmware/ecu/tcu/include/Swc_DtcStore.h, firmware/ecu/tcu/src/Swc_DtcStore.c | — | firmware/ecu/tcu/test/test_Swc_DtcStore_qm.c | **TRACED** |
| SWR-ICU-009 | — | — | firmware/ecu/icu/test/test_Swc_Dashboard_qm.c | **TEST ONLY** |
| SWR-BSW-009 | firmware/bsw/mcal/Dio/include/Dio.h, firmware/bsw/mcal/Dio/src/Dio.c | — | firmware/bsw/test/test_Dio_asild.c, test/unit/bsw/test_Can_a | **TRACED** |
| SWR-BCM-010 | firmware/ecu/bcm/include/Swc_BcmCan.h, firmware/ecu/bcm/src/Swc_BcmCan.c | — | firmware/ecu/bcm/test/test_Swc_BcmCan_qm.c, firmware/ecu/bcm | **TRACED** |
| SWR-ICU-010 | firmware/ecu/icu/src/icu_main.c | — | firmware/ecu/icu/test/test_Swc_IcuMain_qm.c | **TRACED** |
| SWR-RZC-010 | firmware/ecu/rzc/include/Swc_TempMonitor.h, firmware/ecu/rzc/src/Swc_TempMonitor | — | firmware/ecu/cvc/test/test_Swc_Heartbeat_asilc.c, firmware/e | **TRACED** |
| SWR-SC-010 | firmware/ecu/sc/include/sc_relay.h, firmware/ecu/sc/src/sc_relay.c | — | firmware/ecu/sc/test/test_sc_relay_asild.c | **TRACED** |
| SWR-TCU-010 | firmware/ecu/tcu/include/Swc_Obd2Pids.h, firmware/ecu/tcu/src/Swc_Obd2Pids.c | — | firmware/ecu/tcu/test/test_Swc_Obd2Pids_qm.c | **TRACED** |
| SWR-CVC-010 | — | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TEST ONLY** |
| SWR-FZC-010 | — | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c, firmware/ecu/f | **TEST ONLY** |
| SWR-BSW-010 | firmware/bsw/mcal/Gpt/include/Gpt.h, firmware/bsw/mcal/Gpt/src/Gpt.c, firmware/b | — | firmware/bsw/test/test_Gpt_asild.c, firmware/bsw/test/test_U | **TRACED** |
| SWR-BCM-011 | firmware/ecu/bcm/include/Swc_BcmCan.h, firmware/ecu/bcm/src/Swc_BcmCan.c | — | firmware/ecu/bcm/test/test_Swc_BcmCan_qm.c, firmware/ecu/bcm | **TRACED** |
| SWR-RZC-011 | firmware/ecu/rzc/include/Swc_TempMonitor.h, firmware/ecu/rzc/src/Swc_TempMonitor | — | firmware/ecu/rzc/test/test_Swc_TempMonitor_asila.c | **TRACED** |
| SWR-SC-011 | firmware/ecu/sc/include/sc_relay.h, firmware/ecu/sc/src/sc_relay.c | — | firmware/ecu/sc/test/test_sc_relay_asild.c | **TRACED** |
| SWR-TCU-011 | firmware/ecu/tcu/src/Swc_UdsServer.c | — | firmware/ecu/tcu/test/test_Swc_UdsServer_qm.c | **TRACED** |
| SWR-CVC-011 | — | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TEST ONLY** |
| SWR-FZC-011 | — | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c, firmware/ecu/f | **TEST ONLY** |
| SWR-BSW-011 | firmware/bsw/ecual/CanIf/include/CanIf.h, firmware/bsw/ecual/CanIf/src/CanIf.c | — | firmware/bsw/test/test_CanIf_asild.c, test/unit/bsw/test_Can | **TRACED** |
| SWR-BCM-012 | firmware/ecu/bcm/include/Swc_BcmMain.h, firmware/ecu/bcm/src/Swc_BcmMain.c, firm | — | firmware/ecu/bcm/test/test_Swc_BcmMain_qm.c | **TRACED** |
| SWR-FZC-012 | firmware/ecu/fzc/include/Swc_Brake.h, firmware/ecu/fzc/src/Swc_Brake.c | — | firmware/ecu/fzc/test/test_Swc_Brake_asild.c | **TRACED** |
| SWR-RZC-012 | firmware/ecu/rzc/include/Swc_Encoder.h, firmware/ecu/rzc/src/Swc_Encoder.c | — | firmware/ecu/rzc/test/test_Swc_Encoder_asilc.c | **TRACED** |
| SWR-SC-012 | firmware/ecu/sc/include/sc_relay.h, firmware/ecu/sc/src/sc_relay.c | — | firmware/ecu/sc/test/test_sc_relay_asild.c | **TRACED** |
| SWR-TCU-012 | firmware/ecu/tcu/src/Swc_UdsServer.c | — | firmware/ecu/tcu/test/test_Swc_UdsServer_qm.c | **TRACED** |
| SWR-CVC-012 | — | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TEST ONLY** |
| SWR-BSW-012 | firmware/bsw/ecual/CanIf/include/CanIf.h, firmware/bsw/ecual/CanIf/src/CanIf.c | — | firmware/bsw/test/test_CanIf_asild.c, test/unit/bsw/test_Can | **TRACED** |
| SWR-CVC-013 | firmware/ecu/cvc/include/Swc_VehicleState.h, firmware/ecu/cvc/src/Swc_VehicleSta | — | firmware/ecu/cvc/test/test_Swc_VehicleState_asild.c | **TRACED** |
| SWR-FZC-013 | firmware/ecu/fzc/include/Swc_Lidar.h, firmware/ecu/fzc/src/Swc_Lidar.c | — | firmware/ecu/fzc/test/test_Swc_Lidar_asilc.c | **TRACED** |
| SWR-RZC-013 | firmware/ecu/rzc/include/Swc_Encoder.h, firmware/ecu/rzc/src/Swc_Encoder.c | — | firmware/ecu/rzc/test/test_Swc_Encoder_asilc.c | **TRACED** |
| SWR-SC-013 | firmware/ecu/sc/include/sc_led.h, firmware/ecu/sc/src/sc_led.c | — | firmware/ecu/sc/test/test_sc_led_qm.c | **TRACED** |
| SWR-TCU-013 | firmware/ecu/tcu/src/Swc_UdsServer.c | — | firmware/ecu/tcu/test/test_Swc_UdsServer_qm.c | **TRACED** |
| SWR-BSW-013 | firmware/bsw/ecual/PduR/include/PduR.h, firmware/bsw/ecual/PduR/src/PduR.c | — | firmware/bsw/test/test_PduR_asild.c, test/unit/bsw/test_PduR | **TRACED** |
| SWR-CVC-014 | firmware/ecu/cvc/include/Swc_CvcCom.h, firmware/ecu/cvc/src/Swc_CvcCom.c | — | firmware/ecu/cvc/test/test_Swc_CvcCom_asild.c | **TRACED** |
| SWR-BSW-014 | firmware/bsw/ecual/IoHwAb/include/IoHwAb.h, firmware/bsw/ecual/IoHwAb/include/Io | — | firmware/bsw/test/test_IoHwAb_Hil_asild.c, firmware/bsw/test | **TRACED** |
| SWR-RZC-014 | firmware/ecu/rzc/include/Swc_Encoder.h, firmware/ecu/rzc/src/Swc_Encoder.c | — | firmware/ecu/rzc/test/test_Swc_Encoder_asilc.c | **TRACED** |
| SWR-SC-014 | firmware/ecu/sc/include/sc_esm.h, firmware/ecu/sc/src/sc_esm.c | — | firmware/ecu/sc/test/test_sc_esm_asilc.c | **TRACED** |
| SWR-TCU-014 | firmware/ecu/tcu/include/Swc_DataAggregator.h, firmware/ecu/tcu/src/Swc_DataAggr | — | firmware/ecu/tcu/test/test_Swc_DataAggregator_qm.c | **TRACED** |
| SWR-FZC-014 | — | — | firmware/ecu/fzc/test/test_Swc_Lidar_asilc.c | **TEST ONLY** |
| SWR-CVC-015 | firmware/ecu/cvc/include/Swc_CvcCom.h, firmware/ecu/cvc/src/Swc_CvcCom.c | — | firmware/ecu/cvc/test/test_Swc_CvcCom_asild.c | **TRACED** |
| SWR-RZC-015 | firmware/ecu/rzc/include/Swc_Motor.h, firmware/ecu/rzc/src/Swc_Motor.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SWR-SC-015 | firmware/ecu/sc/include/sc_esm.h, firmware/ecu/sc/src/sc_esm.c | — | firmware/ecu/sc/test/test_sc_esm_asilc.c | **TRACED** |
| SWR-TCU-015 | firmware/ecu/tcu/src/tcu_main.c | — | firmware/ecu/tcu/test/test_Swc_TcuMain_qm.c | **TRACED** |
| SWR-FZC-015 | — | — | firmware/ecu/fzc/test/test_Swc_FzcSafety_asild.c, firmware/e | **TEST ONLY** |
| SWR-BSW-015 | firmware/bsw/services/Com/include/Com.h, firmware/bsw/services/Com/src/Com.c | — | firmware/bsw/test/test_Com_asild.c, test/unit/bsw/test_Com_a | **TRACED** |
| SWR-CVC-016 | firmware/ecu/cvc/include/Swc_CvcCom.h, firmware/ecu/cvc/src/Swc_CvcCom.c | — | firmware/ecu/cvc/test/test_Swc_CvcCom_asild.c | **TRACED** |
| SWR-FZC-016 | firmware/ecu/fzc/include/Swc_Lidar.h, firmware/ecu/fzc/src/Swc_Lidar.c | — | firmware/ecu/fzc/test/test_Swc_FzcSafety_asild.c, firmware/e | **TRACED** |
| SWR-RZC-016 | firmware/ecu/rzc/include/Swc_Motor.h, firmware/ecu/rzc/src/Swc_Motor.c | — | firmware/ecu/rzc/test/test_Swc_Motor_asild.c | **TRACED** |
| SWR-SC-016 | firmware/ecu/sc/include/sc_selftest.h, firmware/ecu/sc/src/sc_selftest.c | — | firmware/ecu/sc/test/test_sc_selftest_asild.c | **TRACED** |
| SWR-BSW-016 | firmware/bsw/services/Com/include/Com.h, firmware/bsw/services/Com/src/Com.c | — | firmware/bsw/test/test_Com_asild.c, test/unit/bsw/test_Com_a | **TRACED** |
| SWR-CVC-017 | firmware/ecu/cvc/include/Swc_CvcCom.h, firmware/ecu/cvc/src/Swc_CvcCom.c | — | firmware/ecu/cvc/test/test_Swc_CvcCom_asild.c, firmware/ecu/ | **TRACED** |
| SWR-FZC-017 | firmware/ecu/fzc/include/Swc_Buzzer.h, firmware/ecu/fzc/src/Swc_Buzzer.c | — | firmware/ecu/fzc/test/test_Swc_Buzzer_qm.c | **TRACED** |
| SWR-RZC-017 | firmware/ecu/rzc/include/Swc_Battery.h, firmware/ecu/rzc/src/Swc_Battery.c | — | firmware/ecu/rzc/test/test_Swc_Battery_qm.c | **TRACED** |
| SWR-SC-017 | firmware/ecu/sc/include/sc_selftest.h, firmware/ecu/sc/src/sc_selftest.c | — | firmware/ecu/sc/test/test_sc_selftest_asild.c | **TRACED** |
| SWR-BSW-017 | firmware/bsw/services/Dcm/include/Dcm.h, firmware/bsw/services/Dcm/src/Dcm.c, fi | — | firmware/bsw/test/test_Dcm_qm.c, firmware/bsw/test/test_Dem_ | **TRACED** |
| SWR-CVC-018 | firmware/ecu/cvc/include/Swc_EStop.h, firmware/ecu/cvc/src/Swc_EStop.c | — | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c | **TRACED** |
| SWR-FZC-018 | firmware/ecu/fzc/include/Swc_Buzzer.h, firmware/ecu/fzc/src/Swc_Buzzer.c | — | firmware/ecu/cvc/test/test_Swc_Heartbeat_asilc.c, firmware/e | **TRACED** |
| SWR-RZC-018 | firmware/ecu/rzc/include/Swc_Battery.h, firmware/ecu/rzc/src/Swc_Battery.c | — | firmware/ecu/rzc/test/test_Swc_Battery_qm.c | **TRACED** |
| SWR-SC-018 | firmware/ecu/sc/include/sc_selftest.h, firmware/ecu/sc/src/sc_selftest.c | — | firmware/ecu/sc/test/test_sc_selftest_asild.c | **TRACED** |
| SWR-BSW-018 | firmware/bsw/services/Dem/include/Dem.h, firmware/bsw/services/Dem/src/Dem.c | — | firmware/bsw/test/test_Dem_asilb.c, test/unit/bsw/test_Dem_a | **TRACED** |
| SWR-FZC-019 | firmware/ecu/fzc/include/Swc_FzcCom.h, firmware/ecu/fzc/src/Swc_FzcCom.c | — | firmware/ecu/fzc/test/test_Swc_FzcCom_asild.c, firmware/ecu/ | **TRACED** |
| SWR-RZC-019 | firmware/ecu/rzc/include/Swc_RzcCom.h, firmware/ecu/rzc/src/Swc_RzcCom.c | — | firmware/ecu/rzc/test/test_Swc_RzcCom_asild.c | **TRACED** |
| SWR-SC-019 | firmware/ecu/sc/include/sc_selftest.h, firmware/ecu/sc/src/sc_selftest.c | — | firmware/ecu/sc/test/test_sc_selftest_asild.c | **TRACED** |
| SWR-CVC-019 | — | — | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c | **TEST ONLY** |
| SWR-BSW-019 | firmware/bsw/services/WdgM/include/WdgM.h, firmware/bsw/services/WdgM/src/WdgM.c | — | firmware/bsw/test/test_WdgM_asild.c, test/unit/bsw/test_Det_ | **TRACED** |
| SWR-CVC-020 | firmware/ecu/cvc/include/Swc_EStop.h, firmware/ecu/cvc/src/Swc_EStop.c | — | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c | **TRACED** |
| SWR-FZC-020 | firmware/ecu/fzc/include/Swc_FzcCom.h, firmware/ecu/fzc/src/Swc_FzcCom.c | — | firmware/ecu/fzc/test/test_Swc_FzcCom_asild.c | **TRACED** |
| SWR-RZC-020 | firmware/ecu/rzc/include/Swc_RzcCom.h, firmware/ecu/rzc/src/Swc_RzcCom.c | — | firmware/ecu/rzc/test/test_Swc_RzcCom_asild.c | **TRACED** |
| SWR-SC-020 | — | — | firmware/ecu/sc/test/test_sc_selftest_asild.c | **TEST ONLY** |
| SWR-BSW-020 | firmware/bsw/services/WdgM/include/WdgM.h, firmware/bsw/services/WdgM/src/WdgM.c | — | firmware/bsw/test/test_WdgM_asild.c, test/unit/bsw/test_WdgM | **TRACED** |
| SWR-CVC-021 | firmware/ecu/cvc/include/Swc_Heartbeat.h, firmware/ecu/cvc/src/Swc_Heartbeat.c | — | firmware/ecu/cvc/test/test_Swc_Heartbeat_asilc.c | **TRACED** |
| SWR-FZC-021 | firmware/ecu/fzc/include/Swc_Heartbeat.h, firmware/ecu/fzc/src/Swc_Heartbeat.c | — | firmware/ecu/fzc/test/test_Swc_Heartbeat_asilc.c | **TRACED** |
| SWR-RZC-021 | firmware/ecu/rzc/include/Swc_Heartbeat.h, firmware/ecu/rzc/src/Swc_Heartbeat.c | — | firmware/ecu/rzc/test/test_Swc_Heartbeat_asilc.c | **TRACED** |
| SWR-SC-021 | — | — | firmware/ecu/sc/test/test_sc_selftest_asild.c | **TEST ONLY** |
| SWR-BSW-021 | — | — | firmware/bsw/test/test_WdgM_asild.c, test/unit/bsw/test_WdgM | **TEST ONLY** |
| SWR-CVC-022 | firmware/ecu/cvc/include/Swc_Heartbeat.h, firmware/ecu/cvc/src/Swc_Heartbeat.c | — | firmware/ecu/cvc/test/test_Swc_Heartbeat_asilc.c | **TRACED** |
| SWR-FZC-022 | firmware/ecu/fzc/include/Swc_Heartbeat.h, firmware/ecu/fzc/src/Swc_Heartbeat.c | — | firmware/ecu/fzc/test/test_Swc_Heartbeat_asilc.c | **TRACED** |
| SWR-RZC-022 | firmware/ecu/rzc/include/Swc_Heartbeat.h, firmware/ecu/rzc/src/Swc_Heartbeat.c | — | firmware/ecu/rzc/test/test_Swc_Heartbeat_asilc.c | **TRACED** |
| SWR-SC-022 | firmware/ecu/sc/include/sc_watchdog.h, firmware/ecu/sc/src/sc_watchdog.c | — | firmware/ecu/sc/test/test_sc_watchdog_asild.c | **TRACED** |
| SWR-BSW-022 | firmware/bsw/services/BswM/include/BswM.h, firmware/bsw/services/BswM/src/BswM.c | — | firmware/bsw/test/test_BswM_asild.c, test/unit/bsw/test_BswM | **TRACED** |
| SWR-CVC-023 | firmware/ecu/cvc/include/Swc_Watchdog.h, firmware/ecu/cvc/src/Swc_Watchdog.c | — | firmware/ecu/cvc/test/test_Swc_Watchdog_asild.c | **TRACED** |
| SWR-FZC-023 | firmware/ecu/fzc/include/Swc_FzcSafety.h, firmware/ecu/fzc/src/Swc_FzcSafety.c | — | firmware/ecu/fzc/test/test_Swc_FzcSafety_asild.c | **TRACED** |
| SWR-RZC-023 | firmware/ecu/rzc/include/Swc_RzcSafety.h, firmware/ecu/rzc/src/Swc_RzcSafety.c | — | firmware/ecu/rzc/test/test_Swc_RzcSafety_asild.c | **TRACED** |
| SWR-SC-023 | firmware/ecu/sc/include/sc_can.h, firmware/ecu/sc/src/sc_can.c | — | firmware/ecu/sc/test/test_sc_can_asild.c | **TRACED** |
| SWR-BSW-023 | firmware/bsw/services/E2E/include/E2E.h, firmware/bsw/services/E2E/src/E2E.c | — | firmware/bsw/test/test_E2E_asild.c, test/sil/scenarios/sil_0 | **TRACED** |
| SWR-CVC-024 | firmware/ecu/cvc/include/Swc_CanMonitor.h, firmware/ecu/cvc/src/Swc_CanMonitor.c | — | firmware/ecu/cvc/test/test_Swc_CanMonitor_asilc.c | **TRACED** |
| SWR-FZC-024 | firmware/ecu/fzc/include/Swc_FzcCanMonitor.h, firmware/ecu/fzc/src/Swc_FzcCanMon | — | firmware/ecu/fzc/test/test_Swc_FzcCanMonitor_asilc.c | **TRACED** |
| SWR-RZC-024 | firmware/ecu/rzc/include/Swc_RzcSafety.h, firmware/ecu/rzc/src/Swc_RzcSafety.c | — | firmware/ecu/rzc/test/test_Swc_RzcSafety_asild.c | **TRACED** |
| SWR-SC-024 | firmware/ecu/sc/include/sc_plausibility.h, firmware/ecu/sc/src/sc_plausibility.c | — | firmware/ecu/sc/test/test_sc_plausibility_asilc.c | **TRACED** |
| SWR-BSW-024 | firmware/bsw/services/E2E/include/E2E.h, firmware/bsw/services/E2E/src/E2E.c | — | firmware/bsw/test/test_E2E_asild.c, test/sil/scenarios/sil_0 | **TRACED** |
| SWR-CVC-025 | firmware/ecu/cvc/include/Swc_CanMonitor.h, firmware/ecu/cvc/src/Swc_CanMonitor.c | — | firmware/ecu/cvc/test/test_Swc_CanMonitor_asilc.c | **TRACED** |
| SWR-FZC-025 | firmware/ecu/fzc/include/Swc_FzcSafety.h, firmware/ecu/fzc/src/Swc_FzcSafety.c,  | — | firmware/ecu/fzc/test/test_Swc_FzcSafety_asild.c | **TRACED** |
| SWR-RZC-025 | firmware/ecu/rzc/include/Swc_RzcSelfTest.h, firmware/ecu/rzc/src/Swc_RzcSelfTest | — | firmware/ecu/rzc/test/test_Swc_RzcSelfTest_asild.c | **TRACED** |
| SWR-SC-025 | firmware/ecu/sc/include/sc_hw.h, firmware/ecu/sc/include/sc_state.h, firmware/ec | — | firmware/ecu/sc/test/test_sc_main_asild.c | **TRACED** |
| SWR-BSW-025 | firmware/bsw/services/E2E/include/E2E.h, firmware/bsw/services/E2E/src/E2E.c | — | firmware/bsw/test/test_E2E_asild.c, test/unit/bsw/test_Com_a | **TRACED** |
| SWR-CVC-026 | firmware/ecu/cvc/include/Ssd1306.h, firmware/ecu/cvc/src/Ssd1306.c | — | firmware/ecu/cvc/test/test_Ssd1306_qm.c | **TRACED** |
| SWR-FZC-026 | firmware/ecu/fzc/include/Swc_FzcCom.h, firmware/ecu/fzc/src/Swc_FzcCom.c | — | firmware/ecu/fzc/test/test_Swc_FzcCom_asild.c | **TRACED** |
| SWR-RZC-026 | firmware/ecu/rzc/include/Swc_RzcCom.h, firmware/ecu/rzc/src/Swc_RzcCom.c | — | firmware/ecu/rzc/test/test_Swc_RzcCom_asild.c | **TRACED** |
| SWR-SC-026 | firmware/ecu/sc/include/Sc_Hw_Cfg.h, firmware/ecu/sc/include/sc_types.h, firmwar | — | firmware/ecu/sc/test/test_sc_main_asild.c | **TRACED** |
| SWR-BSW-026 | firmware/bsw/rte/include/Rte.h, firmware/bsw/rte/src/Rte.c, firmware/bsw/service | — | firmware/bsw/test/test_E2E_Sm_asild.c, firmware/bsw/test/tes | **TRACED** |
| SWR-CVC-027 | firmware/ecu/cvc/include/Swc_Dashboard.h, firmware/ecu/cvc/src/Swc_Dashboard.c | — | firmware/ecu/cvc/test/test_Swc_Dashboard_qm.c | **TRACED** |
| SWR-FZC-027 | firmware/ecu/fzc/include/Swc_FzcCom.h, firmware/ecu/fzc/src/Swc_FzcCom.c | — | firmware/ecu/fzc/test/test_Swc_FzcCom_asild.c | **TRACED** |
| SWR-RZC-027 | firmware/ecu/rzc/include/Swc_RzcCom.h, firmware/ecu/rzc/src/Swc_RzcCom.c | — | firmware/ecu/rzc/test/test_Swc_RzcCom_asild.c | **TRACED** |
| SWR-SC-027 | — | — | firmware/ecu/sc/test/test_sc_heartbeat_asilc.c | **TEST ONLY** |
| SWR-BSW-027 | firmware/bsw/rte/include/Rte.h, firmware/bsw/rte/src/Rte.c, firmware/bsw/service | — | firmware/bsw/test/test_Rte_asild.c, test/unit/bsw/test_Rte_a | **TRACED** |
| SWR-CVC-028 | firmware/ecu/cvc/include/Swc_Dashboard.h, firmware/ecu/cvc/src/Swc_Dashboard.c | — | firmware/ecu/cvc/test/test_Swc_Dashboard_qm.c | **TRACED** |
| SWR-FZC-028 | firmware/ecu/fzc/include/Swc_Steering.h, firmware/ecu/fzc/src/Swc_Steering.c | — | firmware/ecu/fzc/test/test_Swc_Steering_asild.c | **TRACED** |
| SWR-RZC-028 | firmware/ecu/rzc/include/Swc_RzcScheduler.h, firmware/ecu/rzc/src/Swc_RzcSchedul | — | firmware/ecu/rzc/test/test_Swc_RzcScheduler_asild.c | **TRACED** |
| SWR-SC-028 | — | — | firmware/ecu/sc/test/test_sc_heartbeat_asilc.c | **TEST ONLY** |
| SWR-BSW-028 | firmware/bsw/services/SchM/include/SchM_Timing.h | — | test/unit/bsw/test_BswM_asild.c | **TRACED** |
| SWR-CVC-029 | firmware/ecu/cvc/include/Swc_SelfTest.h, firmware/ecu/cvc/src/Swc_SelfTest.c, fi | — | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c, firmware/ecu/c | **TRACED** |
| SWR-FZC-029 | firmware/ecu/fzc/include/Swc_FzcScheduler.h, firmware/ecu/fzc/src/Swc_FzcSchedul | — | firmware/ecu/fzc/test/test_Swc_FzcScheduler_asild.c | **TRACED** |
| SWR-RZC-029 | firmware/ecu/rzc/include/Swc_RzcDcm.h, firmware/ecu/rzc/src/Swc_RzcDcm.c | — | firmware/ecu/rzc/test/test_Swc_RzcDcm_qm.c | **TRACED** |
| SWR-SC-029 | firmware/ecu/sc/include/sc_monitoring.h, firmware/ecu/sc/src/sc_can.c, firmware/ | — | firmware/ecu/sc/test/test_sc_can_asild.c | **TRACED** |
| SWR-CVC-030 | firmware/ecu/cvc/include/Swc_Nvm.h, firmware/ecu/cvc/src/Swc_Nvm.c | firmware/ecu/cvc/cfg/Dcm_Cfg_Cvc.c | firmware/ecu/cvc/test/test_Swc_EStop_asilb.c, firmware/ecu/c | **TRACED** |
| SWR-FZC-030 | firmware/ecu/fzc/include/Swc_FzcDcm.h, firmware/ecu/fzc/src/Swc_FzcDcm.c | — | firmware/ecu/fzc/test/test_Swc_FzcDcm_qm.c | **TRACED** |
| SWR-RZC-030 | firmware/ecu/rzc/include/Rzc_App.h, firmware/ecu/rzc/include/Swc_RzcNvm.h, firmw | firmware/ecu/rzc/cfg/Dcm_Cfg_Rzc.c | firmware/ecu/rzc/test/test_Swc_RzcNvm_asild.c | **TRACED** |
| SWR-SC-030 | firmware/ecu/sc/include/sc_monitoring.h, firmware/ecu/sc/src/sc_monitoring.c | — | firmware/ecu/sc/test/test_sc_can_asild.c | **TRACED** |
| SWR-CVC-031 | firmware/ecu/cvc/include/Swc_Nvm.h, firmware/ecu/cvc/src/Swc_Nvm.c | — | firmware/ecu/cvc/test/test_Swc_Nvm_asild.c | **TRACED** |
| SWR-FZC-031 | firmware/ecu/fzc/include/Swc_FzcNvm.h, firmware/ecu/fzc/src/Swc_FzcNvm.c | — | firmware/ecu/fzc/test/test_Swc_FzcNvm_asild.c | **TRACED** |
| SWR-BSW-031 | firmware/bsw/services/NvM/include/NvM.h, firmware/bsw/services/NvM/src/NvM.c | — | test/unit/bsw/test_Com_asild.c | **TRACED** |
| SWR-CVC-032 | firmware/ecu/cvc/include/Swc_Scheduler.h, firmware/ecu/cvc/src/Swc_Scheduler.c | — | firmware/ecu/cvc/test/test_Swc_Scheduler_asild.c | **TRACED** |
| SWR-FZC-032 | firmware/ecu/fzc/include/Fzc_App.h, firmware/ecu/fzc/include/Swc_FzcNvm.h, firmw | firmware/ecu/fzc/cfg/Dcm_Cfg_Fzc.c | firmware/ecu/fzc/test/test_Swc_FzcNvm_asild.c | **TRACED** |
| SWR-CVC-033 | firmware/ecu/cvc/include/Swc_CvcDcm.h, firmware/ecu/cvc/src/Swc_CvcDcm.c | — | firmware/ecu/cvc/test/test_Swc_CvcDcm_qm.c | **TRACED** |
| SWR-CVC-034 | firmware/ecu/cvc/include/Swc_CvcDcm.h, firmware/ecu/cvc/src/Swc_CvcDcm.c | — | firmware/ecu/cvc/test/test_Swc_CvcDcm_qm.c | **TRACED** |
| SWR-CVC-035 | firmware/ecu/cvc/include/Swc_CvcDcm.h, firmware/ecu/cvc/src/Swc_CvcDcm.c, firmwa | firmware/ecu/cvc/cfg/Dcm_Cfg_Cvc.c | firmware/ecu/cvc/test/test_Swc_CvcDcm_qm.c | **TRACED** |
| SWR-SC-035 | firmware/ecu/sc/include/sc_can.h | — | firmware/ecu/sc/test/test_sc_can_asild.c, firmware/ecu/sc/te | **TRACED** |
| SWR-SC-036 | — | — | firmware/ecu/sc/test/test_sc_relay_asild.c | **TEST ONLY** |
| SWR-BSW-040 | firmware/bsw/services/Det/include/Det.h, firmware/bsw/services/Det/src/Det.c, fi | — | firmware/bsw/test/test_Det_Callout_Sil_asild.c, firmware/bsw | **TRACED** |
| SWR-BSW-041 | firmware/bsw/services/SchM/include/SchM.h, firmware/bsw/services/SchM/src/SchM.c | — | firmware/bsw/test/test_SchM_asild.c, test/unit/bsw/test_SchM | **TRACED** |
| SWR-BSW-042 | firmware/bsw/services/CanTp/include/CanTp.h, firmware/bsw/services/CanTp/src/Can | — | firmware/bsw/test/test_CanTp_asild.c, test/unit/bsw/test_Can | **TRACED** |
| SWR-BSW-050 | firmware/bsw/os/bootstrap/include/Os.h | — | test/unit/bsw/test_Com_asild.c | **TRACED** |
| TSR-001 | firmware/bsw/mcal/Spi/include/Spi.h, firmware/bsw/mcal/Spi/src/Spi.c, firmware/b | — | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| TSR-005 | firmware/bsw/mcal/Dio/include/Dio.h, firmware/bsw/mcal/Dio/src/Dio.c, firmware/b | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-006 | firmware/bsw/mcal/Adc/include/Adc.h, firmware/bsw/mcal/Adc/src/Adc.c | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-008 | firmware/bsw/mcal/Adc/include/Adc.h, firmware/bsw/mcal/Adc/src/Adc.c | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-010 | firmware/bsw/mcal/Spi/include/Spi.h, firmware/bsw/mcal/Spi/src/Spi.c | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-012 | firmware/bsw/mcal/Pwm/include/Pwm.h, firmware/bsw/mcal/Pwm/src/Pwm.c | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-015 | firmware/bsw/mcal/Uart/include/Uart.h, firmware/bsw/mcal/Uart/src/Uart.c | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-022 | firmware/bsw/ecual/CanIf/include/CanIf.h, firmware/bsw/ecual/CanIf/src/CanIf.c,  | — | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| TSR-023 | firmware/bsw/ecual/CanIf/include/CanIf.h, firmware/bsw/ecual/CanIf/src/CanIf.c,  | — | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| TSR-024 | firmware/bsw/ecual/CanIf/include/CanIf.h, firmware/bsw/ecual/CanIf/src/CanIf.c,  | — | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| TSR-030 | firmware/bsw/ecual/IoHwAb/include/IoHwAb.h, firmware/bsw/ecual/IoHwAb/include/Io | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-031 | firmware/bsw/ecual/IoHwAb/include/IoHwAb.h, firmware/bsw/ecual/IoHwAb/include/Io | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-032 | firmware/bsw/ecual/IoHwAb/include/IoHwAb.h, firmware/bsw/ecual/IoHwAb/src/IoHwAb | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-033 | firmware/bsw/mcal/Dio/include/Dio.h, firmware/bsw/mcal/Dio/src/Dio.c | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-035 | firmware/bsw/rte/include/Rte.h, firmware/bsw/rte/src/Rte.c, firmware/ecu/tcu/src | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-038 | firmware/bsw/ecual/CanIf/include/CanIf.h, firmware/bsw/mcal/Can/include/Can.h, f | firmware/ecu/cvc/cfg/Dcm_Cfg_Cvc.c, firmware/ecu/fzc/cfg/Dcm | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| TSR-039 | firmware/bsw/mcal/Can/include/Can.h, firmware/bsw/mcal/Can/src/Can.c, firmware/b | firmware/ecu/cvc/cfg/Dcm_Cfg_Cvc.c, firmware/ecu/fzc/cfg/Dcm | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-040 | firmware/bsw/services/CanTp/include/CanTp.h, firmware/bsw/services/CanTp/src/Can | firmware/ecu/cvc/cfg/Dcm_Cfg_Cvc.c, firmware/ecu/fzc/cfg/Dcm | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
| TSR-046 | firmware/bsw/mcal/Gpt/include/Gpt.h, firmware/bsw/mcal/Gpt/src/Gpt.c, firmware/b | — | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| TSR-047 | firmware/bsw/mcal/Gpt/include/Gpt.h, firmware/bsw/mcal/Gpt/src/Gpt.c, firmware/b | — | test/sil/scenarios/sil_001_normal_startup.yaml, test/sil/sce | **TRACED** |
| TSR-048 | firmware/bsw/services/BswM/include/BswM.h, firmware/bsw/services/BswM/src/BswM.c | — | test/sil/scenarios/sil_005_watchdog_timeout_cvc.yaml, test/s | **TRACED** |
| TSR-050 | firmware/bsw/services/NvM/include/NvM.h, firmware/bsw/services/NvM/src/NvM.c | — | test/sil/scenarios/sil_001_normal_startup.yaml | **TRACED** |
