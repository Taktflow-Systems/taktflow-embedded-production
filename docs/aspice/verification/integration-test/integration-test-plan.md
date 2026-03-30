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

---

## 13. AI-Driven Test Generation

This section extends the integration test plan with AI-assisted test generation across two complementary axes: **requirements-based testing (RBT)** — where the generator reads SWR requirements to close coverage gaps — and **fault tree-based testing (FTT)** — where the generator derives fault injection cases from the FMEA and HARA hazardous event tree.

**Safety constraint (QM tool layer only):** All AI generation steps run as offline pre-processors in `tools/pipeline/`. Generated test stubs carry an `// AI_GENERATED_STUB` marker and contain `TEST_IGNORE()` bodies. They do not count toward ASIL coverage metrics until a human reviewer promotes them (replaces `TEST_IGNORE()` with a real assertion and removes the marker). AI output never modifies ASIL-tagged firmware or generated config files directly. See `docs/integration_audit.md §12.2` for the complete safety boundary rationale.

---

### 13.1 Requirements-Based Test Generation (RBT)

Requirements-based generation closes the coverage gap between formal SWR requirements and the existing test suite. The generator parses the SWR documents and the `@verifies` annotations already present in `test/unit/bsw/` and `test/integration/` to find any SWR requirement that has no corresponding test function.

#### 13.1.1 Source Documents

| Document | Location | Content |
|----------|----------|---------|
| SWR-BSW | `docs/aspice/software/sw-requirements/SWR-BSW.md` | Shared BSW requirements (SWR-BSW-001 … SWR-BSW-031) |
| SWR-CVC | `docs/aspice/software/sw-requirements/SWR-CVC.md` | CVC-specific requirements |
| SWR-FZC | `docs/aspice/software/sw-requirements/SWR-FZC.md` | FZC-specific requirements |
| SWR-RZC | `docs/aspice/software/sw-requirements/SWR-RZC.md` | RZC-specific requirements |
| SWR-SC | `docs/aspice/software/sw-requirements/SWR-SC.md` | SC safety controller requirements |
| Traceability matrix | `docs/aspice/traceability/traceability-matrix.md` | Current SWR ↔ test coverage map |

#### 13.1.2 Generation Method

```
Step 1 — Parse SWR documents:
    Extract all requirement IDs (SWR-BSW-NNN, SWR-CVC-NNN, …)
    Classify each requirement by BSW module (Com, E2E, WdgM, BswM, …)

Step 2 — Parse existing tests:
    Scan test/unit/bsw/ and test/integration/ for @verifies annotations
    Build coverage map: {SWR-ID → [test_function_name, …]}

Step 3 — Identify gaps:
    coverage_count == 0                      → uncovered requirement
    coverage_count == 1 AND ASIL D           → under-covered (flag for review)

Step 4 — Generate test stubs:
    For each uncovered SWR requirement, emit a Unity test stub:
      File:     test/unit/bsw/test_<Module>_rbt_generated.c
      Function: test_<SWR_ID>_<brief_label>
      Body:     TEST_IGNORE(); // AI_GENERATED_STUB — promote before counting toward ASIL
      Tags:     @verifies <SWR-ID>, @aspice SWE.5, @asil <D|C|B|QM>
      Setup:    module init / mock reset scaffolding from existing test pattern

Step 5 — Emit coverage report:
    build/pipeline/rbt_coverage.json
      {total_swrs: N, covered: M, uncovered: [SWR-IDs], under_covered: [SWR-IDs],
       coverage_pct: M/N, promoted_pct: <manually promoted> / N}
```

#### 13.1.3 Tool Location

New file: `tools/pipeline/gen_rbt_tests.py`

```bash
# Dry-run: show gap report only, write nothing
python -m tools.pipeline gen_rbt_tests \
    --swr-dir docs/aspice/software/sw-requirements \
    --test-dir test/unit/bsw \
    --integration-test-dir test/integration \
    --report build/pipeline/rbt_coverage.json \
    --dry-run

# Full run: write stubs to output directory
python -m tools.pipeline gen_rbt_tests \
    --swr-dir docs/aspice/software/sw-requirements \
    --test-dir test/unit/bsw \
    --integration-test-dir test/integration \
    --output-dir test/unit/bsw \
    --report build/pipeline/rbt_coverage.json
```

#### 13.1.4 Output Artifacts

| Artifact | Location | Description |
|----------|----------|-------------|
| Generated stub file | `test/unit/bsw/test_<Module>_rbt_generated.c` | Unity stubs with `TEST_IGNORE()` bodies |
| Coverage report | `build/pipeline/rbt_coverage.json` | SWR ID → test mapping, gap list, coverage % |
| Console summary | stdout | `NNN/MMM SWRs covered (NN.N%)` |

**Promotion gate:** A reviewer replaces `TEST_IGNORE()` with a real assertion and removes the `// AI_GENERATED_STUB` marker. Only promoted tests count toward the ASIL traceability report.

#### 13.1.5 Coverage Exit Criteria

| ASIL Level | Criterion | Threshold |
|------------|-----------|-----------|
| ASIL D | All ASIL D SWRs have ≥ 1 promoted (non-stub) test | 100% |
| ASIL D | No ASIL D SWR has only a single test (under-covered) | 0 allowed |
| ASIL C | All ASIL C SWRs have ≥ 1 promoted test | 100% |
| ASIL B / QM | Promoted or stub coverage | ≥ 80% |

---

### 13.2 Fault Tree-Based Test Generation (FTT)

Fault tree-based generation derives test cases from the FMEA failure mode catalogue and HARA hazardous event tree. Each identified failure mode becomes a fault injection test case or multi-ECU fault scenario.

#### 13.2.1 Source Documents

| Document | Location | Content |
|----------|----------|---------|
| FMEA | `docs/safety/analysis/fmea.md` | Failure modes, failure effects, ASIL classification per module |
| HARA | `docs/safety/concept/hara.md` | Hazardous events (HE-001 … HE-017), safety goals, ASIL |
| Fault reactions table | `docs/integration_audit.md §9.3` | Fault trigger → BSW path → safe reaction |
| Safety paths | §9 of this document (SP-01 … SP-15) | Existing safety path coverage |

#### 13.2.2 Fault Tree Injection Mapping

| FMEA Failure Mode Category | BSW Injection Point | Target Test File | Output Format |
|----------------------------|--------------------|--------------------|---------------|
| CAN signal corruption (CRC mismatch) | `E2E_Check()` return override | `test_int_e2e_faults.c` | Unity C stub |
| CAN signal deadline miss | `Com_MainFunction_Rx()` skip N cycles | `test_int_heartbeat_loss.c` | Unity C stub |
| Watchdog deadline violation | `WdgM_CheckpointReached()` skip | `test_int_wdgm_supervision.c` | Unity C stub |
| Sensor value out of range | `IoHwAb_Inject_SetAnalog()` override | `test_int_overcurrent_chain.c` | Unity C stub |
| CAN bus-off event | `CanIf_ControllerBusOff()` trigger | `test_int_can_busoff.c` | Unity C stub |
| ECU software lockup (heartbeat silent) | Suppress heartbeat TX N cycles | `test/sil/scenarios/` | YAML scenario |
| Multi-ECU failure (dual heartbeat loss) | Suppress FZC + RZC heartbeats | `gateway/fault_inject/scenarios/ai_generated/` | YAML scenario |
| E2E DataID masquerade | Tamper DataId field in PDU | `test_int_e2e_faults.c` | Unity C stub |
| Actuator command out of range | Inject invalid RTE signal value | `test_int_safe_state.c` | Unity C stub |

#### 13.2.3 HARA Hazardous Event Coverage Map

Each HARA hazardous event must be traceable to at least one fault injection test. The table below is the authoritative gap map used by the generator:

| HARA HE | Description | Required Test Coverage | Existing SP Coverage | Gap |
|---------|-------------|----------------------|---------------------|-----|
| HE-001 | Unintended acceleration from rest | Pedal stuck → motor shutdown | SP-05, SP-08 | None — covered |
| HE-004 | Loss of steering control | Steering PDU CRC corrupt → safe center position | SP-09, SP-10 | None — covered |
| HE-005 | Loss of braking | Brake signal E2E deadline miss → BswM SAFE_STOP | SP-06 (partial) | **FTT-001: brake PDU timeout → SAFE_STOP** |
| HE-017 | Unintended motion from rest | CVC watchdog miss → torque cut | SP-01, SP-02, SP-03 | None — covered |

Generated test IDs for gaps: `FTT-001` (brake signal E2E deadline miss → SAFE_STOP) is the first output of the FTT generator.

#### 13.2.4 Generation Method

```
Step 1 — Parse FMEA failure modes:
    Extract: {failure_mode_id, module, failure_type, effect, ASIL}

Step 2 — Classify injection type:
    failure_type "signal corruption"    → E2E_Check return override
    failure_type "timeout / deadline"   → cycle skip injection
    failure_type "value OOR"            → IoHwAb analog override
    failure_type "bus-off"              → CanIf_ControllerBusOff call
    failure_type "ECU silent"           → heartbeat suppression YAML scenario
    failure_type "multi-ECU fault"      → multi-suppression YAML scenario

Step 3 — Cross-check against SP-01 … SP-15 (§9 of this document):
    Map each failure mode to its nearest existing safety path test
    Mark uncovered failure modes → generate new stubs or YAML

Step 4 — Emit test artifacts:
    BSW-level faults   → Unity C stub in test/integration/
                         file: test_int_ftt_<FMEA_ID>_generated.c
    Multi-ECU faults   → YAML scenario in
                         gateway/fault_inject/scenarios/ai_generated/ftt_<HE_ID>.yaml

Step 5 — Emit FTT coverage report:
    build/pipeline/ftt_coverage.json
      {total_fmea_items: N, covered: M, generated_c_stubs: K, generated_yaml: J,
       hara_he_covered: X, hara_he_total: Y}
```

