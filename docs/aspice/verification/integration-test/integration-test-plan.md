---
document_id: ITP
title: "Integration Test Plan"
version: "1.0"
status: approved
aspice_process: "SWE.5"
iso_reference: "ISO 26262 Part 6, Section 10"
date: 2026-02-24
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


# Integration Test Plan

## 1. Purpose

This document defines the integration test plan for the Taktflow Zonal Vehicle Platform BSW stack, per Automotive SPICE 4.0 SWE.5 (Software Component Verification & Integration) and ISO 26262:2018 Part 6, Section 10 (Software Integration and Integration Testing). It specifies the integration strategy, test environment, interface test specifications, pass/fail criteria, module pairs under test, and safety path coverage.

## 2. Scope

### 2.1 In Scope

- Inter-module interface verification within the BSW stack (MCAL, ECUAL, Services, RTE)
- End-to-end communication data path testing (TX and RX)
- E2E protection chain verification (protect, transmit, receive, check)
- Safety supervision chain testing (WdgM, BswM, Dem, safe state transitions)
- Diagnostic chain testing (DEM event storage, DCM UDS readout)
- CAN message matrix conformance
- Signal routing through the full BSW communication stack
- CAN bus-off detection and recovery
- Overcurrent fault chain (sensor to motor shutdown to DEM to safe state)
- Heartbeat loss detection and degraded/safe-stop mode transitions

### 2.2 Out of Scope

- PIL (Processor-in-the-Loop) testing (requires target hardware)
- HIL (Hardware-in-the-Loop) testing (requires physical CAN bus and ECU boards)
- Application-level SWC integration (covered in ECU-level and SIL tests)
- Multi-ECU integration across CAN bus (covered in SIL test plan)

## 3. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| ITS | Integration Test Strategy & Plan | 1.0 |
| SWR-BSW | Software Requirements -- Shared BSW | 1.0 |
| UTP | Unit Test Plan | 1.0 |
| CAN-MATRIX | CAN Message Matrix | 0.1 |
| SYSREQ | System Requirements Specification | 1.0 |
| TSR | Technical Safety Requirements | 1.0 |
| BSW-ARCH | BSW Architecture | 1.0 |

## 4. Integration Strategy: Bottom-Up

The integration approach follows a bottom-up strategy as defined in ITS (Integration Test Strategy & Plan). Modules are integrated progressively from the hardware abstraction boundary upward through the BSW services layer.

### 4.1 Integration Levels

```
Level 1: Module Pairs (MCAL <-> ECUAL)
    Can <-> CanIf, CanIf <-> PduR, PduR <-> Com, PduR <-> Dcm
    Verified by: INT-003, INT-004, INT-012, INT-015

Level 2: BSW Communication Stack (full data path)
    Can -> CanIf -> PduR -> Com -> signal (bidirectional)
    Dcm -> PduR -> CanIf -> Can (UDS diagnostic path)
    Verified by: INT-003, INT-004, INT-011, INT-012

Level 3: Safety Supervision
    WdgM -> Dem -> BswM -> safe state
    E2E -> Com -> signal validation (CRC, sequence counter)
    Verified by: INT-005, INT-007, INT-013, INT-014

Level 4: ECU-Level Safety Chains
    Sensor -> RTE -> Monitor Runnable -> DEM -> BswM -> SAFE_STOP
    Heartbeat monitor -> DEGRADED / SAFE_STOP mode transitions
    Verified by: INT-007, INT-008, INT-010

Level 5: Multi-ECU SIL
    CVC <-> FZC <-> RZC via simulated CAN bus, SC monitoring all
    (Covered in SIL test plan, not this document)
```

### 4.2 Integration Order

