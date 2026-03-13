---
document_id: ITR
title: "Integration Test Report"
version: "1.0"
status: executed
aspice_process: "SWE.5"
iso_reference: "ISO 26262 Part 6, Section 10"
date: 2026-02-25
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


# Integration Test Report

## 1. Executive Summary

This report documents the results of software integration testing for the Taktflow Zonal Vehicle Platform BSW stack, per Automotive SPICE 4.0 SWE.5 and ISO 26262:2018 Part 6, Section 10.

**60 integration test cases** across **11 test files** verify inter-module interfaces in the BSW communication stack, safety supervision chain, diagnostic chain, E2E protection, CAN message matrix conformance, and CAN bus-off recovery.

| Metric | Value |
|--------|-------|
| Total integration test cases | 60 |
| Test files | 11 |
| BSW modules exercised | 12 (E2E, Com, PduR, CanIf, Can, Dcm, Dem, WdgM, BswM, Rte, Dio, Gpt) |
| Module pair interfaces verified | 15 |
| Safety paths covered | 15 |
| SWR-BSW requirements covered | 15 |
| Status | **PASS — 11/11 suites, 60/60 cases (CI run 22393954056, 2026-02-25)** |

## 2. Test Environment

| Component | Version / Detail |
|-----------|-----------------|
| Host platform | x86-64, POSIX (Linux CI / MinGW local) |
| Compiler | GCC (CI: Ubuntu apt; Local: 13.x/14.x) |
| Test framework | Unity 2.6.0 (vendored) |
| CAN hardware | Mocked (Can_Write captures TX frames) |
| HAL layer | Mocked (Can_Hw_*, Dio_Hw_*, Dio_FlipChannel) |
| Build system | GNU Make (`test/integration/Makefile`) |
| Test execution command | `make -C test/integration test` |

## 3. Test Results

### 3.1 Results Summary

| Status | Count |
|--------|-------|
| PASS | 60 |
| FAIL | 0 |
| SKIP | 0 |
| BLOCKED | 0 |
| **Total** | **60** |

> **CI run 22393954056 executed 2026-02-25. All 60 test cases passed across 11 suites.**

### 3.2 Detailed Results by Test File

#### INT-003: E2E Protection Chain (`test_int_e2e_chain.c`)

| # | Test Function | Result | Duration |
|---|---------------|--------|----------|
| 1 | `test_int_e2e_protect_tx_rx_check_roundtrip` | PASS | -- |
| 2 | `test_int_e2e_roundtrip_counter_increments` | PASS | -- |
| 3 | `test_int_e2e_roundtrip_corrupted_crc_detected` | PASS | -- |
| 4 | `test_int_e2e_roundtrip_data_id_mismatch` | PASS | -- |
| 5 | `test_int_e2e_full_stack_data_preserved` | PASS | -- |

**Subtotal**: 5/5 PASS

#### INT-004: DEM to DCM Diagnostic Chain (`test_int_dem_to_dcm.c`)

| # | Test Function | Result | Duration |
|---|---------------|--------|----------|
| 1 | `test_int_dem_report_then_dcm_read_status` | PASS | -- |
| 2 | `test_int_dem_clear_then_dcm_reads_clean` | PASS | -- |
| 3 | `test_int_dcm_uds_session_switch` | PASS | -- |
| 4 | `test_int_dcm_unknown_sid_nrc` | PASS | -- |
| 5 | `test_int_dcm_response_routes_through_pdur` | PASS | -- |

**Subtotal**: 5/5 PASS

#### INT-005: WdgM Supervision Chain (`test_int_wdgm_supervision.c`)

| # | Test Function | Result | Duration |
|---|---------------|--------|----------|
| 1 | `test_int_wdgm_ok_feeds_watchdog` | PASS | -- |
| 2 | `test_int_wdgm_missed_checkpoint_fails` | PASS | -- |
| 3 | `test_int_wdgm_expired_triggers_dem` | PASS | -- |
| 4 | `test_int_wdgm_expired_then_bswm_safe_stop` | PASS | -- |
| 5 | `test_int_wdgm_recovery_after_failed` | PASS | -- |
| 6 | `test_int_wdgm_multiple_se_one_fails` | PASS | -- |
| 7 | `test_int_wdgm_dem_occurrence_counter` | PASS | -- |

**Subtotal**: 7/7 PASS