#### 13.2.5 Tool Location

New file: `tools/pipeline/gen_ftt_tests.py`

```bash
# Dry-run: show FMEA → injection mapping, write nothing
python -m tools.pipeline gen_ftt_tests \
    --fmea docs/safety/analysis/fmea.md \
    --hara docs/safety/concept/hara.md \
    --safety-paths docs/aspice/verification/integration-test/integration-test-plan.md \
    --report build/pipeline/ftt_coverage.json \
    --dry-run

# Full run: emit C stubs and YAML scenarios
python -m tools.pipeline gen_ftt_tests \
    --fmea docs/safety/analysis/fmea.md \
    --hara docs/safety/concept/hara.md \
    --safety-paths docs/aspice/verification/integration-test/integration-test-plan.md \
    --output-c test/integration \
    --output-yaml gateway/fault_inject/scenarios/ai_generated \
    --report build/pipeline/ftt_coverage.json
```

#### 13.2.6 Output Artifacts

| Artifact | Location | Description |
|----------|----------|-------------|
| FTT C stubs | `test/integration/test_int_ftt_<FMEA_ID>_generated.c` | Unity fault injection stubs |
| FTT YAML scenarios | `gateway/fault_inject/scenarios/ai_generated/ftt_<HE_ID>.yaml` | Multi-ECU fault scenarios |
| FTT coverage report | `build/pipeline/ftt_coverage.json` | FMEA ID → test mapping, HE gap list |
| HARA coverage detail | `build/pipeline/hara_coverage.json` | HE ID → safety path → promoted test mapping |

---

### 13.3 Daily Tasks and Deliverables

#### Day 1 — Baseline Establishment

| # | Task | Command / Action | Deliverable |
|---|------|-----------------|-------------|
| 1.1 | Run traceability check, record baseline SWR coverage | `make traceability` | `build/traceability_baseline.json` |
| 1.2 | Dry-run RBT generator to count total SWRs and gaps | `gen_rbt_tests --dry-run` | stdout: `NNN SWRs, MMM uncovered` |
| 1.3 | Dry-run FTT generator to count FMEA failure modes | `gen_ftt_tests --dry-run` | stdout: `NNN failure modes, MMM uncovered` |
| 1.4 | Document gap list and HARA HE map | Manual from §13.2.3 | `docs/plans/rbt_gap_list.md` committed |

**Exit criterion:** Baseline coverage percentage established and committed. Gap list in `docs/plans/`.

**Compliance prerequisite:** Run `bash scripts/setup-misra-rules.sh` to prepare the cppcheck MISRA environment (§14.7 C1.1). No generated stubs exist yet; §14.7 C1 MISRA baseline scanning begins on Day 2 concurrently with stub generation.

---

#### Day 2 — RBT Generator Implementation

| # | Task | Command / Action | Deliverable |
|---|------|-----------------|-------------|
| 2.1 | Implement `tools/pipeline/gen_rbt_tests.py` | Code + unit tests | `gen_rbt_tests.py` passing pytest |
| 2.2 | Run generator on SWR-BSW (highest priority) | `gen_rbt_tests --swr SWR-BSW.md` | `test_Com_rbt_generated.c`, `test_E2E_rbt_generated.c`, `test_WdgM_rbt_generated.c` |
| 2.3 | Review generated stubs — remove false positives | Manual review | Annotated file with false-positive markers |
| 2.4 | Promote ≥ 3 stubs: replace `TEST_IGNORE()` with real assertions | Edit + `make test` | 3 new PASS tests in CI |
| 2.5 | Update traceability matrix with new `@verifies` tags | `make traceability` | `traceability-matrix.md` updated |
| 2.6 | **[MISRA gate — §14.7 C1 concurrent]** Run cppcheck MISRA scan on all generated stubs; capture baseline violation log | `find test/unit/bsw -name '*_rbt_generated.c' \| xargs cppcheck --addon=tools/misra/misra.json --enable=style,warning --std=c99 --platform=unix32 --suppress=missingIncludeSystem --suppressions-list=tools/misra/suppressions.txt --inline-suppr --error-exitcode=1 -I firmware/bsw/include -I firmware/bsw/services -I firmware/bsw/rte 2>&1 \| tee build/misra-baseline-generated.txt; true` | `build/misra-baseline-generated.txt` committed as SWE.5 WP04 seed artifact |
| 2.7 | **[MISRA triage — §14.7 C1.3/C1.4]** Classify each violation: required vs. advisory (§14.2.2 table); annotate rows as `DEV match` or `new deviation needed` against `docs/safety/analysis/misra-deviation-register.md` | Manual triage + grep of deviation register | Triage table appended to `docs/plans/rbt_gap_list.md`; required-rule violation count recorded |

**Exit criterion:** `gen_rbt_tests.py` operational; ≥ 3 SWR-BSW stubs promoted to passing tests; CI green. **MISRA baseline scan complete (§14.7 C1): required-rule violation count recorded in `build/misra-baseline-generated.txt`; triage table committed to `docs/plans/rbt_gap_list.md`.**

---

#### Day 3 — RBT Coverage Expansion and CI Gate

| # | Task | Command / Action | Deliverable |
|---|------|-----------------|-------------|
| 3.1 | Run generator on SWR-CVC and SWR-FZC | `gen_rbt_tests --swr SWR-CVC.md SWR-FZC.md` | ECU-specific stub files |
| 3.2 | Promote all ASIL D stubs to real tests | Manual review + edit | ASIL D coverage at 100% |
| 3.3 | Add `codegen:rbt-coverage` CI step (blocking at ≥ 80% for SWR-BSW) | Edit `.github/workflows/*.yml` | CI step green |
| 3.4 | Confirm coverage report shows ≥ 80% SWR-BSW promoted coverage | `gen_rbt_tests --report` | `rbt_coverage.json` |
| 3.5 | Commit stubs in separate `chore(testgen):` commit | `git commit -m "chore(testgen): add RBT stubs SWR-BSW"` | Committed |
| 3.6 | **[MISRA required-rule fix — §14.7 C2 concurrent]** Before promoting any ASIL D stub: fix Rules 17.7 (`(void)` casts), 15.7 (final `else` branches), 20.1 (`#include` order), 8.4/8.5 (prototypes/externs); add `DEV-NNN` deviation register entries for any pattern not covered by DEV-001/DEV-002 | Edit stubs per §14.2.2; re-run `cppcheck --error-exitcode=1` on each file | Exit code 0 on cppcheck for every ASIL D stub being promoted; new DEV-NNN rows committed to `docs/safety/analysis/misra-deviation-register.md` |
| 3.7 | **[ISO 26262 promotion gate — §14.4 checklist]** For each promoted ASIL D stub, complete the full §14.4 pre-promotion checklist (MISRA gate + ISO 26262 Checks 1–4 + TEST_QUALITY block); embed signed checklist in the `chore(testgen):` commit body | Manual: tick all §14.4 boxes; verify `@verifies SWR-<ID>` and `@asil D` annotations present; confirm `TEST_IGNORE()` removed and `// AI_GENERATED_STUB` marker stripped | Each promotion commit body contains signed §14.4 checklist; ASIL D stubs carry `@verifies` + `@asil D`; no stub promoted with a remaining required-rule violation |

**Exit criterion:** ≥ 80% SWR-BSW promoted coverage; ASIL D requirements at 100%; CI `codegen:rbt-coverage` green. **§14.7 C2 complete: every promoted ASIL D stub passes `cppcheck --error-exitcode=1`; §14.4 checklist signed in each promotion commit; all new MISRA suppressions backed by a DEV-NNN entry.**

---

#### Day 4 — FTT Parser and Injection Mapping

| # | Task | Command / Action | Deliverable |
|---|------|-----------------|-------------|
| 4.1 | Implement `tools/pipeline/gen_ftt_tests.py` — FMEA parser + injection classifier | Code + unit tests | `gen_ftt_tests.py` parsing `fmea.md` without errors |
| 4.2 | Run `--dry-run` to produce full injection mapping table | `gen_ftt_tests --dry-run` | stdout: all failure modes classified |
| 4.3 | Cross-check mapping against SP-01 … SP-15 in §9 | Manual + `--hara-coverage` flag | `hara_coverage.json` |
| 4.4 | Identify HARA HE gaps per §13.2.3 (at minimum: HE-005) | `gen_ftt_tests --hara-coverage` | Gap list confirmed |
| 4.5 | Generate FTT C stubs for uncovered BSW-level failure modes | `gen_ftt_tests --output-c test/integration` | `test_int_ftt_*_generated.c` compiling |
| 4.6 | **[MISRA gate — §14.7 C1 procedure applied to FTT stubs]** Run cppcheck MISRA scan on all generated FTT C stubs; record violation baseline | `find test/integration -name '*_ftt_*_generated.c' \| xargs cppcheck --addon=tools/misra/misra.json --enable=style,warning --std=c99 --platform=unix32 --suppress=missingIncludeSystem --suppressions-list=tools/misra/suppressions.txt --inline-suppr --error-exitcode=1 -I firmware/bsw/include -I firmware/bsw/services -I firmware/bsw/rte 2>&1 \| tee build/misra-ftt-baseline.txt; true` | `build/misra-ftt-baseline.txt`; 0 required-rule violations in committed stubs (required-rule violations must be fixed before commit) |
| 4.7 | **[ISO 26262 traceability pre-check — §14.3.2 Checks 1–3]** Verify every FTT stub carries: `@verifies FMEA-<ID>` (Check 1: source ID exists in `docs/safety/analysis/fmea.md`), exact annotation match (Check 2), correct `@asil` tag (Check 3) | `grep -rn "@verifies\|@asil" test/integration/*_ftt_*_generated.c`; cross-check each FMEA-ID against `fmea.md`; cross-check `@asil` against FMEA ASIL column | All FTT stubs carry valid `@verifies FMEA-<ID>` and `@asil` tags; Checks 1–3 columns of §14.4 checklist ticked for each stub |

