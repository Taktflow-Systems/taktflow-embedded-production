# arxmlgen Test Report

**Date:** 2026-03-10
**Version:** arxmlgen v1.0.0
**Python:** 3.14.3, pytest 9.0.2
**Platform:** win32
**Result:** 242/242 PASSED (0 failed, 0 errors)

## Test Suite Summary

| Test File | Tests | Category | Status |
|-----------|-------|----------|--------|
| `test_model_integrity.py` | 24 | ARXML model validation | ALL PASS |
| `test_quality.py` | 32 | Professional parity + golden file comparison | ALL PASS |
| `test_com_generator.py` | 25 | Com_Cfg.c generator output | ALL PASS |
| `test_rte_generator.py` | 25 | Rte_Cfg.c generator output | ALL PASS |
| `test_cfg_header_generator.py` | 25 | Ecu_Cfg.h generator output | ALL PASS |
| `test_rte_wrapper_generator.py` | 29 | Per-SWC typed RTE wrappers | ALL PASS |
| `test_canif_generator.py` | 23 | CanIf_Cfg.c generator output | ALL PASS |
| `test_pdur_generator.py` | 21 | PduR_Cfg.c generator output | ALL PASS |
| `test_e2e_generator.py` | 23 | E2E_Cfg.c generator output | ALL PASS |
| `test_swc_skeleton_generator.py` | 15 | SWC skeleton .c/.h generation | ALL PASS |
| **Total** | **242** | | **ALL PASS** |

## Phase 1: Model Integrity (24 tests)

Tests that the ARXML reader produces a correct internal model.

| Test | Verdict |
|------|---------|
| `TestSignalInvariants::test_signal_bit_size_nonzero` | PASS |
| `TestSignalInvariants::test_signal_fits_in_pdu` | PASS |
| `TestSignalInvariants::test_no_duplicate_signal_names_per_pdu` | PASS |
| `TestSignalInvariants::test_signal_type_is_valid` | PASS |
| `TestPduInvariants::test_pdu_dlc_range` | PASS |
| `TestPduInvariants::test_pdu_has_can_id` | PASS |
| `TestPduInvariants::test_can_id_range` | PASS |
| `TestPduInvariants::test_no_duplicate_tx_can_ids_per_ecu` | PASS |
| `TestPduInvariants::test_pdu_has_at_least_one_signal` | PASS |
| `TestPduInvariants::test_pdu_id_uniqueness` | PASS |
| `TestEcuInvariants::test_all_ecus_present` | PASS |
| `TestEcuInvariants::test_ecu_has_prefix` | PASS |
| `TestEcuInvariants::test_ecu_prefix_is_uppercase` | PASS |
| `TestEcuInvariants::test_ecu_has_tx_pdus` | PASS |
| `TestSwcInvariants::test_swc_has_name` | PASS |
| `TestSwcInvariants::test_runnable_has_positive_period` | PASS |
| `TestSwcInvariants::test_no_duplicate_runnable_names_per_ecu` | PASS |
| `TestCrossEcuInvariants::test_no_two_ecus_tx_same_can_id` | PASS |
| `TestCrossEcuInvariants::test_total_signal_count` | PASS |
| `TestCrossEcuInvariants::test_e2e_pdus_have_data_id` | PASS |
| `TestSidecarInvariants::test_cvc_has_dtc_events` | PASS |
| `TestSidecarInvariants::test_cvc_dtc_count` | PASS |
| `TestSidecarInvariants::test_cvc_has_enums` | PASS |
| `TestSidecarInvariants::test_cvc_has_thresholds` | PASS |

## Phase 1: Quality Verification (32 tests)

Tests comparing arxmlgen output against professional tool (DaVinci) reference files.