| # | Integration | Modules Under Test | Depends On |
|---|------------|-------------------|------------|
| 1 | CAN data path | Can, CanIf, PduR, Com | Unit tests pass |
| 2 | E2E protection chain | E2E, Com, PduR, CanIf | #1 |
| 3 | Diagnostic path (DEM to DCM) | Dem, Dcm, PduR, CanIf | #1 |
| 4 | Safety supervision (WdgM to BswM) | WdgM, BswM, Dem | Unit tests pass |
| 5 | BswM mode transitions | BswM | Unit tests pass |
| 6 | Safe state chain | WdgM, BswM, Dem, Rte | #4, #5 |
| 7 | Heartbeat loss | WdgM, BswM, Dem, Rte | #4 |
| 8 | Overcurrent chain | Rte, BswM, Dem | #6 |
| 9 | CAN message matrix | E2E, Com, PduR, CanIf | #1, #2 |
| 10 | Signal routing full stack | Com, PduR, CanIf | #1 |
| 11 | CAN bus-off recovery | CanIf, PduR, Com | #1 |
| 12 | E2E fault detection | E2E | #2 |

## 5. Test Environment

| Component | Implementation |
|-----------|---------------|
| Host platform | x86-64, POSIX (Linux/MinGW) |
| Compiler | GCC (CI: Ubuntu apt; Local: 13.x/14.x) |
| Test framework | Unity 2.6.0 (vendored in `firmware/shared/bsw/unity/`) |
| CAN hardware layer | Mocked via `Can_Write()` stub capturing TX frames |
| HAL layer | Mocked: `Can_Hw_*`, `Dio_Hw_*`, `Dio_FlipChannel` stubs |
| Build system | GNU Make, `test/integration/Makefile` |
| Coverage tool | gcov + lcov (companion to GCC version) |
| Test execution | `make -C test/integration test` |

### 5.1 Mock Strategy

Each integration test links REAL BSW module object files together and mocks only the hardware layer at the MCAL boundary:

- **Mocked**: `Can_Write()`, `Can_Hw_Init()`, `Can_Hw_Start()`, `Can_Hw_Stop()`, `Can_Hw_Transmit()`, `Can_Hw_Receive()`, `Can_Hw_IsBusOff()`, `Can_Hw_GetErrorCounters()`, `Dio_FlipChannel()`, `Dio_Hw_ReadPin()`, `Dio_Hw_WritePin()`
- **Real (linked)**: E2E.c, Com.c, PduR.c, CanIf.c, Can.c (state machine), Dcm.c, Dem.c, WdgM.c, BswM.c, Rte.c

Mock state is reset in `setUp()` before every test function. Mock functions capture call counts and data for assertion.

## 6. Interface Test Specification

### 6.1 Summary

| Test ID | Test File | Description | Test Count | ASIL |
|---------|-----------|-------------|------------|------|
| INT-003 | `test_int_e2e_chain.c` | E2E protect -> transmit -> receive -> check | 5 | D |
| INT-004 | `test_int_dem_to_dcm.c` | DEM error report -> DCM DTC readout via UDS | 5 | B |
| INT-005 | `test_int_wdgm_supervision.c` | WdgM checkpoint -> deadline violation -> BswM safe state | 7 | D |
| INT-006 | `test_int_bswm_mode.c` | BswM mode transitions propagate via action callbacks | 7 | D |
| INT-007 | `test_int_safe_state.c` | Critical fault -> all actuators to safe state | 5 | D |
| INT-008 | `test_int_heartbeat_loss.c` | ECU heartbeat timeout -> degraded mode -> safe stop | 5 | D |
| INT-010 | `test_int_overcurrent_chain.c` | Current sensor -> threshold -> motor shutdown -> DEM | 5 | D |
| INT-011 | `test_int_can_matrix.c` | CAN message matrix verification (IDs, DLCs, E2E) | 5 | D |
| INT-012 | `test_int_signal_routing.c` | Signal routing full stack (TX and RX) | 5 | D |
| INT-013/014 | `test_int_e2e_faults.c` | E2E CRC corruption and sequence gap detection | 7 | D |
| INT-015 | `test_int_can_busoff.c` | CAN bus-off detection and recovery | 4 | D |
| | | **Total** | **60** | |

### 6.2 INT-003: E2E Protection Chain

**File**: `test/integration/test_int_e2e_chain.c`
**Linked Modules (REAL)**: E2E.c, Com.c, PduR.c, CanIf.c
**Mocked**: Can_Write, Can_Hw_*, Dio_FlipChannel, Dcm_RxIndication, CanIf_ControllerBusOff
**Verifies**: SWR-BSW-011, SWR-BSW-013, SWR-BSW-015, SWR-BSW-016, SWR-BSW-023, SWR-BSW-024, SWR-BSW-025