**Exit criterion:** `gen_ftt_tests.py` operational; all FMEA failure modes classified; ≥ 1 FTT C stub compiling under `make -C test/integration build`. **FTT stubs carry valid `@verifies FMEA-<ID>` and `@asil` annotations (§14.3.2 Checks 1–3 verified); MISRA scan recorded with 0 required-rule violations (`build/misra-ftt-baseline.txt`).**

---

#### Day 5 — Fault Injection Scenarios and SIL Integration

| # | Task | Command / Action | Deliverable |
|---|------|-----------------|-------------|
| 5.1 | Generate YAML scenario for HE-005 (brake PDU timeout → SAFE_STOP) | `gen_ftt_tests --output-yaml gateway/fault_inject/scenarios/ai_generated` | `ftt_HE-005.yaml` |
| 5.2 | Execute YAML scenario via `fault_inject` test runner against SIL | `python gateway/fault_inject/test_runner.py --scenario ftt_HE-005.yaml` | PASS or documented expected failure |
| 5.3 | Promote FTT-001 C stub to passing integration test | Edit `test_int_ftt_HE005_generated.c`, `make -C test/integration test` | 1 new passing FTT integration test |
| 5.4 | Add `codegen:ftt-coverage` CI step | Edit `.github/workflows/*.yml` | CI step green |
| 5.5 | Append new safety path SP-16 (FTT-001: brake PDU timeout) to §9 of this document | Edit ITP §9 | SP-16 added to safety path table |
| 5.6 | Commit FTT artifacts in separate `chore(testgen):` commit | `git commit -m "chore(testgen): add FTT scenarios for HARA HE gaps"` | Committed |
| 5.7 | **[ISO 26262 traceability Checks 4–6 — §14.7 C3 concurrent]** For every promoted stub: Check 4 — verify assertions are deterministic (no `TEST_ASSERT_TRUE(1)` or constant-expression bodies); Check 5 — confirm `gcov` branch coverage delta ≥ 1% on module under test (`make -C test/integration coverage`); Check 6 — update traceability matrix (`make traceability`) and commit | Manual code review for Check 4; `make -C test/integration coverage` for Check 5; `make traceability` for Check 6 | All 6 §14.3.2 traceability checks pass for every promoted stub; per-stub branch delta ≥ 1% confirmed in `build/coverage/lcov-report/`; `traceability-matrix.md` committed with new bidirectional links |
| 5.8 | **[Full compliance sign-off — §14.7 C4 + C5]** Confirm all four §14.5 CI compliance gates pass on clean branch: `codegen:misra-check-stubs`, `codegen:rbt-coverage`, `codegen:ftt-coverage`, `codegen:traceability`; verify §14.3.4 ASIL D metrics met (`rbt_coverage.json`, `ftt_coverage.json`, `hara_coverage.json`, `branch_coverage_pct` ≥ 80%); compile AI-Generated Test Promotion Summary table for ITR per §14.6 | CI run summary review; `python -m tools.pipeline gen_rbt_tests --report`; `python -m tools.pipeline gen_ftt_tests --hara-coverage`; `make -C test/integration coverage` | All four CI stages green; §14.3.4 thresholds met (ASIL D `stub_only_pct` = 0%, `requirement_coverage_pct` = 100%, `hara_he_coverage_pct` = 100%); AI-Generated Test Promotion Summary table added to `docs/aspice/verification/integration-test/itr-template.md` |

**Exit criterion:** ≥ 1 YAML FTT scenario executes against SIL; all HARA HEs have ≥ 1 test (promoted or traceable to existing SP); CI `codegen:ftt-coverage` green. **§14.7 C3–C5 complete: all 6 ISO 26262 traceability checks pass for every promoted stub; all four §14.5 CI compliance gates green; §14.3.4 ASIL D coverage metrics met; SWE.5 WP01–WP05 evidence artifacts committed.**

---

### 13.4 CI Integration

The two new CI stages added by this section:

| CI Stage | Command | Blocking | Added Day |
|----------|---------|----------|-----------|
| `codegen:rbt-coverage` | `gen_rbt_tests --report --fail-below-promoted 80` | Yes (SWR-BSW only) | Day 3 |
| `codegen:ftt-coverage` | `gen_ftt_tests --hara-coverage --fail-on-uncovered-asil-d` | Yes | Day 5 |

`--fail-below-promoted 80`: blocks CI if fewer than 80% of SWR-BSW requirements have at least one promoted (non-stub) test. Threshold rises to 100% for ASIL D requirements once Day 3 promotion is complete.

`--fail-on-uncovered-asil-d`: blocks CI if any ASIL D FMEA failure mode has no traceable test (promoted or existing safety path).

---

### 13.5 Safety Constraints for AI-Generated Tests

1. **Generated stubs are QM until promoted.** Any test function retaining the `// AI_GENERATED_STUB` marker does not count toward ASIL D or ASIL C coverage metrics in `rbt_coverage.json` or the traceability matrix.

2. **Never hand-edit generated stub files directly.** If the stub template is wrong, fix `gen_rbt_tests.py` or `gen_ftt_tests.py` and regenerate. This mirrors `development-discipline.md §1`.

3. **FMEA is the source of truth for fault test cases.** Every FTT test must carry a `@verifies FMEA-<ID>` or `@verifies HE-<NNN>` annotation. Tests created from memory without a FMEA trace are not valid FTT tests.

4. **HITL-LOCK blocks in safety documents must not be parsed or modified.** Both generators must skip content inside `<!-- HITL-LOCK … -->` markers in HARA and SWR documents. Rule applies to reading and writing.

5. **Generated YAML fault scenarios stay within the QM gateway boundary.** All YAML scenarios are executed by `gateway/fault_inject/test_runner.py` on the gateway tier. They must not issue raw CAN frames that bypass the ECU BSW stack or directly set ASIL D actuator signals.

6. **MISRA gate still applies to promoted C stubs.** Once `// AI_GENERATED_STUB` is removed and the test is part of the ASIL suite, it must pass `tools/misra/` cppcheck (0 violations) if it includes firmware headers.

---

### 13.6 Traceability Chain for AI-Generated Tests

```
FMEA failure mode / SWR requirement
    │
    ▼
AI generator  (tools/pipeline/gen_rbt_tests.py OR gen_ftt_tests.py)
    │
    ▼  file: test/unit/bsw/test_<Module>_rbt_generated.c
       OR test/integration/test_int_ftt_<FMEA_ID>_generated.c
       Body: TEST_IGNORE(); // AI_GENERATED_STUB
    │
    ▼  Human review: replace TEST_IGNORE(), remove AI_GENERATED_STUB marker
       Add @verifies <SWR-ID> or @verifies <FMEA-ID>
    │
    ▼  Integration test suite passes  (make -C test/integration test)
    │
    ▼  CI gate  (codegen:rbt-coverage, codegen:ftt-coverage)
    │
    ▼  Traceability matrix update  (docs/aspice/traceability/traceability-matrix.md)
    │
    ▼  Coverage reports  (build/pipeline/rbt_coverage.json, ftt_coverage.json)
```

---

*AI-driven test generation section added 2026-03-27.*
*Relates to: `docs/integration_audit.md §12.3 Zone C`, `docs/integration_audit.md §12.4`, `docs/plans/plan-ai-assisted-codegen.md §4`.*

---

## 14. AI-Assisted Compliance Checking

This section defines the verification processes through which AI-generated test artifacts (from §13) are checked for MISRA C:2012 conformance and ISO 26262 compliance before they may be promoted to the ASIL test suite.

**Safety constraint (inherited from §13.5):** AI generation is a QM-level tool step. Compliance checking occurs at the human-promotion gate — it does NOT happen autonomously. No AI-generated artifact carries ASIL credit until both the MISRA gate and the ISO 26262 traceability checks have passed and a human reviewer removes the `// AI_GENERATED_STUB` marker.

---

### 14.1 Compliance Checking Overview

AI-generated C stubs and YAML fault scenarios must pass two independent compliance axes before promotion:

```
AI_GENERATED_STUB (test/unit/bsw/ or test/integration/)
         │
         ▼
  ┌─────────────────────────────────────────────┐
  │  Axis 1 — MISRA C:2012                      │
  │  tools/misra/  cppcheck 2.13+               │
  │  0 required-rule violations                  │
  │  Suppressions from suppressions.txt apply    │
  └──────────────┬──────────────────────────────┘
                 │
                 ▼
  ┌─────────────────────────────────────────────┐
  │  Axis 2 — ISO 26262 Part 6 Traceability     │
  │  @verifies annotation present               │
  │  Requirement exists in SWR / FMEA doc       │
  │  ASIL tag matches requirement ASIL          │
  │  Coverage count updated in matrix           │
  └──────────────┬──────────────────────────────┘
                 │
                 ▼
  Human reviewer removes // AI_GENERATED_STUB
  Stub promoted → counts toward ASIL metrics
```

---

### 14.2 MISRA C:2012 Verification for AI-Generated Stubs

#### 14.2.1 Applicable Standard and Tool

| Attribute | Detail |
|-----------|--------|
| Standard | MISRA C:2012 (required + mandatory rules) |
| Tool | cppcheck 2.13+ with MISRA addon (`tools/misra/misra.json`) |
| Rule texts | Downloaded per-run via `scripts/setup-misra-rules.sh` (not committed) |
| Suppression file | `tools/misra/suppressions.txt` (committed — see `plan-misra-pipeline.md §Phase 7`) |
| Exit code | `--error-exitcode=1` (blocking — 0 required-rule violations) |
| Reference | `docs/safety/analysis/misra-deviation-register.md` (formal deviations DEV-001, DEV-002) |

Full pipeline history, violation categories, and suppression rationale are documented in `docs/plans/plan-misra-pipeline.md`.

#### 14.2.2 Common MISRA Violations in AI-Generated C

AI-generated Unity test stubs frequently violate the following rules. Reviewers must check for these before promotion:

| MISRA Rule | Category | Typical AI Pattern | Required Fix |
|------------|----------|--------------------|--------------|
| 8.4 | Required | Test function defined without prior declaration in header | Add prototype to a shared stub header or `// cppcheck-suppress` with justification |
| 8.5 | Required | `extern` declaration repeated inside `.c` (copies from context) | Remove duplicate extern; rely on `#include` of the BSW header |
| 11.5 | Required | `void *` cast when accessing PDU buffer data | Governed by DEV-001 (Com.c AUTOSAR pattern); apply same deviation if pattern matches |
| 15.5 | Advisory | Multiple `return` in test helper | Suppressed globally (`suppressions.txt`) — defensive coding pattern |
| 15.7 | Required | `if-else-if` chain without final `else` in fault-path assertions | Add `else { TEST_FAIL_MESSAGE("unexpected path"); }` |
| 17.7 | Required | BSW API return value discarded (`Dem_ReportErrorStatus(…)`) | Add `(void)` cast: `(void)Dem_ReportErrorStatus(…)` |
| 17.8 | Advisory | Test loop counter modified inside body | Suppressed globally — accepted for decrement-and-retry patterns |
| 2.5 | Advisory | Macro from BSW header unused in this translation unit | Suppressed globally — inherent to shared-header architecture |
| 20.1 | Required | `#include` below type definitions in stub header | Move all includes to top of file |
| 9.3 | Advisory | `uint8 buf[] = {0u}` zero-init pattern | Suppressed globally — standard embedded C99 init |

#### 14.2.3 Pre-Promotion MISRA Scan Procedure

Run the following before removing `// AI_GENERATED_STUB` from any C stub:

```bash
# 1. Ensure rule texts are present (one-time per environment)
bash scripts/setup-misra-rules.sh

# 2. Scan the specific generated file (unit test stubs)
cd firmware
cppcheck \
    --addon=../tools/misra/misra.json \
    --enable=style,warning \
    --std=c99 \
    --platform=unix32 \
    --suppress=missingIncludeSystem \
    --suppressions-list=../tools/misra/suppressions.txt \
    --inline-suppr \
    --error-exitcode=1 \
    -I shared/bsw/include \
    -I shared/bsw/mcal \
    -I shared/bsw/ecual \
    -I shared/bsw/services \
    -I shared/bsw/rte \
    ../test/unit/bsw/test_<Module>_rbt_generated.c

# 3. Scan integration FTT stub (integration test stubs)
cppcheck \
    --addon=../tools/misra/misra.json \
    --enable=style,warning \
    --std=c99 \
    --platform=unix32 \
    --suppress=missingIncludeSystem \
    --suppressions-list=../tools/misra/suppressions.txt \
    --inline-suppr \
    --error-exitcode=1 \
    -I shared/bsw/include \
    -I shared/bsw/mcal \
    -I shared/bsw/ecual \
    -I shared/bsw/services \
    -I shared/bsw/rte \
    ../test/integration/test_int_ftt_<FMEA_ID>_generated.c
```

Exit code 0 = MISRA-clean and ready for promotion. Exit code 1 = violations must be fixed first.

#### 14.2.4 New Deviation Register Entries for AI Patterns

If the AI stub introduces a pattern that is not covered by an existing deviation (DEV-001, DEV-002) but is structurally justified, the reviewer must add a new entry to `docs/safety/analysis/misra-deviation-register.md` before the suppression is added. The register format is:

| Field | Content |
|-------|---------|
| Deviation ID | `DEV-NNN` (increment from last entry) |
| Rule | MISRA C:2012 Rule X.Y |
| Location | `test/…/filename.c:line` |
| Technical justification | Why the pattern is safe in this context |
| Risk | Probability and severity of misuse |
| Compensating measure | What prevents the hazard (e.g., Unity assertions, bounded arrays) |
| Reviewer | Name + date |

Suppressions added for AI-stub deviations must use file-scoped form (`misra-c2012-X.Y:*/test_*_generated.c`) rather than global suppression, to prevent masking real violations in production firmware.

---

### 14.3 ISO 26262 Part 6 Compliance Verification

#### 14.3.1 Applicable ISO 26262 Clauses

| Clause | Title | Compliance Check |
|--------|-------|-----------------|
| Part 6 §8.4.6 | Use of software tools (coding guidelines) | MISRA gate (§14.2) must pass before promotion |
| Part 6 §9.4.2 | Equivalence classes and boundary values | FTT boundary tests (e.g., INT-010 T5) must use equivalence-class injection, not arbitrary values |
| Part 6 §9.4.3 | Error guessing / fault injection | FTT stubs must derive from FMEA (`@verifies FMEA-<ID>`) — not from intuition |
| Part 6 §9.4.4 | Statement and branch coverage | Promoted stubs must increase `gcov` branch coverage on the module under test (0-coverage added stubs are rejected) |
| Part 6 §10.4.5 | Pass/fail criteria | Promoted stubs must have deterministic assertions — no `TEST_IGNORE()`, no floating assertions |
| Part 6 §10.4.7 | Regression testing | Promoted stubs are part of the regression gate — CI must pass 60/60 tests after promotion |
| Part 6 §7.2.6 | Safety analyses (FMEA) | FTT stubs must remain traceable to the source FMEA row through their lifetime |

#### 14.3.2 ISO 26262 Traceability Check for AI-Generated Tests

Before promoting any stub, the reviewer verifies the complete traceability chain:

```
SWR requirement ID (RBT)  OR  FMEA failure mode ID (FTT)
         │
         ▼ (verified in SWR-BSW.md or docs/safety/analysis/fmea.md)
Exists in requirement / FMEA document?                  ← CHECK 1
         │
         ▼
@verifies annotation in stub matches the source ID?     ← CHECK 2
         │
         ▼
ASIL tag (@asil D|C|B|QM) matches requirement ASIL?     ← CHECK 3
         │
         ▼
Test assertion is non-trivial (not always-pass)?        ← CHECK 4
         │
         ▼
Branch coverage increases on module under test?         ← CHECK 5
         │
         ▼
Traceability matrix updated after promotion?            ← CHECK 6
```

All 6 checks must pass. A stub failing any check stays as `TEST_IGNORE()` until resolved.

#### 14.3.3 ASIL-Level Compliance Thresholds

These thresholds are enforced by the CI coverage gates (`codegen:rbt-coverage`, `codegen:ftt-coverage`) defined in §13.4, and supplement the exit criteria from §13.1.5:

| ASIL | MISRA Gate | Traceability Chain | Branch Coverage Delta | Threshold |
|------|-----------|--------------------|-----------------------|-----------|
| ASIL D | 0 required-rule violations | All 6 checks pass | ≥ 1% increase on module under test | 100% promoted |
| ASIL C | 0 required-rule violations | Checks 1–4 pass | ≥ 1% increase on module under test | 100% promoted |
| ASIL B | 0 required-rule violations | Checks 1–3 pass | Documented | ≥ 80% promoted |
| QM | Pass or documented suppression | Check 1 pass | No requirement | ≥ 80% stub or promoted |

**ASIL D hard rule:** An ASIL D stub that cannot achieve ≥ 1% branch coverage increase on the target module (e.g., because the path it exercises is already covered) must be flagged as duplicate coverage and removed, not promoted as a dead test.

#### 14.3.4 ISO 26262 Coverage Metric Computation

After all promoted stubs are merged, the CI `codegen:rbt-coverage` step emits the following metrics to `build/pipeline/rbt_coverage.json`:

| Metric | Formula | ASIL D Gate |
|--------|---------|-------------|
| `requirement_coverage_pct` | promoted tests with `@verifies SWR-X` / total SWR-X requirements | ≥ 100% |
| `fmea_coverage_pct` | promoted FTT tests / total FMEA failure modes | ≥ 100% (ASIL D) |
| `hara_he_coverage_pct` | safety paths SP-01…SP-NN / total HARA HE items | ≥ 100% |
| `branch_coverage_pct` | gcov branches hit / total branches in BSW source | ≥ 80% (MC/DC target for ASIL D) |
| `stub_only_pct` | stubs with `TEST_IGNORE()` / total generated | Must be 0% for ASIL D stubs |

These metrics feed the ASPICE SWE.5 verification evidence in the Integration Test Report (ITR).

---

### 14.4 Pre-Promotion Checklist

The following checklist is mandatory for any stub transitioning from `TEST_IGNORE()` to a real assertion. It is signed off by the reviewer in the commit message.

```
AI-GENERATED STUB PROMOTION CHECKLIST
======================================
Stub file:  ___________________________________
Stub ID:    test_<SWR_ID or FMEA_ID>_<label>
Reviewer:   ___________________________________
Date:       ___________________________________

MISRA GATE
[ ] Pre-promotion cppcheck scan: exit code 0 (see §14.2.3)
[ ] No new entries added to suppressions.txt without deviation register entry
[ ] No global suppressions added (file-scope only for test files)

ISO 26262 TRACEABILITY (§14.3.2)
[ ] Check 1: SWR / FMEA source ID verified in source document
[ ] Check 2: @verifies annotation matches source ID exactly
[ ] Check 3: @asil tag matches requirement ASIL classification
[ ] Check 4: Assertion is deterministic and non-trivially always-pass
[ ] Check 5: gcov branch coverage increases ≥ 1% on target module
[ ] Check 6: traceability-matrix.md updated with new @verifies link

TEST QUALITY
[ ] TEST_IGNORE() replaced with real assertion
[ ] // AI_GENERATED_STUB marker removed
[ ] make -C test/integration test (or make -C firmware test): 60/60 PASS
[ ] No debug fprintf / printf traces left in stub
[ ] setUp() resets all mock state before assertion

COMMIT
[ ] Stub committed in separate chore(testgen): commit (per development-discipline.md §7)
[ ] Commit message cites stub ID and promoted SWR or FMEA ID
```

---