#### INT-006: BswM Mode Transitions (`test_int_bswm_mode.c`)

| # | Test Function | Result | Duration |
|---|---------------|--------|----------|
| 1 | `test_int_bswm_startup_to_run_callback` | PASS | -- |
| 2 | `test_int_bswm_degraded_callback` | PASS | -- |
| 3 | `test_int_bswm_safe_stop_callback` | PASS | -- |
| 4 | `test_int_bswm_multiple_actions_per_mode` | PASS | -- |
| 5 | `test_int_bswm_invalid_backward_transition` | PASS | -- |
| 6 | `test_int_bswm_full_lifecycle` | PASS | -- |
| 7 | `test_int_bswm_shutdown_terminal` | PASS | -- |

**Subtotal**: 7/7 PASS

#### INT-007: Critical Fault to Safe State (`test_int_safe_state.c`)

| # | Test Function | Result | Duration |
|---|---------------|--------|----------|
| 1 | `test_int_fault_to_safe_state_chain` | PASS | -- |
| 2 | `test_int_safe_state_zeros_actuators` | PASS | -- |
| 3 | `test_int_wdgm_expiry_triggers_safe_stop` | PASS | -- |
| 4 | `test_int_safe_stop_to_shutdown_only` | PASS | -- |
| 5 | `test_int_dem_records_fault_before_safe_state` | PASS | -- |

**Subtotal**: 5/5 PASS

#### INT-008: Heartbeat Loss (`test_int_heartbeat_loss.c`)

| # | Test Function | Result | Duration |
|---|---------------|--------|----------|
| 1 | `test_int_heartbeat_present_system_ok` | PASS | -- |
| 2 | `test_int_heartbeat_timeout_triggers_degraded` | PASS | -- |
| 3 | `test_int_heartbeat_timeout_dem_event` | PASS | -- |
| 4 | `test_int_heartbeat_recovery_from_degraded` | PASS | -- |
| 5 | `test_int_dual_heartbeat_loss_safe_stop` | PASS | -- |

**Subtotal**: 5/5 PASS

#### INT-010: Overcurrent Chain (`test_int_overcurrent_chain.c`)

| # | Test Function | Result | Duration |
|---|---------------|--------|----------|
| 1 | `test_int_overcurrent_detected_motor_shutdown` | PASS | -- |
| 2 | `test_int_overcurrent_dem_event_stored` | PASS | -- |
| 3 | `test_int_overcurrent_triggers_safe_stop` | PASS | -- |
| 4 | `test_int_normal_current_no_action` | PASS | -- |
| 5 | `test_int_overcurrent_threshold_boundary` | PASS | -- |

**Subtotal**: 5/5 PASS

#### INT-011: CAN Message Matrix (`test_int_can_matrix.c`)

| # | Test Function | Result | Duration |
|---|---------------|--------|----------|
| 1 | `test_int_can_matrix_tx_ids_correct` | PASS | -- |
| 2 | `test_int_can_matrix_dlc_correct` | PASS | -- |
| 3 | `test_int_can_matrix_e2e_protected_messages` | PASS | -- |
| 4 | `test_int_can_matrix_non_e2e_messages` | PASS | -- |
| 5 | `test_int_can_matrix_rx_routing` | PASS | -- |

**Subtotal**: 5/5 PASS

#### INT-012: Signal Routing Full Stack (`test_int_signal_routing.c`)

| # | Test Function | Result | Duration |
|---|---------------|--------|----------|
| 1 | `test_int_tx_signal_routes_to_can` | PASS | -- |
| 2 | `test_int_rx_can_routes_to_signal` | PASS | -- |
| 3 | `test_int_multiple_pdus_routed_independently` | PASS | -- |
| 4 | `test_int_tx_failure_propagates` | PASS | -- |
| 5 | `test_int_rx_unknown_canid_discarded` | PASS | -- |

**Subtotal**: 5/5 PASS

#### INT-013/014: E2E Fault Detection (`test_int_e2e_faults.c`)

| # | Test Function | Result | Duration |
|---|---------------|--------|----------|
| 1 | `test_int_e2e_crc_single_bit_corruption` | PASS | -- |
| 2 | `test_int_e2e_crc_all_zeros_corruption` | PASS | -- |
| 3 | `test_int_e2e_sequence_gap_detected` | PASS | -- |
| 4 | `test_int_e2e_sequence_repeated_detected` | PASS | -- |
| 5 | `test_int_e2e_counter_wraparound_valid` | PASS | -- |
| 6 | `test_int_e2e_max_delta_boundary` | PASS | -- |
| 7 | `test_int_e2e_data_id_masquerade` | PASS | -- |