| # | Test Function | Description | Pass Criteria |
|---|---------------|-------------|---------------|
| 1 | `test_int_e2e_protect_tx_rx_check_roundtrip` | E2E protect a payload, transmit via Com stack, capture at Can_Write mock, feed back via CanIf_RxIndication, verify E2E_Check returns E2E_STATUS_OK | E2E_STATUS_OK on valid round-trip |
| 2 | `test_int_e2e_roundtrip_counter_increments` | Two consecutive protect+check cycles, verify alive counter increments by 1 | Counter_2 == Counter_1 + 1, second check returns OK |
| 3 | `test_int_e2e_roundtrip_corrupted_crc_detected` | Protect, corrupt CRC byte, feed back, verify E2E_Check returns E2E_STATUS_ERROR | E2E_STATUS_ERROR returned |
| 4 | `test_int_e2e_roundtrip_data_id_mismatch` | Protect with DataId=5, Check with DataId=6, verify E2E_STATUS_ERROR | E2E_STATUS_ERROR returned |
| 5 | `test_int_e2e_full_stack_data_preserved` | Send signal value torque=75 through full stack, receive back, verify value preserved | send_val == recv_val |

### 6.3 INT-004: DEM to DCM Diagnostic Chain

**File**: `test/integration/test_int_dem_to_dcm.c`
**Linked Modules (REAL)**: Dem.c, Dcm.c, PduR.c, CanIf.c
**Mocked**: Can_Write (captures TX response), Com_RxIndication
**Verifies**: SWR-BSW-017, SWR-BSW-018, SWR-BSW-011, SWR-BSW-013

| # | Test Function | Description | Pass Criteria |
|---|---------------|-------------|---------------|
| 1 | `test_int_dem_report_then_dcm_read_status` | Report DEM event FAILED x3 (debounce), UDS ReadDID -> verify CONFIRMED status byte | Positive response (0x62), DTC status has CONFIRMED bit set |
| 2 | `test_int_dem_clear_then_dcm_reads_clean` | Create confirmed DTC, clear all DTCs, UDS ReadDID -> verify status=0 | DTC status byte is 0x00 |
| 3 | `test_int_dcm_uds_session_switch` | UDS DiagnosticSessionControl to Extended (0x03), verify positive response | Response SID=0x50, session=0x03 |
| 4 | `test_int_dcm_unknown_sid_nrc` | Send unknown SID 0xFF -> verify NRC response (0x7F, 0xFF, 0x11) | Negative response with NRC serviceNotSupported |
| 5 | `test_int_dcm_response_routes_through_pdur` | TesterPresent -> verify response routes through PduR -> CanIf -> Can_Write with correct CAN ID | mock_can_tx_id == 0x7E8, positive response 0x7E |

### 6.4 INT-005: WdgM Supervision Chain

**File**: `test/integration/test_int_wdgm_supervision.c`
**Linked Modules (REAL)**: WdgM.c, BswM.c, Dem.c
**Mocked**: Dio_FlipChannel (captures call count)
**Verifies**: SWR-BSW-019, SWR-BSW-020, SWR-BSW-022
**Traces to**: TSR-046, TSR-047

| # | Test Function | Description | Pass Criteria |
|---|---------------|-------------|---------------|
| 1 | `test_int_wdgm_ok_feeds_watchdog` | 2 checkpoints (within 1..3 range), WdgM_MainFunction, verify Dio_FlipChannel called | dio_flip_count==1, global status OK |
| 2 | `test_int_wdgm_missed_checkpoint_fails` | 0 checkpoints, WdgM_MainFunction, verify Dio NOT called | dio_flip_count==0, global status FAILED |
| 3 | `test_int_wdgm_expired_triggers_dem` | Low tolerance config, miss 2 cycles, verify SE EXPIRED and DEM event 15 has TEST_FAILED | local==EXPIRED, DEM testFailed bit set |
| 4 | `test_int_wdgm_expired_then_bswm_safe_stop` | Expire SE, request BswM SAFE_STOP, verify mode transition and action callback | BswM mode==SAFE_STOP, action_safe_stop_count==1 |
| 5 | `test_int_wdgm_recovery_after_failed` | Miss 1 cycle (FAILED), provide correct checkpoints next cycle, verify recovery to OK | local==OK, global==OK, dio_flip_count>0 |
| 6 | `test_int_wdgm_multiple_se_one_fails` | 2 SEs, SE 0 OK, SE 1 missed, verify global FAILED, watchdog NOT fed | global==FAILED, dio_flip_count==0 |
| 7 | `test_int_wdgm_dem_occurrence_counter` | Expire SE, reach DEM debounce threshold, verify occurrence counter >= 1 | occ_count>=1, testFailed bit set |