### 14.5 CI Integration for Compliance Gates

Two new CI stages are added to the MISRA workflow (`.github/workflows/misra.yml`) to extend compliance checking to AI-generated test stubs:

| CI Stage | Trigger | Command | Blocking | Purpose |
|----------|---------|---------|----------|---------|
| `codegen:misra-check-stubs` | Push to `test/**/*_generated.c` | `cppcheck` on all `*_generated.c` matching the suppression profile | Yes | Prevents MISRA violations from entering generated test files at commit time |
| `codegen:rbt-coverage` | Push to `test/**` or `docs/aspice/traceability/**` | `gen_rbt_tests --report --fail-below-promoted 80` | Yes (SWR-BSW only) | Enforces coverage thresholds |
| `codegen:ftt-coverage` | Push to `test/**` or `docs/safety/analysis/fmea.md` | `gen_ftt_tests --hara-coverage --fail-on-uncovered-asil-d` | Yes | Enforces ASIL D FMEA coverage |
| `codegen:traceability` | Push to `docs/**` or `test/**` | `tools/trace/gen_traceability_matrix.py --validate` | Yes | Bidirectional SWR ↔ test link validation |

**`codegen:misra-check-stubs` detail:**

```yaml
# .github/workflows/misra.yml (additional job)
misra-check-generated-stubs:
  name: MISRA check AI-generated stubs
  runs-on: ubuntu-24.04
  if: >
    github.event_name == 'push' &&
    contains(github.event.head_commit.modified, 'test/')
  steps:
    - uses: actions/checkout@v4
    - run: sudo apt-get install -y cppcheck
    - run: bash scripts/setup-misra-rules.sh
    - name: Scan generated stubs
      run: |
        find test/ -name '*_generated.c' | xargs -I{} cppcheck \
          --addon=tools/misra/misra.json \
          --enable=style,warning \
          --std=c99 --platform=unix32 \
          --suppress=missingIncludeSystem \
          --suppressions-list=tools/misra/suppressions.txt \
          --inline-suppr \
          --error-exitcode=1 \
          -I firmware/bsw/include \
          -I firmware/bsw/mcal \
          -I firmware/bsw/ecual \
          -I firmware/bsw/services \
          -I firmware/bsw/rte \
          {}
```

This stage runs on every commit that modifies a `*_generated.c` file — whether the stub is still `TEST_IGNORE()` or has been promoted. It catches any MISRA violations introduced by the generator templates themselves before they reach human review.

---

### 14.6 Compliance Evidence for ASPICE SWE.5

The compliance checking artifacts produced by this section contribute to the ASPICE SWE.5 Integration Test Report (ITR) as follows:

| Evidence Artifact | Location | Contributes To |
|-------------------|----------|----------------|
| MISRA scan log for promoted stubs | `build/misra-report.txt` (CI artifact) | SWE.5 WP04 (verification evidence) |
| Deviation register | `docs/safety/analysis/misra-deviation-register.md` | SWE.5 WP05 (non-conformance evidence) |
| RBT coverage report | `build/pipeline/rbt_coverage.json` | SWE.5 WP03 (test coverage evidence) |
| FTT coverage report | `build/pipeline/ftt_coverage.json` | SWE.5 WP03 (fault coverage evidence) |
| HARA coverage report | `build/pipeline/hara_coverage.json` | SWE.5 WP03 (safety path evidence) |
| Promotion checklist (per stub) | Commit message body | SWE.5 WP04 (review record) |
| Traceability matrix | `docs/aspice/traceability/traceability-matrix.md` | SWE.5 WP02 (bidirectional traceability) |
| gcov branch coverage report | `build/coverage/lcov-report/` | SWE.5 WP03 (structural coverage) |

**ISO 26262 Part 6 §10 review note:** The integration test report must declare which tests were AI-generated-and-promoted and confirm that all such tests have passed the pre-promotion checklist (§14.4). The ITR template (`docs/aspice/verification/integration-test/`) must include an "AI-Generated Test Promotion Summary" table listing stub IDs, source IDs, MISRA status, and promotion date.

---

### 14.7 Daily Tasks for AI Compliance Checking

This sub-section schedules the day-by-day execution of MISRA C:2012 and ISO 26262 compliance verification for AI-generated test stubs. It runs concurrently with §13.3 Days 2–5: stubs are generated in §13.3 and compliance-checked here before promotion.

**Prerequisite:** §13.3 Day 1 (baseline) must be complete. At least one `*_generated.c` stub must exist in `test/unit/bsw/` or `test/integration/` before this plan's Day 1 begins.

---

#### Day 1 — MISRA Baseline Scan

| # | Task | Command / Action | Deliverable |
|---|------|-----------------|-------------|
| C1.1 | Download MISRA rule texts if not present (one-time per environment) | `bash scripts/setup-misra-rules.sh` | `tools/misra/misra_c_2023__headlines_for_cppcheck.txt` present |
| C1.2 | Run cppcheck on all `*_generated.c` stubs; capture full output | `find test/ -name '*_generated.c' \| xargs cppcheck --addon=tools/misra/misra.json --enable=style,warning --std=c99 --error-exitcode=1 -I firmware/bsw/include ... 2>&1 \| tee build/misra-baseline-generated.txt` | `build/misra-baseline-generated.txt` |
| C1.3 | Count violations by rule; classify each as required vs. advisory using §14.2.2 table | Manual triage against §14.2.2 | Triage table committed to `docs/plans/rbt_gap_list.md` (appended section) |
| C1.4 | Identify patterns that match existing deviations DEV-001/DEV-002 | Compare against `docs/safety/analysis/misra-deviation-register.md` | Triage table annotated: `DEV match` or `new deviation needed` |
| C1.5 | Add file-scoped `suppressions.txt` entries for confirmed advisory rules only | Edit `tools/misra/suppressions.txt`; form: `misra-c2012-X.Y:*/test_*_generated.c` — no global suppressions | `suppressions.txt` updated and committed |

**Exit criterion:** Baseline violation count recorded; every required-rule violation categorised; no advisory rules suppressed globally. `build/misra-baseline-generated.txt` committed as SWE.5 evidence artifact.

---

#### Day 2 — MISRA Required-Rule Fix and Deviation Register

| # | Task | Command / Action | Deliverable |
|---|------|-----------------|-------------|
| C2.1 | Fix Rule 17.7 — add `(void)` cast on discarded BSW API return values | Edit stubs: `(void)Dem_ReportErrorStatus(…)`, `(void)BswM_RequestMode(…)` | 0 Rule 17.7 violations on re-scan |
| C2.2 | Fix Rule 15.7 — add final `else` branch to all `if-else-if` fault-path chains | Edit stubs: `else { TEST_FAIL_MESSAGE("unexpected path"); }` | 0 Rule 15.7 violations on re-scan |
| C2.3 | Fix Rule 20.1 — move all `#include` directives above type definitions | Edit stubs and any generated stub headers | 0 Rule 20.1 violations on re-scan |
| C2.4 | Fix Rule 8.4/8.5 — add missing prototypes or remove duplicated `extern` declarations | Add prototypes to shared stub header or remove duplicated externs copied from context | 0 Rule 8.4/8.5 violations on re-scan |
| C2.5 | Add new deviation register entries (DEV-NNN) for any pattern not covered by DEV-001/DEV-002 | Edit `docs/safety/analysis/misra-deviation-register.md` per §14.2.4 format | New DEV-NNN entries committed; no suppression added without a register entry |
| C2.6 | Re-run cppcheck on all `*_generated.c`; verify exit code 0 | `find test/ -name '*_generated.c' \| xargs cppcheck --error-exitcode=1 ...` | Exit code 0 for every generated stub |

**Exit criterion:** All required-rule violations eliminated; cppcheck exits 0 on all `*_generated.c` files; every new suppression is backed by a signed deviation register entry.

---

#### Day 3 — ISO 26262 Traceability Verification

| # | Task | Command / Action | Deliverable |
|---|------|-----------------|-------------|
| C3.1 | Check 1 — verify SWR or FMEA source ID exists in the source document | Search `docs/aspice/software/sw-requirements/SWR-BSW.md` and `docs/safety/analysis/fmea.md` | Check 1 column in §14.4 promotion checklist for each stub |
| C3.2 | Check 2 — verify `@verifies` annotation in stub matches source ID exactly (no typos, no aliases) | `grep -rn "@verifies" test/unit/bsw/*_rbt_generated.c test/integration/*_generated.c` | Check 2 column; mismatches corrected in stub |
| C3.3 | Check 3 — verify `@asil` tag matches requirement ASIL classification | Cross-reference SWR ASIL column and FMEA ASIL field for each stub | Check 3 column; downgraded tags corrected |
| C3.4 | Check 4 — verify each promoted assertion is deterministic and non-trivially always-pass | Manual code review of every promoted test body; flag any `TEST_ASSERT_TRUE(1)` or constant-expression assertions | Check 4 column; trivial assertions rewritten |
| C3.5 | Check 5 — run gcov and confirm branch coverage delta ≥ 1% on module under test | `make -C test/integration coverage`; compare per-module branch hit count before and after adding stub | `build/coverage/lcov-report/` per-stub delta ≥ 1%; stubs with 0% delta flagged as duplicate coverage and removed |
| C3.6 | Check 6 — update traceability matrix for all promoted stubs | `make traceability` → review diff; commit `docs/aspice/traceability/traceability-matrix.md` | `traceability-matrix.md` updated; bidirectional SWR ↔ test link validated |

**Exit criterion:** All 6 ISO 26262 traceability checks pass for every promoted stub; traceability matrix committed; gcov branch delta ≥ 1% confirmed per stub; any stub failing Check 5 removed rather than promoted as dead test.

---

#### Day 4 — CI Compliance Gate Implementation and Verification