| Test | Verdict |
|------|---------|
| `TestStructuralCompleteness::test_com_cfg_h_exists` | PASS |
| `TestStructuralCompleteness::test_com_cfg_c_exists` | PASS |
| `TestStructuralCompleteness::test_rte_cfg_c_exists` | PASS |
| `TestStructuralCompleteness::test_canif_cfg_c_exists` | PASS |
| `TestStructuralCompleteness::test_bcm_cfg_h_exists` | PASS |
| `TestStructuralCompleteness::test_all_files_have_generated_marker` | PASS |
| `TestStructuralCompleteness::test_headers_have_guards` | PASS |
| `TestDataIntegrity::test_com_signal_count` | PASS |
| `TestDataIntegrity::test_com_tx_pdu_count` | PASS |
| `TestDataIntegrity::test_com_rx_pdu_count` | PASS |
| `TestDataIntegrity::test_rte_signal_count` | PASS |
| `TestDataIntegrity::test_runnable_count` | PASS |
| `TestDataIntegrity::test_bsw_signal_offset` | PASS |
| `TestDataIntegrity::test_tx_can_ids_present` | PASS |
| `TestDataIntegrity::test_rx_can_ids_present` | PASS |
| `TestDataIntegrity::test_signal_array_count_matches_define` | PASS |
| `TestDataIntegrity::test_runnable_array_count_matches_define` | PASS |
| `TestCrossModuleConsistency::test_canif_tx_count_matches_com` | PASS |
| `TestCrossModuleConsistency::test_canif_rx_count_matches_com` | PASS |
| `TestCrossModuleConsistency::test_rte_signal_array_matches_count` | PASS |
| `TestCrossModuleConsistency::test_tx_can_ids_in_both_canif_and_com` | PASS |
| `TestProfessionalParity::test_no_magic_numbers_in_pdu_config` | PASS |
| `TestProfessionalParity::test_shadow_buffers_are_static` | PASS |
| `TestProfessionalParity::test_config_structs_are_const` | PASS |
| `TestProfessionalParity::test_signal_bit_positions_no_overlap` | PASS |
| `TestModelVsProfessional::test_bcm_signal_count` | PASS |
| `TestModelVsProfessional::test_bcm_tx_pdu_count` | PASS |
| `TestModelVsProfessional::test_bcm_rx_pdu_count` | PASS |
| `TestModelVsProfessional::test_bcm_tx_can_ids` | PASS |
| `TestModelVsProfessional::test_bcm_rx_can_ids` | PASS |
| `TestModelVsProfessional::test_bcm_swc_count` | PASS |
| `TestModelVsProfessional::test_all_ecus_have_heartbeat_can_id` | PASS |

## Phase 2: Com_Cfg Generator (25 tests)

Tests that `ComCfgGenerator` produces valid `Com_Cfg_*.c` for all 7 ECUs.

| Test | Verdict |
|------|---------|
| `TestComCfgCStructure::test_has_generated_header` | PASS |
| `TestComCfgCStructure::test_includes_com_h` | PASS |
| `TestComCfgCStructure::test_includes_ecu_cfg_h` | PASS |
| `TestComCfgCStructure::test_has_shadow_buffers` | PASS |
| `TestComCfgCStructure::test_has_signal_config_array` | PASS |
| `TestComCfgCStructure::test_has_tx_pdu_config_array` | PASS |
| `TestComCfgCStructure::test_has_rx_pdu_config_array` | PASS |
| `TestComCfgCStructure::test_has_aggregate_config` | PASS |
| `TestComCfgCStructure::test_signal_count_define` | PASS |
| `TestComCfgCStructure::test_tx_pdu_count_define` | PASS |
| `TestComCfgCStructure::test_rx_pdu_count_define` | PASS |
| `TestComCfgCData::test_bcm_tx_signal_count` | PASS |
| `TestComCfgCData::test_bcm_tx_pdu_count` | PASS |
| `TestComCfgCData::test_bcm_rx_pdu_count` | PASS |
| `TestComCfgCData::test_shadow_buffers_are_static` | PASS |
| `TestComCfgCData::test_signal_config_is_const` | PASS |
| `TestComCfgCData::test_tx_pdu_config_is_const` | PASS |
| `TestComCfgCData::test_rx_pdu_config_is_const` | PASS |
| `TestComCfgCData::test_aggregate_config_is_const_not_static` | PASS |
| `TestComCfgCData::test_signal_ids_are_sequential` | PASS |
| `TestComCfgCData::test_com_type_matches_bit_size` | PASS |
| `TestComCfgCvc::test_cvc_has_at_least_as_many_signals_as_bcm` | PASS |
| `TestComCfgCvc::test_cvc_includes_cvc_cfg_h` | PASS |
| `TestComCfgCvc::test_cvc_aggregate_uses_cvc_prefix` | PASS |
| `TestComCfgAllEcus::test_all_com_ecus_have_output` | PASS |
| `TestComCfgAllEcus::test_each_ecu_has_unique_prefix` | PASS |
| `TestComCfgAllEcus::test_no_ecu_has_zero_signals` | PASS |