**Subtotal**: 7/7 PASS

#### INT-015: CAN Bus-Off Recovery (`test_int_can_busoff.c`)

| # | Test Function | Result | Duration |
|---|---------------|--------|----------|
| 1 | `test_int_busoff_notification_received` | PASS | -- |
| 2 | `test_int_tx_during_busoff` | PASS | -- |
| 3 | `test_int_recovery_after_busoff` | PASS | -- |
| 4 | `test_int_rx_still_works_after_busoff` | PASS | -- |

**Subtotal**: 4/4 PASS

## 4. Coverage

### 4.1 Structural Coverage

| Metric | Target | Measured | Status |
|--------|--------|----------|--------|
| Function entry | 100% (ASIL D) | 98.0% (49/50) | Below target — 1 function not entered; see note |
| Statement (line) | 100% (ASIL D) | 75.0% (392/523) | Below target — uncovered lines in PIL/HIL-only paths |
| Branch (decision) | 100% (ASIL D) | 59.6% (204/342) | Below target — hardware-dependent branches not reachable on host |
| MC/DC | 100% (ASIL D) | -- | Not measured on host; deferred to PIL/HIL |

> Coverage measured by lcov in CI run 22393954056 (2026-02-25) via `make -C test/integration coverage`. Coverage combines unit and integration test execution. Gap to 100% target is expected at this stage: lines and branches not covered correspond to hardware-dependent code paths (POSIX CAN error injection, target-specific MCAL stubs) that are not reachable in the host-based test environment. These paths are deferred to PIL/HIL testing per LIM-001 through LIM-004. The 98% function coverage confirms all safety-relevant functions are exercised; the single uncovered function is a POSIX-only diagnostic stub not reachable without real CAN hardware.

### 4.2 Interface Coverage

| Interface Type | Total Identified | Tested | Coverage |
|----------------|-----------------|--------|----------|
| BSW module pair interfaces | 15 | 15 | 100% |
| Safety paths | 15 | 15 | 100% |
| E2E fault types (ISO 26262 Annex E) | 7 | 7 | 100% |
| BswM mode transitions (forward) | 5 | 5 | 100% |
| BswM backward rejection paths | 5+ | 5+ | 100% |

### 4.3 E2E Fault Type Coverage

| Fault Type (ISO 26262 Part 6) | E2E Detection Method | Test IDs |
|-------------------------------|---------------------|----------|
| Corruption | CRC-8 | INT-003 T3, INT-013 T1, T2 |
| Repetition | Sequence counter (same counter twice) | INT-014 T4 |
| Loss / Delay | Sequence counter gap | INT-014 T3 |
| Insertion | Data ID validation | INT-003 T4, INT-014 T7 |
| Masquerade | Data ID in CRC computation | INT-014 T7 |
| Incorrect sequence | Counter delta > MaxDeltaCounter | INT-014 T3, T6 |
| Asymmetric information | CRC over full payload | INT-003 T3, INT-013 T1 |

## 5. Traceability

### 5.1 SWR Requirements Covered

| Requirement | Title | Test Coverage |
|-------------|-------|---------------|
| SWR-BSW-004 | CAN bus-off detection and recovery | INT-015 (4 tests) |
| SWR-BSW-011 | CanIf RX/TX frame routing | INT-003, INT-011, INT-012, INT-015 (19 tests) |
| SWR-BSW-013 | PduR bidirectional PDU routing | INT-003, INT-004, INT-012, INT-015 (14 tests) |
| SWR-BSW-015 | Com TX path (signal to CAN frame) | INT-003, INT-011, INT-012, INT-015 (19 tests) |
| SWR-BSW-016 | Com RX path (CAN frame to signal) | INT-003, INT-011, INT-012 (15 tests) |
| SWR-BSW-017 | Dcm UDS service processing | INT-004 (5 tests) |
| SWR-BSW-018 | Dem event storage with debouncing | INT-004, INT-008, INT-010 (15 tests) |
| SWR-BSW-019 | WdgM alive supervision and watchdog feed | INT-005, INT-007 (12 tests) |
| SWR-BSW-020 | WdgM expiry to Dem event reporting | INT-005, INT-007 (10 tests) |
| SWR-BSW-022 | BswM forward-only mode state machine | INT-005, INT-006, INT-007, INT-008, INT-010 (29 tests) |
| SWR-BSW-023 | E2E CRC-8 protection | INT-003, INT-011, INT-013/014 (12 tests) |
| SWR-BSW-024 | E2E alive counter supervision | INT-003, INT-013/014 (10 tests) |
| SWR-BSW-025 | E2E Data ID validation | INT-003, INT-013/014 (9 tests) |
| SWR-BSW-026 | Fault detection to safe state transition | INT-007, INT-008, INT-010 (15 tests) |
| SWR-BSW-027 | Actuator zeroing in safe state | INT-007, INT-010 (7 tests) |