| # | Task | Command / Action | Deliverable |
|---|------|-----------------|-------------|
| C4.1 | Implement `codegen:misra-check-stubs` CI job in `.github/workflows/misra.yml` | Add job per §14.5 YAML template; trigger on push to `test/**/*_generated.c` | CI job present in workflow file |
| C4.2 | Inject a known required-rule violation into a stub and confirm CI blocks | Add un-cast `Dem_ReportErrorStatus()` call to one stub; push; observe CI exit code 1 | CI blocks merge; violation message visible in job log |
| C4.3 | Revert injected violation; confirm CI returns to green | Revert edit; push | `codegen:misra-check-stubs` exit code 0 |
| C4.4 | Implement `codegen:traceability` CI step | `tools/trace/gen_traceability_matrix.py --validate`; trigger on push to `docs/**` or `test/**` | `codegen:traceability` CI step present and green |
| C4.5 | Confirm all four §14.5 compliance CI stages pass on clean branch: `codegen:misra-check-stubs`, `codegen:rbt-coverage`, `codegen:ftt-coverage`, `codegen:traceability` | Review CI run summary page | All four stages green; thresholds enforced |
| C4.6 | Document CI gate thresholds (§14.3.3 table) in ITR template | Append gate table to `docs/aspice/verification/integration-test/itr-template.md` | ITR template updated and committed |

**Exit criterion:** All four compliance CI stages green on clean branch; MISRA gate correctly blocks on injected violation and passes after revert; ITR template includes CI gate thresholds.

---

#### Day 5 — ASPICE SWE.5 Evidence Packaging and Sign-Off

| # | Task | Command / Action | Deliverable |
|---|------|-----------------|-------------|
| C5.1 | Run full MISRA scan on all promoted stubs; save report as CI evidence artifact | `make -C firmware misra-report` | `build/misra-report.txt` — exit code 0, retained as SWE.5 WP04 artifact |
| C5.2 | Confirm `rbt_coverage.json` meets all §14.3.4 ASIL D thresholds: ASIL D promoted = 100%, SWR-BSW promoted ≥ 80%, `stub_only_pct` = 0% for ASIL D | `python -m tools.pipeline gen_rbt_tests --report` | `build/pipeline/rbt_coverage.json` — all §14.3.4 metrics met |
| C5.3 | Confirm `ftt_coverage.json` shows all ASIL D FMEA failure modes covered by promoted test or existing SP | `python -m tools.pipeline gen_ftt_tests --hara-coverage` | `build/pipeline/ftt_coverage.json` — `stub_only_pct` = 0% for ASIL D items |
| C5.4 | Confirm `hara_coverage.json` shows `hara_he_coverage_pct` = 100% (every HARA HE traceable to SP-XX or promoted FTT test) | `python -m tools.pipeline gen_ftt_tests --hara-coverage` | `build/pipeline/hara_coverage.json` — `hara_he_coverage_pct` = 100% |
| C5.5 | Confirm gcov branch coverage ≥ 80% across BSW source (MC/DC target per ISO 26262 Part 6 §11 for ASIL D) | `make -C test/integration coverage` | `build/coverage/lcov-report/` — `branch_coverage_pct` ≥ 80% |
| C5.6 | Compile AI-Generated Test Promotion Summary table for the ITR (required by §14.6 ISO 26262 Part 6 §10 review note) | Manual — list stub IDs, source IDs, MISRA scan status, promotion date, reviewer | Table added to `docs/aspice/verification/integration-test/itr-template.md` |
| C5.7 | Complete §14.4 promotion checklist for each stub and embed in commit message body | Per-stub `chore(testgen):` commit with signed checklist | All promoted stubs committed with signed checklist; traceability chain fully linked from FMEA/SWR → stub → ASIL metric |

**Exit criterion:** All §14.3.4 coverage metrics met; MISRA scan exit code 0 on every promoted stub; `hara_he_coverage_pct` = 100%; AI-Generated Test Promotion Summary table in ITR; §14.4 checklist signed in every promotion commit.

---

### 14.8 Compliance Checking Schedule vs. Test Generation Schedule

The two daily plans interlock as follows. **As of 2026-03-27, §13.3 Days 2–5 embed inline compliance gate tasks (2.6/2.7, 3.6/3.7, 4.6/4.7, 5.7/5.8)** that run concurrently with test generation. The §14.7 standalone compliance days remain the definitive reference for command-level detail and ASPICE evidence mapping; §13.3 tasks delegate to them via cross-reference.

| §13.3 Day | Test Generation Activity | Inline Compliance Tasks | §14.7 Day | Compliance Checking Activity |
|-----------|--------------------------|------------------------|-----------|------------------------------|
| Day 1 | Baseline establishment | Compliance prerequisite: `setup-misra-rules.sh` | — | (No stubs yet; tool environment prepared) |
| Day 2 | RBT stubs generated | Tasks 2.6–2.7: MISRA scan + triage | C1 | MISRA baseline scan of Day 2 stubs |
| Day 3 | ASIL D stubs promoted | Tasks 3.6–3.7: required-rule fixes + §14.4 promotion checklist | C2 | Required-rule fixes + deviation register |
| Day 4 | FTT stubs generated | Tasks 4.6–4.7: MISRA scan + ISO 26262 Checks 1–3 | C3 | ISO 26262 traceability checks |
| Day 5 | YAML scenarios + SIL | Tasks 5.7–5.8: ISO 26262 Checks 4–6 + CI sign-off + SWE.5 evidence | C4 + C5 | CI gates implemented; ASPICE evidence packaged |

Generation and compliance checking overlap from Day 2 onward: inline tasks in §13.3 enforce MISRA and ISO 26262 gates before each stub review or promotion step. §14.7 provides the standalone reference for each compliance day; §13.3 inline tasks mirror the same operations so that a developer executing §13.3 alone does not accidentally skip the compliance gates. No stub counts toward ASIL coverage until both axes in §14.1 are satisfied.

---

*AI compliance checking section added 2026-03-27.*
*Relates to: `docs/plans/plan-misra-pipeline.md`, `docs/safety/analysis/misra-deviation-register.md`, `docs/integration_audit.md §12.6`, `docs/integration_audit.md §9.4`.*

---

### 14.9 Traceability — Daily Tasks to ASPICE Process Areas and ISO 26262 Work Products

This section maps every §14.7 daily compliance checking task to its governing ASPICE 4.0 process area, required work product, ISO 26262:2018 Part 6 clause, and resulting evidence artifact. It is the auditor's reference for demonstrating that each AI-generated test compliance activity is normatively grounded.

**Reference standards:**
- Automotive SPICE Process Reference Model 4.0 (PRM 4.0)
- ISO 26262:2018 Part 6 — Product Development at the Software Level
- ISO 26262:2018 Part 3 §7 (HARA) and Part 8 §11 (Tool Qualification)

---

#### 14.9.1 Master Day Summary

| Day | Primary Focus | ASPICE Process Area(s) | ISO 26262 Chapter(s) |
|-----|--------------|------------------------|----------------------|
| C1 | MISRA baseline scan of generated stubs | SWE.4, SUP.1 | Part 6 §8.4.6 (coding guidelines) |
| C2 | Required-rule fix + deviation register | SWE.3, SWE.4, SUP.1 | Part 6 §8.4.6 |
| C3 | ISO 26262 traceability checks (Checks 1–6) | SWE.1, SWE.5, SUP.8 | Part 6 §7.2.6, §9.4.4, §10.4.5, §10.4.7 |
| C4 | CI compliance gate implementation | SWE.4, SWE.5, SUP.1 | Part 6 §8.4.6, §10.4.5, §10.4.7, ISO 26262-8 §11 |
| C5 | ASPICE SWE.5 evidence packaging and sign-off | SWE.5, SYS.1, SUP.1 | Part 6 §9.4.3, §9.4.4, §10, §11; Part 3 §7 |

---

#### 14.9.2 Day C1 — MISRA Baseline Scan

| Task ID | Task Summary | ASPICE Process Area | ASPICE Work Product (PRM 4.0) | ISO 26262:2018 Clause | Evidence Artifact |
|---------|-------------|--------------------|-----------------------------|----------------------|-------------------|
| C1.1 | Download MISRA rule texts; verify tool readiness | SUP.1 (Quality Assurance) | SUP.1 QA Plan — tool readiness record | Part 6 §8.4.6 (software tools); ISO 26262-8 §11 (tool qualification) | Entry in `docs/aspice/verification/tool-qualification/tool-qual-cppcheck.md`; one-time per-environment record |
| C1.2 | Run cppcheck on all `*_generated.c`; capture full output to `build/misra-baseline-generated.txt` | SWE.4 (SW Unit Verification), SUP.1 | SWE.4 — Static Verification Results (BP3) | Part 6 §8.4.6 (coding guidelines gate) | `build/misra-baseline-generated.txt` — seed artifact for SWE.5 WP04 |
| C1.3 | Count violations by rule; classify each as required vs. advisory per §14.2.2 table; commit triage table | SWE.4, SUP.1 | SWE.4 — Static Verification Results; SUP.1 — Non-conformance Records | Part 6 §8.4.6 Table 11 (required / advisory distinction) | Triage table appended to `docs/plans/rbt_gap_list.md`; every violation row annotated with Rule ID, category, count |
| C1.4 | Identify patterns matching existing DEV-001/DEV-002 in deviation register | SUP.1, SWE.3 | SUP.1 — Non-conformance Records; SWE.3 — SW Unit (source deviation record) | Part 6 §8.4.6 (formal deviation process) | Triage table annotated: `DEV match` or `new deviation needed` per row |
| C1.5 | Add file-scoped suppressions for confirmed advisory rules only; no global suppressions | SWE.3 (SW Unit Construction), SUP.1, SUP.8 | SWE.3 — SW Unit; SUP.8 — Configuration Item (`suppressions.txt` baseline) | Part 6 §8.4.6 (advisory rule governance) | `tools/misra/suppressions.txt` — controlled CM artifact; suppression form: `misra-c2012-X.Y:*/test_*_generated.c` |