### 6.5 INT-006: BswM Mode Transitions

**File**: `test/integration/test_int_bswm_mode.c`
**Linked Modules (REAL)**: BswM.c
**Mocked**: None (BswM has no HW dependencies)
**Verifies**: SWR-BSW-022
**Traces to**: TSR-046, TSR-047, TSR-048

| # | Test Function | Description | Pass Criteria |
|---|---------------|-------------|---------------|
| 1 | `test_int_bswm_startup_to_run_callback` | STARTUP -> RUN, MainFunction, verify RUN callback fires | cb_run_count==1, cb_startup_count==0 |
| 2 | `test_int_bswm_degraded_callback` | RUN -> DEGRADED, MainFunction, verify DEGRADED callback fires | cb_degraded_count==1, no other callbacks |
| 3 | `test_int_bswm_safe_stop_callback` | RUN -> SAFE_STOP, verify SAFE_STOP callback fires | cb_safe_stop_count==1 |
| 4 | `test_int_bswm_multiple_actions_per_mode` | 3 RUN mode actions, verify all 3 fire during MainFunction | All 3 RUN callbacks invoked |
| 5 | `test_int_bswm_invalid_backward_transition` | RUN -> STARTUP rejected, mode stays RUN | E_NOT_OK returned, mode==RUN |
| 6 | `test_int_bswm_full_lifecycle` | STARTUP -> RUN -> DEGRADED -> SAFE_STOP -> SHUTDOWN, verify callback sequence | All 5 callbacks fire in order |
| 7 | `test_int_bswm_shutdown_terminal` | From SHUTDOWN, all transitions rejected | All BswM_RequestMode calls return E_NOT_OK |

### 6.6 INT-007: Critical Fault to Safe State

**File**: `test/integration/test_int_safe_state.c`
**Linked Modules (REAL)**: WdgM.c, BswM.c, Dem.c, Rte.c
**Mocked**: Dio_FlipChannel
**Verifies**: SWR-BSW-019, SWR-BSW-020, SWR-BSW-022, SWR-BSW-026, SWR-BSW-027
**Traces to**: TSR-035, TSR-046, TSR-047, TSR-048

| # | Test Function | Description | Pass Criteria |
|---|---------------|-------------|---------------|
| 1 | `test_int_fault_to_safe_state_chain` | Fault signal -> safety runnable -> BswM SAFE_STOP | BswM mode==SAFE_STOP |
| 2 | `test_int_safe_state_zeros_actuators` | SAFE_STOP callback zeros actuator RTE signals | Motor=0, Steering=5000 (center), Brake=10000 (max) |
| 3 | `test_int_wdgm_expiry_triggers_safe_stop` | WdgM SE expires -> DEM event -> safety handler -> SAFE_STOP | SE==EXPIRED, DEM testFailed, BswM==SAFE_STOP |
| 4 | `test_int_safe_stop_to_shutdown_only` | From SAFE_STOP, only SHUTDOWN transition allowed | STARTUP/RUN/DEGRADED rejected, SHUTDOWN accepted |
| 5 | `test_int_dem_records_fault_before_safe_state` | DEM event stored BEFORE BswM transition (ordering guarantee) | DEM event 10 testFailed, BswM==SAFE_STOP |

### 6.7 INT-008: Heartbeat Loss