## Phase 2: Rte_Cfg Generator (25 tests)

Tests that `RteCfgGenerator` produces valid `Rte_Cfg_*.c` for all 7 ECUs.

| Test | Verdict |
|------|---------|
| `TestRteCfgCStructure::test_has_generated_header` | PASS |
| `TestRteCfgCStructure::test_includes_rte_h` | PASS |
| `TestRteCfgCStructure::test_includes_ecu_cfg_h` | PASS |
| `TestRteCfgCStructure::test_has_extern_declarations` | PASS |
| `TestRteCfgCStructure::test_has_signal_config_array` | PASS |
| `TestRteCfgCStructure::test_has_runnable_config_array` | PASS |
| `TestRteCfgCStructure::test_has_aggregate_config` | PASS |
| `TestRteCfgSignals::test_starts_with_16_bsw_signals` | PASS |
| `TestRteCfgSignals::test_bsw_signals_have_zero_init` | PASS |
| `TestRteCfgSignals::test_ecu_signals_start_at_16` | PASS |
| `TestRteCfgSignals::test_signal_count_matches_model` | PASS |
| `TestRteCfgSignals::test_signal_array_size_uses_sig_count` | PASS |
| `TestRteCfgRunnables::test_runnable_count_matches_model` | PASS |
| `TestRteCfgRunnables::test_runnables_have_function_pointers` | PASS |
| `TestRteCfgRunnables::test_runnables_sorted_by_priority_descending` | PASS |
| `TestRteCfgRunnables::test_runnable_period_is_non_negative` | PASS |
| `TestRteCfgRunnables::test_each_extern_has_matching_runnable` | PASS |
| `TestRteCfgAggregate::test_aggregate_is_const` | PASS |
| `TestRteCfgAggregate::test_aggregate_has_signal_config` | PASS |
| `TestRteCfgAggregate::test_aggregate_has_signal_count` | PASS |
| `TestRteCfgAggregate::test_aggregate_has_runnable_config` | PASS |
| `TestRteCfgAggregate::test_aggregate_has_runnable_count` | PASS |
| `TestRteCfgAllEcus::test_all_rte_ecus_have_output` | PASS |
| `TestRteCfgAllEcus::test_each_ecu_uses_own_prefix` | PASS |
| `TestRteCfgAllEcus::test_cvc_has_more_runnables_than_icu` | PASS |

## Phase 2: Ecu_Cfg.h Generator (25 tests)

Tests that `CfgHeaderGenerator` produces valid `*_Cfg.h` for all 7 ECUs.

| Test | Verdict |
|------|---------|
| `TestCfgHeaderStructure::test_has_generated_header` | PASS |
| `TestCfgHeaderStructure::test_has_header_guard` | PASS |
| `TestCfgHeaderStructure::test_cvc_has_header_guard` | PASS |
| `TestCfgHeaderRteSignals::test_bcm_signals_start_at_16` | PASS |
| `TestCfgHeaderRteSignals::test_bcm_has_sig_count` | PASS |
| `TestCfgHeaderRteSignals::test_sig_count_matches_model` | PASS |
| `TestCfgHeaderRteSignals::test_signal_ids_are_sequential` | PASS |
| `TestCfgHeaderRteSignals::test_cvc_has_at_least_as_many_signals_as_bcm` | PASS |
| `TestCfgHeaderComPdus::test_bcm_tx_pdus_start_at_0` | PASS |
| `TestCfgHeaderComPdus::test_bcm_rx_pdus_start_at_0` | PASS |
| `TestCfgHeaderComPdus::test_tx_pdu_count_matches_model` | PASS |
| `TestCfgHeaderComPdus::test_rx_pdu_count_matches_model` | PASS |
| `TestCfgHeaderComPdus::test_pdu_defines_have_can_id_comment` | PASS |
| `TestCfgHeaderDtcs::test_cvc_has_dtc_defines` | PASS |
| `TestCfgHeaderDtcs::test_cvc_dtc_count_matches_sidecar` | PASS |
| `TestCfgHeaderDtcs::test_bcm_has_no_dtc_defines` | PASS |
| `TestCfgHeaderE2e::test_cvc_has_e2e_defines` | PASS |
| `TestCfgHeaderE2e::test_cvc_e2e_count_matches_sidecar` | PASS |
| `TestCfgHeaderEnumsThresholds::test_cvc_has_enum_defines` | PASS |
| `TestCfgHeaderEnumsThresholds::test_cvc_has_threshold_defines` | PASS |
| `TestCfgHeaderEnumsThresholds::test_bcm_has_threshold_defines` | PASS |
| `TestCfgHeaderAllEcus::test_all_ecus_have_output` | PASS |
| `TestCfgHeaderAllEcus::test_each_ecu_uses_own_prefix_in_guard` | PASS |