**ASPICE traceability note:** C1.2–C1.5 collectively produce the SWE.4 static verification evidence record (BP3 — Perform static verification). The baseline scan log is retained as the before-state record; the Day C2 re-scan log is the after-state. Together they satisfy the SWE.4 evidence requirement for AI-generated stubs.

---

#### 14.9.3 Day C2 — MISRA Required-Rule Fix and Deviation Register

| Task ID | Task Summary | ASPICE Process Area | ASPICE Work Product (PRM 4.0) | ISO 26262:2018 Clause | Evidence Artifact |
|---------|-------------|--------------------|-----------------------------|----------------------|-------------------|
| C2.1 | Fix Rule 17.7 — add `(void)` cast on all discarded BSW API return values | SWE.3 (SW Unit Construction) | SWE.3 — SW Unit (source code correction) | Part 6 §8.4.6 Rule 17.7 (required rule) | Modified generated stub file(s); cppcheck re-scan showing 0 Rule 17.7 violations |
| C2.2 | Fix Rule 15.7 — add final `else` to all `if-else-if` fault-path chains | SWE.3 | SWE.3 — SW Unit | Part 6 §8.4.6 Rule 15.7 (required rule); Part 6 §9.4.2 (path completeness) | Modified stubs; re-scan output |
| C2.3 | Fix Rule 20.1 — move all `#include` above type definitions | SWE.3 | SWE.3 — SW Unit | Part 6 §8.4.6 Rule 20.1 (required rule) | Modified stubs; re-scan output |
| C2.4 | Fix Rules 8.4/8.5 — add missing prototypes; remove duplicated `extern` declarations | SWE.3 | SWE.3 — SW Unit | Part 6 §8.4.6 Rules 8.4, 8.5 (required rules) | Modified stubs; re-scan output |
| C2.5 | Add new DEV-NNN entries to deviation register for any pattern not covered by DEV-001/DEV-002 | SUP.1, SWE.3 | SUP.1 — Non-conformance Records (formal deviation); SWE.5 WP05 (non-conformance evidence) | Part 6 §8.4.6 (deviation rationale and risk documentation) | `docs/safety/analysis/misra-deviation-register.md` — new DEV-NNN rows; each row: rule, location, technical justification, risk, compensating measure, reviewer, date |
| C2.6 | Re-run cppcheck on all `*_generated.c`; verify exit code 0 | SWE.4, SUP.1 | SWE.4 — Static Verification Results (BP3 complete); SWE.5 WP04 | Part 6 §8.4.6 (zero required-rule violations gate) | Re-scan log stored as SWE.5 WP04 evidence artifact; exit code 0 is the pass criterion for Day C2 |

**ASPICE traceability note:** C2.5 is the key SUP.1 deliverable — every suppression added to `suppressions.txt` without a signed DEV-NNN entry is a process non-conformance. The deviation register is an auditable CM-controlled artifact (SUP.8) and feeds SWE.5 WP05 (non-conformance records).

---

#### 14.9.4 Day C3 — ISO 26262 Traceability Verification

| Task ID | Task Summary | ASPICE Process Area | ASPICE Work Product (PRM 4.0) | ISO 26262:2018 Clause | Evidence Artifact |
|---------|-------------|--------------------|-----------------------------|----------------------|-------------------|
| C3.1 | Check 1 — verify SWR or FMEA source ID exists in source document | SWE.5 (SW Integration Testing), SWE.1 (SW Requirements) | SWE.5 — Traceability Information (BP5); SWE.1 — SW Requirements Specification | Part 6 §7.2.6 (FMEA traceability); Part 6 §10.4.5 (defined pass/fail) | Source row in `SWR-BSW.md` or `fmea.md` confirmed present; Check 1 column of §14.4 checklist ticked |
| C3.2 | Check 2 — verify `@verifies` annotation in stub matches source ID exactly | SWE.5 | SWE.5 — Traceability Information (BP5) | Part 6 §10.4.5, §10.4.7 (test-to-requirement linkage for regression validity) | `grep -rn "@verifies"` output showing exact matches; mismatched annotations corrected in stub |
| C3.3 | Check 3 — verify `@asil` tag matches ASIL column of requirement or FMEA row | SWE.5, SWE.1 | SWE.5 — Traceability Information; SWE.1 — SW Requirements Specification (ASIL column) | ISO 26262-1 §5.3 (ASIL classification integrity); Part 6 §10.4.5 | ASIL cross-reference review note in commit; any downgraded tags corrected and flagged for safety engineer review |
| C3.4 | Check 4 — verify each promoted assertion is deterministic and non-trivially always-pass | SWE.5, SUP.1 | SWE.5 — Verification Evidence (BP4); SUP.1 — QA Records | Part 6 §10.4.5 (pass/fail criteria validity) | Manual review annotation in `chore(testgen):` commit message; trivial assertions (`TEST_ASSERT_TRUE(1)`) rewritten |
| C3.5 | Check 5 — run gcov; confirm branch coverage delta ≥ 1% on module under test; remove zero-delta stubs | SWE.4, SWE.5 | SWE.4 — SW Unit Verification Evidence (coverage); SWE.5 WP03 (integration test coverage) | Part 6 §9.4.4 (statement and branch coverage); Part 6 §11 (MC/DC for ASIL D) | `build/coverage/lcov-report/` per-stub delta; stubs failing Check 5 removed and documented as duplicate coverage |
| C3.6 | Check 6 — update traceability matrix; run `make traceability`; commit updated matrix | SWE.5, SUP.8 | SWE.5 — Traceability Information (BP5); SUP.8 — Configuration Item (traceability-matrix.md baseline) | Part 6 §10.4.7 (regression completeness); Part 6 §7.2.6 (FMEA traceability chain) | `docs/aspice/traceability/traceability-matrix.md` diff committed; `codegen:traceability` CI stage green |

**ASPICE traceability note:** C3.1–C3.6 collectively satisfy SWE.5 BP5 (bidirectional traceability from test to requirement to safety analysis). All six checks must pass before any stub exits `TEST_IGNORE()` status. This is the primary ISO 26262 evidence gate for the test promotion workflow.

---

#### 14.9.5 Day C4 — CI Compliance Gate Implementation and Verification

| Task ID | Task Summary | ASPICE Process Area | ASPICE Work Product (PRM 4.0) | ISO 26262:2018 Clause | Evidence Artifact |
|---------|-------------|--------------------|-----------------------------|----------------------|-------------------|
| C4.1 | Implement `codegen:misra-check-stubs` CI job per §14.5 YAML template | SWE.4, SUP.1 | SWE.4 — Verification Procedure (automated static analysis gate); tool qualification record | Part 6 §8.4.6 (automated coding guidelines enforcement) | `.github/workflows/misra.yml` diff; CI job definition is a SWE.4 verification procedure artifact |
| C4.2 | Inject a known required-rule violation; confirm CI blocks (exit code 1) | SWE.4, SUP.1 | SWE.4 — Tool Qualification Evidence (positive test: tool correctly rejects) | Part 6 §8.4.6; ISO 26262-8 §11.4.5 (tool qualification — positive test confirms detection) | CI run log showing exit code 1 on injected violation; retained as tool qualification positive-test evidence |
| C4.3 | Revert injected violation; confirm CI returns to green (exit code 0) | SWE.4, SUP.1 | SWE.4 — Tool Qualification Evidence (negative test: tool correctly passes clean code) | Part 6 §8.4.6; ISO 26262-8 §11.4.5 (tool qualification — negative test confirms no false positive) | CI run log showing exit code 0 after revert; C4.2 + C4.3 together constitute the tool qualification test pair |
| C4.4 | Implement `codegen:traceability` CI step; trigger on push to `docs/**` or `test/**` | SWE.5, SUP.8 | SWE.5 — Traceability Information (automated gate); SUP.8 — Configuration Management process | Part 6 §10.4.7 (automated regression traceability validation) | CI job in `.github/workflows/*.yml`; `gen_traceability_matrix.py --validate` exit code |
| C4.5 | Confirm all four §14.5 CI stages pass on clean branch | SWE.5, SWE.4, SUP.1 | SWE.5 WP04 (composite verification evidence); SUP.1 — QA sign-off record | Part 6 §8.4.6, §10.4.5, §10.4.7 (complete compliance gate confirmation) | CI run summary showing all four stages green; this CI run is the Day C4 audit sign-off artifact |
| C4.6 | Append CI gate thresholds table (§14.3.3) to ITR template | SWE.5 | SWE.5 WP01 (Integration Test Plan — pass/fail criteria) | Part 6 §10.4.5 (pass/fail criteria must be documented in test plan) | `docs/aspice/verification/integration-test/itr-template.md` updated; threshold table under "Compliance Gates" heading |

**ASPICE traceability note:** C4.2–C4.3 satisfy the ISO 26262-8 §11 tool qualification requirement for cppcheck as a T2 tool (detection tool, no direct output to firmware). The positive test (C4.2) verifies that the tool detects the violation class; the negative test (C4.3) verifies that it does not raise false positives on clean code.

---

#### 14.9.6 Day C5 — ASPICE SWE.5 Evidence Packaging and Sign-Off