**File**: `test/integration/test_int_heartbeat_loss.c`
**Linked Modules (REAL)**: WdgM.c, BswM.c, Dem.c, Rte.c
**Mocked**: Dio_FlipChannel
**Verifies**: SWR-BSW-018, SWR-BSW-019, SWR-BSW-020, SWR-BSW-022, SWR-BSW-026

| # | Test Function | Description | Pass Criteria |
|---|---------------|-------------|---------------|
| 1 | `test_int_heartbeat_present_system_ok` | 20 cycles with incrementing heartbeat, system stays RUN | BswM mode==RUN, no DEM events |
| 2 | `test_int_heartbeat_timeout_triggers_degraded` | Stop FZC heartbeat, 5 stale cycles, system enters DEGRADED | BswM mode==DEGRADED |
| 3 | `test_int_heartbeat_timeout_dem_event` | Heartbeat timeout -> DEM event 20 has testFailed | DEM testFailed bit set |
| 4 | `test_int_heartbeat_recovery_from_degraded` | Heartbeat returns after DEGRADED entry, BswM stays DEGRADED (forward-only) | BswM mode==DEGRADED (no recovery) |
| 5 | `test_int_dual_heartbeat_loss_safe_stop` | Both FZC and RZC heartbeats lost -> BswM SAFE_STOP | BswM mode==SAFE_STOP, both DEM events set |

### 6.8 INT-010: Overcurrent Chain

**File**: `test/integration/test_int_overcurrent_chain.c`
**Linked Modules (REAL)**: WdgM.c, BswM.c, Dem.c, Rte.c
**Mocked**: Dio_FlipChannel
**Verifies**: SWR-BSW-018, SWR-BSW-022, SWR-BSW-026, SWR-BSW-027

| # | Test Function | Description | Pass Criteria |
|---|---------------|-------------|---------------|
| 1 | `test_int_overcurrent_detected_motor_shutdown` | Current > threshold -> motor enable goes to 0 | RTE motor_enable==0 |
| 2 | `test_int_overcurrent_dem_event_stored` | Overcurrent x3 (debounce) -> DEM event CONFIRMED | DEM CONFIRMED + testFailed bits set |
| 3 | `test_int_overcurrent_triggers_safe_stop` | Overcurrent -> BswM SAFE_STOP | BswM mode==SAFE_STOP, motor_enable==0 |
| 4 | `test_int_normal_current_no_action` | Current < threshold -> no shutdown, no DEM, RUN mode | motor_enable==1, DEM clean, BswM==RUN |
| 5 | `test_int_overcurrent_threshold_boundary` | At threshold: no trigger; at threshold+1: triggers | Boundary value verification |

### 6.9 INT-011: CAN Message Matrix Verification

**File**: `test/integration/test_int_can_matrix.c`
**Linked Modules (REAL)**: Com.c, PduR.c, CanIf.c, E2E.c
**Mocked**: Can_Write, Can_Hw_*, Dio_*, Dcm_RxIndication, CanIf_ControllerBusOff
**Verifies**: SWR-BSW-011, SWR-BSW-015, SWR-BSW-016, SWR-BSW-023

| # | Test Function | Description | Pass Criteria |
|---|---------------|-------------|---------------|
| 1 | `test_int_can_matrix_tx_ids_correct` | 4 TX PDUs (0x101, 0x102, 0x300, 0x350), verify correct CAN IDs | All 4 CAN IDs found in TX captures |
| 2 | `test_int_can_matrix_dlc_correct` | Verify DLC per matrix: 8, 8, 8, 4 | DLC matches for each CAN ID |
| 3 | `test_int_can_matrix_e2e_protected_messages` | E2E protect Torque/Steer/Motor PDUs, verify DataId and CRC in headers | DataId matches, CRC non-zero |
| 4 | `test_int_can_matrix_non_e2e_messages` | Body_Control_Cmd (QM, no E2E), verify raw signal passes through | Byte 0 contains raw signal value |
| 5 | `test_int_can_matrix_rx_routing` | Inject Steering_Status (0x200) and UDS (0x7E0), verify correct routing | Steering signal updated, UDS routed to DCM |

### 6.10 INT-012: Signal Routing Full Stack