## Phase 3: Per-SWC Typed RTE Wrappers (29 tests)

Tests that `RteCfgGenerator.render_wrappers()` produces valid `Rte_Swc_*.h` for all SWCs.

| Test | Verdict |
|------|---------|
| `TestRteWrapperStructure::test_cvc_has_wrapper_files` | PASS |
| `TestRteWrapperStructure::test_wrapper_filenames_match_swc_names` | PASS |
| `TestRteWrapperStructure::test_has_generated_header` | PASS |
| `TestRteWrapperStructure::test_has_header_guard` | PASS |
| `TestRteWrapperStructure::test_includes_rte_h` | PASS |
| `TestRteWrapperStructure::test_includes_ecu_cfg_h` | PASS |
| `TestRteWrapperStructure::test_bcm_includes_bcm_cfg_h` | PASS |
| `TestRteWrapperReads::test_has_read_wrappers` | PASS |
| `TestRteWrapperReads::test_read_is_static_inline` | PASS |
| `TestRteWrapperReads::test_read_returns_rte_status` | PASS |
| `TestRteWrapperReads::test_read_takes_typed_pointer` | PASS |
| `TestRteWrapperReads::test_read_calls_rte_read` | PASS |
| `TestRteWrapperReads::test_read_casts_from_uint32` | PASS |
| `TestRteWrapperWrites::test_has_write_wrappers` | PASS |
| `TestRteWrapperWrites::test_write_is_static_inline` | PASS |
| `TestRteWrapperWrites::test_write_returns_rte_status` | PASS |
| `TestRteWrapperWrites::test_write_takes_typed_value` | PASS |
| `TestRteWrapperWrites::test_write_calls_rte_write` | PASS |
| `TestRteWrapperWrites::test_write_casts_to_uint32` | PASS |
| `TestRteWrapperTypes::test_port_types_not_all_uint32` | PASS |
| `TestRteWrapperTypes::test_port_type_matches_signal_type` | PASS |
| `TestRteWrapperScoping::test_swc_wrapper_count_matches_swc_count` | PASS |
| `TestRteWrapperScoping::test_each_swc_has_both_read_and_write` | PASS |
| `TestRteWrapperScoping::test_wrapper_signal_ids_use_ecu_prefix` | PASS |
| `TestRteWrapperScoping::test_bcm_wrapper_uses_bcm_prefix` | PASS |
| `TestRteWrapperAllEcus::test_all_rte_ecus_have_wrappers` | PASS |
| `TestRteWrapperAllEcus::test_fzc_has_more_swcs_than_bcm` | PASS |
| `TestRteWrapperAllEcus::test_no_ecu_has_zero_wrappers` | PASS |
| `TestRteWrapperAllEcus::test_total_wrapper_count` | PASS |

## Phase 4: CanIf_Cfg Generator (23 tests)

Tests that `CanIfCfgGenerator` produces valid `CanIf_Cfg_*.c` for all 7 ECUs (including SC).