| Task ID | Task Summary | ASPICE Process Area | ASPICE Work Product (PRM 4.0) | ISO 26262:2018 Clause | Evidence Artifact |
|---------|-------------|--------------------|-----------------------------|----------------------|-------------------|
| C5.1 | Full MISRA scan on all promoted stubs; save as CI artifact | SWE.4, SWE.5, SUP.1 | SWE.4 — Static Verification Results (final); SWE.5 WP04 (verification evidence) | Part 6 §8.4.6 (MISRA compliance evidence gate — final state) | `build/misra-report.txt` — exit code 0 required; retained CI artifact; satisfies SWE.5 WP04 |
| C5.2 | Confirm `rbt_coverage.json` meets all §14.3.4 ASIL D thresholds | SWE.5 | SWE.5 WP03 (test coverage evidence — requirement-based) | Part 6 §9.4.4 (coverage); §10.4.5 (pass/fail criteria for coverage threshold) | `build/pipeline/rbt_coverage.json` — `requirement_coverage_pct` = 100% ASIL D; `stub_only_pct` = 0% |
| C5.3 | Confirm `ftt_coverage.json` shows all ASIL D FMEA failure modes covered by promoted test or existing SP | SWE.5, SWE.4 | SWE.5 WP03 (fault coverage evidence); SWE.4 — Fault Injection Results | Part 6 §9.4.3 (error guessing / fault injection); §7.2.6 (FMEA traceability) | `build/pipeline/ftt_coverage.json` — `fmea_coverage_pct` = 100% for ASIL D; `stub_only_pct` = 0% |
| C5.4 | Confirm `hara_coverage.json` `hara_he_coverage_pct` = 100% | SWE.5, SYS.1 | SWE.5 WP03 (safety path evidence); SYS.1 — Safety Concept (HARA traceability to tests) | ISO 26262-3 §7 (HARA); Part 6 §10.4.5 (all hazardous events traceable to at least one test) | `build/pipeline/hara_coverage.json` — `hara_he_coverage_pct` = 100%; links HARA HE-NNN → SP-XX or promoted FTT test |
| C5.5 | Confirm gcov branch coverage ≥ 80% across BSW source | SWE.5, SWE.4 | SWE.5 WP03 (structural coverage evidence); SWE.4 — Coverage Results | Part 6 §11 (structural coverage — MC/DC target for ASIL D) | `build/coverage/lcov-report/` — `branch_coverage_pct` ≥ 80%; reported metric feeds SWE.5 coverage record |
| C5.6 | Compile AI-Generated Test Promotion Summary table for the ITR | SWE.5, SUP.1 | SWE.5 WP01 (ITR template — AI disclosure section); SWE.5 WP04 (review record) | Part 6 §10 review note (AI-generated test disclosure required in integration test report) | "AI-Generated Test Promotion Summary" table added to `docs/aspice/verification/integration-test/itr-template.md`; columns: Stub ID, Source ID (SWR or FMEA), MISRA status, promotion date, reviewer |
| C5.7 | Complete §14.4 promotion checklist per stub; embed signed checklist in commit body | SWE.5, SUP.1 | SWE.5 WP04 (per-stub review record); SUP.1 — QA Records (signed checklist) | Part 6 §10.4.5, §10.4.7 (formal review and sign-off for each promoted test) | Each `chore(testgen):` commit body contains signed §14.4 checklist; retained as permanent audit trail in git log |

**ASPICE traceability note:** C5.2–C5.5 together constitute the complete SWE.5 WP03 evidence package (requirement coverage + fault coverage + HARA coverage + structural coverage). No single metric is sufficient in isolation — all four are needed to close the ASPICE SWE.5 verification record.

---

#### 14.9.7 Cross-Reference: Evidence Artifacts to ASPICE SWE.5 Work Products

The table below maps all §14.7 artifacts to the five ASPICE SWE.5 work products required for Level 2 compliance:

| SWE.5 Work Product | Description | Evidence Artifacts | Produced on Day(s) |
|--------------------|-----------  |--------------------|-------------------|
| **WP01** — Integration Test Plan | Test strategy, environment, pass/fail criteria, safety path coverage, AI compliance gates | This document (§14, §14.5 CI thresholds, §14.6, itr-template.md) | Plan-time + C4.6, C5.6 |
| **WP02** — Traceability Information | Bidirectional SWR ↔ test ↔ FMEA ↔ HARA linkage | `docs/aspice/traceability/traceability-matrix.md`, `@verifies` annotations, `@asil` tags, promotion checklist in commit body | C3.2, C3.6, C5.7 |
| **WP03** — Test Coverage Evidence | Requirement coverage %, fault coverage %, HARA coverage %, structural branch coverage % | `build/pipeline/rbt_coverage.json`, `build/pipeline/ftt_coverage.json`, `build/pipeline/hara_coverage.json`, `build/coverage/lcov-report/` | C3.5, C5.2, C5.3, C5.4, C5.5 |
| **WP04** — Verification Evidence | Test execution results, MISRA scan results, tool qualification records, per-stub review records | `build/misra-baseline-generated.txt`, `build/misra-report.txt`, CI run logs (C4.2 + C4.3 tool qualification pair), per-stub `chore(testgen):` commit bodies | C1.2, C2.6, C4.2, C4.3, C5.1, C5.7 |
| **WP05** — Non-conformance Records | MISRA deviation register, violation triage table, suppression justifications | `docs/safety/analysis/misra-deviation-register.md` (new DEV-NNN rows), triage table in `docs/plans/rbt_gap_list.md` | C1.3, C1.4, C2.5 |

---

#### 14.9.8 Cross-Reference: Daily Tasks to ISO 26262 Part 6 Sections

| ISO 26262 Reference | Title / Scope | Daily Task(s) | Audit Gate / Metric |
|---------------------|---------------|---------------|---------------------|
| Part 6 §7.2.6 | Safety analyses — FMEA traceability | C3.1, C5.3 | `@verifies FMEA-ID` on every FTT stub; `ftt_coverage.json` `fmea_coverage_pct` = 100% ASIL D |
| Part 6 §8.4.6 | Use of software tools — MISRA C:2012 coding guidelines | C1.1–C1.5, C2.1–C2.6, C4.1–C4.3, C5.1 | `cppcheck --error-exitcode=1`; exit code 0 on `*_generated.c`; `misra-report.txt` retained |
| Part 6 §9.4.2 | Equivalence classes and boundary values | C2.2 (Rule 15.7 path completeness forces explicit else-branches) | Required-rule clean; fault-path assertions cover boundary behaviour |
| Part 6 §9.4.3 | Error guessing / fault injection | C3.1 (FTT stubs require FMEA source ID), C5.3 | `build/pipeline/ftt_coverage.json` `fmea_coverage_pct` = 100% ASIL D |
| Part 6 §9.4.4 | Statement and branch coverage | C3.5, C5.5 | `gcov` branch delta ≥ 1% per promoted stub; overall `branch_coverage_pct` ≥ 80% BSW |
| Part 6 §10.4.5 | Pass/fail criteria | C3.4, C4.6, C5.2, C5.7 | Deterministic assertions only; ITR template thresholds documented; `rbt_coverage.json` thresholds met |
| Part 6 §10.4.7 | Regression testing | C3.2, C3.6, C4.4 | `codegen:traceability` CI green; traceability-matrix committed; all 60 integration tests PASS |
| Part 6 §11 | Structural coverage — MC/DC for ASIL D | C5.5 | `build/coverage/lcov-report/` `branch_coverage_pct` ≥ 80%; MC/DC argument documented in SWE.5 WP03 |
| ISO 26262-3 §7 | Hazard analysis and risk assessment (HARA) | C5.4 | `build/pipeline/hara_coverage.json` `hara_he_coverage_pct` = 100%; every HARA HE linked to SP-XX or promoted FTT test |
| ISO 26262-8 §11 | Tool qualification — T2 tool (cppcheck) | C1.1, C4.1, C4.2, C4.3 | Tool qualification record in `tool-qual-cppcheck.md`; positive test (C4.2) + negative test (C4.3) CI run logs |

---

#### 14.9.9 Audit Completeness Checklist

Before closing the ASPICE SWE.5 compliance record for AI-generated test stubs, an auditor must verify that every item in the following list is satisfied:

```
ASPICE SWE.5 AI COMPLIANCE AUDIT CHECKLIST
===========================================
Evidence Artifact            Location                                  Day   WP    Status
─────────────────────────────────────────────────────────────────────────────────────────
MISRA baseline scan log      build/misra-baseline-generated.txt       C1    WP04  [ ]
MISRA violation triage       docs/plans/rbt_gap_list.md (appended)    C1    WP05  [ ]
Deviation register entries   docs/safety/analysis/misra-deviation-    C2    WP05  [ ]
                             register.md (DEV-NNN rows)
MISRA final scan (exit 0)    build/misra-report.txt                   C5    WP04  [ ]
Traceability matrix updated  docs/aspice/traceability/                C3    WP02  [ ]
                             traceability-matrix.md
Branch coverage delta ≥ 1%  build/coverage/lcov-report/ (per stub)   C3    WP03  [ ]
Branch coverage ≥ 80% BSW   build/coverage/lcov-report/ (overall)    C5    WP03  [ ]
RBT coverage JSON            build/pipeline/rbt_coverage.json         C5    WP03  [ ]
FTT coverage JSON            build/pipeline/ftt_coverage.json         C5    WP03  [ ]
HARA coverage JSON           build/pipeline/hara_coverage.json        C5    WP03  [ ]
CI: misra-check-stubs green  .github/workflows/misra.yml run          C4    WP04  [ ]
CI: rbt-coverage green       CI run summary                           C4    WP03  [ ]
CI: ftt-coverage green       CI run summary                           C4    WP03  [ ]
CI: traceability green       CI run summary                           C4    WP02  [ ]
Tool qual: positive test     CI run log (C4.2 — exit code 1)          C4    WP04  [ ]
Tool qual: negative test     CI run log (C4.3 — exit code 0)          C4    WP04  [ ]
AI Promotion Summary table   docs/aspice/verification/                C5    WP01  [ ]
                             integration-test/itr-template.md
Per-stub promotion checklist Commit body of each chore(testgen):      C5    WP04  [ ]
                             commit (signed §14.4 checklist)
```

All items marked `[ ]` must be ticked `[x]` before the SWE.5 integration test report (ITR) is promoted to `status: approved`.

---

*Traceability mapping section added 2026-03-27.*
*Relates to: §14.7 (daily plan), §14.6 (evidence for SWE.5), §9 (safety paths SP-01…SP-15), `docs/aspice/plans/aspice-plans-overview.md` (process coverage), `docs/integration_audit.md §9.5` (ASPICE process status table).*