**File**: `test/integration/test_int_signal_routing.c`
**Linked Modules (REAL)**: Com.c, PduR.c, CanIf.c
**Mocked**: Can_Write, Can_Hw_*, Dio_*, Dcm_RxIndication, CanIf_ControllerBusOff
**Verifies**: SWR-BSW-011, SWR-BSW-013, SWR-BSW-015, SWR-BSW-016

| # | Test Function | Description | Pass Criteria |
|---|---------------|-------------|---------------|
| 1 | `test_int_tx_signal_routes_to_can` | Com_SendSignal -> Com_MainFunction_Tx -> Can_Write, verify CAN ID and data | CAN ID==0x100, data[2]==128 |
| 2 | `test_int_rx_can_routes_to_signal` | CanIf_RxIndication -> PduR -> Com_RxIndication -> Com_ReceiveSignal | recv_val==42 |
| 3 | `test_int_multiple_pdus_routed_independently` | 2 PDUs with different signals, verify independent routing | Torque==55, Speed==99 |
| 4 | `test_int_tx_failure_propagates` | Can_Write returns CAN_NOT_OK, verify Com handles gracefully (no crash) | No crash, retry on next cycle |
| 5 | `test_int_rx_unknown_canid_discarded` | CanIf_RxIndication with CAN ID 0x999, verify no signal update | Signal retains pre-set value |

### 6.11 INT-013/014: E2E Fault Detection

**File**: `test/integration/test_int_e2e_faults.c`
**Linked Modules (REAL)**: E2E.c
**Mocked**: Can_Write, Can_Hw_*, Dio_* (stubs, not exercised)
**Verifies**: SWR-BSW-023, SWR-BSW-024, SWR-BSW-025

| # | Test Function | Description | Pass Criteria |
|---|---------------|-------------|---------------|
| 1 | `test_int_e2e_crc_single_bit_corruption` | Protect, flip 1 bit in payload, Check returns ERROR | E2E_STATUS_ERROR |
| 2 | `test_int_e2e_crc_all_zeros_corruption` | Protect, zero CRC byte, Check returns ERROR | E2E_STATUS_ERROR |
| 3 | `test_int_e2e_sequence_gap_detected` | Skip 3 messages (MaxDelta=1), Check returns WRONG_SEQ | E2E_STATUS_WRONG_SEQ |
| 4 | `test_int_e2e_sequence_repeated_detected` | Same PDU checked twice, second returns REPEATED | E2E_STATUS_REPEATED |
| 5 | `test_int_e2e_counter_wraparound_valid` | Counter 14 -> 15 -> 0 (wraparound), all return OK | E2E_STATUS_OK for all 3 |
| 6 | `test_int_e2e_max_delta_boundary` | Delta == MaxDelta: OK; Delta == MaxDelta+1: WRONG_SEQ | Boundary behavior verified |
| 7 | `test_int_e2e_data_id_masquerade` | Tamper DataId field in byte 0, CRC from original protect, Check returns ERROR | E2E_STATUS_ERROR |

### 6.12 INT-015: CAN Bus-Off Recovery

**File**: `test/integration/test_int_can_busoff.c`
**Linked Modules (REAL)**: CanIf.c, PduR.c, Com.c
**Mocked**: Can_Write (simulates bus-off by returning CAN_NOT_OK), Dcm_RxIndication
**Verifies**: SWR-BSW-004, SWR-BSW-011, SWR-BSW-013, SWR-BSW-015

| # | Test Function | Description | Pass Criteria |
|---|---------------|-------------|---------------|
| 1 | `test_int_busoff_notification_received` | CanIf_ControllerBusOff(0), no crash, RX path still works | System functional after bus-off notification |
| 2 | `test_int_tx_during_busoff` | Queue signal, Can_Write returns CAN_NOT_OK, verify TX attempt made | mock_can_tx_count==1, PDU remains pending |
| 3 | `test_int_recovery_after_busoff` | Fail TX, then restore CAN_OK, re-send signal, verify successful TX | CAN ID matches, data correct |
| 4 | `test_int_rx_still_works_after_busoff` | After bus-off notification, inject 2 RX frames, verify both route correctly | Both signals received correctly |