### 5.2 TSR Traceability

| TSR | Description | Integration Tests |
|-----|-------------|-------------------|
| TSR-022 | E2E protection on all safety-critical CAN messages | INT-003, INT-011, INT-013/014 |
| TSR-035 | Safe state: motor de-energized, brakes applied | INT-007, INT-010 |
| TSR-046 | WdgM alive supervision detects missed checkpoints | INT-005, INT-007, INT-008 |
| TSR-047 | DEM stores fault event before safe state transition | INT-005, INT-007 |
| TSR-048 | BswM enforces forward-only mode transitions | INT-006, INT-007 |

### 5.3 Orphan Analysis

- **Untested SWR-BSW requirements**: None within integration scope. All 15 relevant interface requirements are covered.
- **Untested safety paths**: None. All 15 identified safety paths have dedicated test coverage.
- **Tests without requirement traceability**: None. Every test function has `@verifies` tags linking to SWR-BSW requirements.

## 6. Failure Analysis

### 6.1 Failed Tests

No failures. All 60 test cases passed in CI run 22393954056 (2026-02-25).

### 6.2 Known Issues

None at the time of test creation.

## 7. Known Limitations

| ID | Limitation | Impact | Mitigation |
|----|-----------|--------|------------|
| LIM-001 | Tests execute on host (x86-64), not target MCU (STM32G474/TMS570) | Timing behavior may differ on target; byte-order differences possible | PIL and HIL testing planned for target verification |
| LIM-002 | CAN hardware layer is mocked (no real CAN transceiver) | Physical-layer CAN faults (bit errors, bus contention) not tested | HIL testing with real CAN bus planned |
| LIM-003 | Timing not verified (no real-time clock in host test) | FTTI compliance not measurable in host environment | Timing validation deferred to PIL/HIL |
| LIM-004 | Single-threaded execution (no RTOS scheduling) | Priority inversion, preemption, and ISR interaction not tested | RTOS integration covered in ECU-level testing |
| LIM-005 | Inter-ECU communication not tested | CAN bus arbitration, multi-ECU heartbeat, gateway routing | Covered in SIL test suite (multi-ECU Docker) |
| LIM-006 | Host coverage below ASIL D 100% target (lines 75.0%, branches 59.6%) | Hardware-dependent paths not reachable on x86 host | Gap paths deferred to PIL/HIL; function coverage 98% confirms all safety functions exercised |
| LIM-007 | WdgM window watchdog timing not verified | Window timing (too early = reset) not testable without real timer | HIL required for window watchdog validation |

## 8. Conclusion

The integration test suite provides comprehensive coverage of the BSW inter-module interfaces, safety supervision chains, E2E protection mechanisms, diagnostic paths, and CAN communication stack. All 60 test cases passed in CI run 22393954056 (2026-02-25) with zero failures. Host-based structural coverage is 98.0% functions, 75.0% lines, and 59.6% branches; the gap to 100% is attributable to hardware-dependent code paths not reachable in the x86 host environment (see LIM-001 through LIM-004 and LIM-006). The BSW integration is considered verified at the host-simulation level per ASPICE SWE.5 requirements. Full 100% coverage confirmation is deferred to PIL/HIL execution.

**Remaining actions**:
1. Obtain FSE and PM sign-off in Section 9
2. Execute PIL/HIL tests to close hardware-dependent coverage gaps
3. Update coverage metrics in Section 4.1 after PIL/HIL execution

## 9. Approvals

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Test Engineer | | | |
| Safety Engineer (FSE) | | | |
| Project Manager | | | |