| Test | Verdict |
|------|---------|
| `TestCanIfCfgStructure::test_has_generated_header` | PASS |
| `TestCanIfCfgStructure::test_includes_canif_h` | PASS |
| `TestCanIfCfgStructure::test_includes_ecu_cfg_h` | PASS |
| `TestCanIfCfgStructure::test_has_tx_pdu_config_array` | PASS |
| `TestCanIfCfgStructure::test_has_rx_pdu_config_array` | PASS |
| `TestCanIfCfgStructure::test_has_aggregate_config` | PASS |
| `TestCanIfCfgStructure::test_tx_config_is_static_const` | PASS |
| `TestCanIfCfgStructure::test_rx_config_is_static_const` | PASS |
| `TestCanIfCfgStructure::test_aggregate_is_const_not_static` | PASS |
| `TestCanIfCfgData::test_tx_pdu_count_matches_model` | PASS |
| `TestCanIfCfgData::test_rx_pdu_count_matches_model` | PASS |
| `TestCanIfCfgData::test_can_ids_in_hex` | PASS |
| `TestCanIfCfgData::test_tx_can_ids_match_model` | PASS |
| `TestCanIfCfgData::test_rx_can_ids_match_model` | PASS |
| `TestCanIfCfgData::test_dlc_values_present` | PASS |
| `TestCanIfCfgData::test_tx_pdu_count_define` | PASS |
| `TestCanIfCfgData::test_rx_pdu_count_define` | PASS |
| `TestCanIfCfgData::test_upper_pdu_ids_reference_com_defines` | PASS |
| `TestCanIfCfgAllEcus::test_all_canif_ecus_have_output` | PASS |
| `TestCanIfCfgAllEcus::test_each_ecu_has_own_prefix` | PASS |
| `TestCanIfCfgAllEcus::test_sc_has_canif_output` | PASS |
| `TestCanIfCfgAllEcus::test_cvc_has_at_least_as_many_pdus_as_bcm` | PASS |
| `TestCanIfCfgAllEcus::test_no_ecu_has_empty_tx_and_rx` | PASS |

## Phase 4: PduR_Cfg Generator (21 tests)

Tests that `PduRCfgGenerator` produces valid `PduR_Cfg_*.c` for 6 ECUs (not SC).

| Test | Verdict |
|------|---------|
| `TestPduRCfgStructure::test_has_generated_header` | PASS |
| `TestPduRCfgStructure::test_includes_pdur_h` | PASS |
| `TestPduRCfgStructure::test_includes_ecu_cfg_h` | PASS |
| `TestPduRCfgStructure::test_has_rx_routing_table` | PASS |
| `TestPduRCfgStructure::test_has_tx_routing_table` | PASS |
| `TestPduRCfgStructure::test_has_aggregate_config` | PASS |
| `TestPduRCfgStructure::test_rx_routing_is_static_const` | PASS |
| `TestPduRCfgStructure::test_tx_routing_is_static_const` | PASS |
| `TestPduRCfgStructure::test_aggregate_is_const_not_static` | PASS |
| `TestPduRCfgData::test_rx_route_count_matches_rx_pdus` | PASS |
| `TestPduRCfgData::test_tx_route_count_matches_tx_pdus` | PASS |
| `TestPduRCfgData::test_rx_routes_use_canif_to_com` | PASS |
| `TestPduRCfgData::test_tx_routes_use_com_to_canif` | PASS |
| `TestPduRCfgData::test_rx_routes_reference_com_rx_defines` | PASS |
| `TestPduRCfgData::test_tx_routes_reference_com_tx_defines` | PASS |
| `TestPduRCfgData::test_rx_route_count_define` | PASS |
| `TestPduRCfgData::test_tx_route_count_define` | PASS |
| `TestPduRCfgAllEcus::test_pdur_ecus_have_output` | PASS |
| `TestPduRCfgAllEcus::test_sc_not_in_pdur` | PASS |
| `TestPduRCfgAllEcus::test_each_ecu_has_own_prefix` | PASS |
| `TestPduRCfgAllEcus::test_cvc_has_at_least_as_many_routes_as_bcm` | PASS |

## Phase 5: E2E_Cfg Generator (23 tests)

Tests that `E2ECfgGenerator` produces valid `E2E_Cfg_*.c` for E2E-enabled ECUs.