## 7. Module Pairs Tested

The following BSW module pair interfaces are exercised by the integration tests:

| Module A | Module B | Interface | Verified By |
|----------|----------|-----------|-------------|
| Com | PduR | `PduR_ComTransmit()` (TX path) | INT-003, INT-011, INT-012 |
| PduR | CanIf | `CanIf_Transmit()` (TX path) | INT-003, INT-004, INT-011, INT-012, INT-015 |
| CanIf | Can (mock) | `Can_Write()` (TX to HW) | INT-003, INT-004, INT-011, INT-012, INT-015 |
| CanIf | PduR | `PduR_RxIndication()` (RX path) | INT-003, INT-011, INT-012, INT-015 |
| PduR | Com | `Com_RxIndication()` (RX to signal) | INT-003, INT-011, INT-012, INT-015 |
| PduR | Dcm | `Dcm_RxIndication()` (RX diagnostic) | INT-004, INT-011 |
| Dcm | PduR | `PduR_DcmTransmit()` (TX diagnostic response) | INT-004 |
| E2E | Com | E2E_Protect -> Com PDU (pre-TX); E2E_Check -> Com PDU (post-RX) | INT-003, INT-011, INT-013/014 |
| WdgM | Dem | `Dem_ReportErrorStatus()` (expiry event) | INT-005, INT-007 |
| WdgM | Dio (mock) | `Dio_FlipChannel()` (watchdog toggle) | INT-005, INT-007 |
| BswM | (callbacks) | Mode action callbacks dispatch | INT-005, INT-006, INT-007 |
| Dem | Dcm | `Dem_GetEventStatus()` via DID callback | INT-004 |
| Rte | BswM | `BswM_RequestMode()` from runnable | INT-007, INT-008, INT-010 |
| Rte | Dem | `Dem_ReportErrorStatus()` from runnable | INT-007, INT-008, INT-010 |
| Rte | WdgM | `WdgM_CheckpointReached()` from runnable dispatch | INT-007 |

## 8. Pass / Fail Criteria

| Criterion | Threshold | Rationale |
|-----------|-----------|-----------|
| All integration tests pass | 0 failures across 60 tests | ISO 26262 Part 6, Section 10.4.5 |
| All safety path tests pass | 0 failures (INT-005, INT-007, INT-008, INT-010) | ASIL D: safety paths must be fully verified |
| E2E fault detection coverage | All 7 E2E fault types detected (CRC, sequence gap, repeated, wraparound, delta boundary, masquerade, data ID mismatch) | ISO 26262 Part 6, Section 10: E2E comm faults |
| Safe state reachable | From RUN, DEGRADED, or any fault condition | Part 4: Safe state within FTTI |
| BswM forward-only enforced | All backward transitions rejected | SWR-BSW-022: mode state machine integrity |
| No regression | All unit tests still pass after integration | ASPICE SWE.5: no regression from integration |
| Signal data preserved | TX/RX round-trip preserves signal values | Communication integrity |
| CAN matrix conformance | All CAN IDs and DLCs match can-message-matrix.md | Interface control document compliance |

## 9. Safety Path Coverage

The following safety-critical paths are covered by the integration test suite:

| Safety Path | Description | Test IDs | SWR Requirements | TSR Traceability |
|-------------|-------------|----------|-------------------|------------------|
| SP-01 | WdgM alive supervision -> Dio watchdog feed | INT-005 T1, T2 | SWR-BSW-019 | TSR-046 |
| SP-02 | WdgM SE expiry -> Dem event report | INT-005 T3, T7 | SWR-BSW-019, SWR-BSW-020 | TSR-046, TSR-047 |
| SP-03 | WdgM expiry -> BswM SAFE_STOP transition | INT-005 T4, INT-007 T3 | SWR-BSW-019, SWR-BSW-020, SWR-BSW-022 | TSR-046, TSR-047 |
| SP-04 | BswM SAFE_STOP -> actuator zeroing | INT-007 T2 | SWR-BSW-022, SWR-BSW-026, SWR-BSW-027 | TSR-035, TSR-048 |
| SP-05 | RTE fault signal -> safety runnable -> SAFE_STOP | INT-007 T1, T5 | SWR-BSW-022, SWR-BSW-026 | TSR-035, TSR-046 |
| SP-06 | Heartbeat loss -> DEGRADED mode | INT-008 T2, T3 | SWR-BSW-022, SWR-BSW-026 | TSR-046 |
| SP-07 | Dual heartbeat loss -> SAFE_STOP | INT-008 T5 | SWR-BSW-022, SWR-BSW-018 | TSR-046, TSR-047 |
| SP-08 | Overcurrent -> motor shutdown -> DEM | INT-010 T1, T2, T3 | SWR-BSW-018, SWR-BSW-026, SWR-BSW-027 | TSR-035 |
| SP-09 | E2E CRC corruption detected | INT-003 T3, INT-013 T1, T2 | SWR-BSW-023 | TSR-022 |
| SP-10 | E2E sequence counter gap detected | INT-014 T3, T6 | SWR-BSW-024 | TSR-022 |
| SP-11 | E2E repeated message detected | INT-014 T4 | SWR-BSW-024 | TSR-022 |
| SP-12 | E2E data ID masquerade detected | INT-014 T7 | SWR-BSW-024, SWR-BSW-025 | TSR-022 |
| SP-13 | BswM forward-only mode transitions enforced | INT-006 T5, T7, INT-007 T4 | SWR-BSW-022 | TSR-048 |
| SP-14 | CAN bus-off recovery | INT-015 T1, T3, T4 | SWR-BSW-004, SWR-BSW-011 | TSR-022 |
| SP-15 | DEM event stored BEFORE safe state transition | INT-007 T5 | SWR-BSW-020, SWR-BSW-022 | TSR-047 |

## 10. Requirements Traceability

### 10.1 SWR to Integration Test Mapping

| Requirement | Description | Integration Test Coverage |
|-------------|-------------|--------------------------|
| SWR-BSW-004 | CAN bus-off detection and recovery | INT-015 |
| SWR-BSW-011 | CanIf RX/TX routing | INT-003, INT-011, INT-012, INT-015 |
| SWR-BSW-013 | PduR bidirectional routing | INT-003, INT-004, INT-012, INT-015 |
| SWR-BSW-015 | Com TX path (signal -> PDU -> CAN) | INT-003, INT-011, INT-012, INT-015 |
| SWR-BSW-016 | Com RX path (CAN -> PDU -> signal) | INT-003, INT-011, INT-012 |
| SWR-BSW-017 | Dcm UDS service handling | INT-004 |
| SWR-BSW-018 | Dem event storage and debouncing | INT-004, INT-008, INT-010 |
| SWR-BSW-019 | WdgM alive supervision | INT-005, INT-007 |
| SWR-BSW-020 | WdgM expiry -> Dem event reporting | INT-005, INT-007 |
| SWR-BSW-022 | BswM forward-only mode state machine | INT-005, INT-006, INT-007, INT-008, INT-010 |
| SWR-BSW-023 | E2E CRC-8 protection | INT-003, INT-011, INT-013/014 |
| SWR-BSW-024 | E2E alive counter (4-bit) supervision | INT-003, INT-013/014 |
| SWR-BSW-025 | E2E Data ID validation | INT-003, INT-013/014 |
| SWR-BSW-026 | Fault detection -> safe state transition | INT-007, INT-008, INT-010 |
| SWR-BSW-027 | Actuator zeroing in safe state | INT-007, INT-010 |

### 10.2 Orphan Analysis

All 15 SWR-BSW requirements relevant to inter-module interfaces are covered by at least one integration test. No orphan requirements exist for the BSW integration scope.

## 11. Test Execution Procedure

1. Ensure all unit tests pass: `make -C firmware/shared/bsw test`
2. Build integration tests: `make -C test/integration build`
3. Execute integration test suite: `make -C test/integration test`
4. Verify 60/60 tests PASS with 0 failures
5. Generate coverage report: `make -C test/integration coverage`
6. Record results in Integration Test Report (ITR)

## 12. Approvals

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Test Engineer | | | |
| Safety Engineer (FSE) | | | |
| Project Manager | | | |