| Test | Verdict |
|------|---------|
| `TestE2ECfgStructure::test_has_generated_header` | PASS |
| `TestE2ECfgStructure::test_includes_e2e_h` | PASS |
| `TestE2ECfgStructure::test_includes_ecu_cfg_h` | PASS |
| `TestE2ECfgStructure::test_defines_pdu_protect_struct` | PASS |
| `TestE2ECfgStructure::test_has_tx_protect_config` | PASS |
| `TestE2ECfgStructure::test_has_rx_protect_config` | PASS |
| `TestE2ECfgStructure::test_has_aggregate_config` | PASS |
| `TestE2ECfgStructure::test_tx_config_is_static_const` | PASS |
| `TestE2ECfgStructure::test_rx_config_is_static_const` | PASS |
| `TestE2ECfgStructure::test_aggregate_is_const_not_static` | PASS |
| `TestE2ECfgData::test_tx_protected_pdu_count` | PASS |
| `TestE2ECfgData::test_rx_protected_pdu_count` | PASS |
| `TestE2ECfgData::test_data_ids_in_hex` | PASS |
| `TestE2ECfgData::test_tx_entries_reference_com_tx_defines` | PASS |
| `TestE2ECfgData::test_rx_entries_reference_com_rx_defines` | PASS |
| `TestE2ECfgData::test_data_ids_match_pdu_model` | PASS |
| `TestE2ECfgData::test_tx_count_define` | PASS |
| `TestE2ECfgData::test_rx_count_define` | PASS |
| `TestE2ECfgData::test_cvc_has_protected_pdus` | PASS |
| `TestE2ECfgAllEcus::test_e2e_ecus_have_output` | PASS |
| `TestE2ECfgAllEcus::test_sc_excluded_from_e2e` | PASS |
| `TestE2ECfgAllEcus::test_each_ecu_has_own_prefix` | PASS |
| `TestE2ECfgAllEcus::test_fzc_has_protected_pdus` | PASS |

## Phase 6: SWC Skeleton Generator (15 tests)

Tests that `SwcSkeletonGenerator` produces valid `Swc_*.c` and `Swc_*.h` skeletons.

| Test | Verdict |
|------|---------|
| `TestSwcSkeletonNaming::test_cvc_has_swc_files` | PASS |
| `TestSwcSkeletonNaming::test_each_swc_has_c_and_h` | PASS |
| `TestSwcSkeletonNaming::test_filenames_match_swc_names` | PASS |
| `TestSwcSkeletonHeader::test_has_header_guard` | PASS |
| `TestSwcSkeletonHeader::test_has_function_declarations` | PASS |
| `TestSwcSkeletonHeader::test_has_generated_banner` | PASS |
| `TestSwcSkeletonSource::test_includes_own_header` | PASS |
| `TestSwcSkeletonSource::test_includes_rte_wrapper` | PASS |
| `TestSwcSkeletonSource::test_has_runnable_function_bodies` | PASS |
| `TestSwcSkeletonSource::test_init_runnable_has_init_comment` | PASS |
| `TestSwcSkeletonSource::test_periodic_runnable_has_period_comment` | PASS |
| `TestSwcSkeletonAllEcus::test_swc_ecus_have_output` | PASS |
| `TestSwcSkeletonAllEcus::test_sc_excluded` | PASS |
| `TestSwcSkeletonAllEcus::test_total_file_count` | PASS |
| `TestSwcSkeletonAllEcus::test_bcm_has_fewer_swcs_than_fzc` | PASS |

## Notes

- Broadcast CAN: all non-sender ECUs receive all messages, so signal/PDU counts are equal across ECUs (162 signals, 31 RX PDUs per ECU)
- Init runnables have `period_ms=0` (event-triggered), which is valid
- RTE signal IDs: 0-15 reserved for BSW, 16+ for ECU-specific application signals
- TX/RX PDU IDs are numbered separately per ECU, starting at 0
- E2E data IDs propagated across all ECUs via global `pdu_e2e_map` (handles broadcast CAN)
- Port types resolved by stripping PDU name prefix from CAN signal names (e.g., "Vehicle_State_VehicleState" matches port "VehicleState")
- Per-SWC wrapper headers use `static inline` functions (industry standard per AUTOSAR SWS_RTE)
- CanIf PDU IDs mirror Com PDU IDs 1:1 — templates reference ECU_COM_TX_*/ECU_COM_RX_* defines directly
- PduR routing is a straight pass-through (CanIf ↔ Com) since PDU IDs are mirrored
- SC (Safety Controller) gets cfg + canif + e2e — no Com, Rte, PduR, or SWC (safety monitor needs PDU defines, CAN routing, and E2E verification)
- E2E config generated for all 7 ECUs — SC must verify CRC/alive counters on incoming safety-critical messages (ISO 26262)
- E2E uses simplified AUTOSAR: no config struct in E2E.h, so E2E_Cfg.c defines E2E_PduProtectCfgType and E2E_ConfigType inline
- SWC skeletons use generate-if-absent (`overwrite=False`) — never overwrite application code
- SWC .c files include per-SWC typed RTE wrapper (`Rte_{SwcName}.h`) for port access
