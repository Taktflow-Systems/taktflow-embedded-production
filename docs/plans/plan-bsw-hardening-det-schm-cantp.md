# Plan: AUTOSAR BSW Hardening — DET, SchM, CanTp (ISO-TP)

**Status:** IN PROGRESS
**Date:** 2026-03-03

## Context

Research of our 18-module AUTOSAR-like BSW architecture revealed 3 gaps compared to professional AUTOSAR that have the highest ROI for hardening:

1. **DET (Default Error Tracer)** — All modules silently `return E_NOT_OK` on parameter errors (NULL pointer, uninitialized module, out-of-range IDs). No error tracing, no logging, no hook for diagnostics. In professional AUTOSAR, DET catches these at development time and logs them.

2. **SchM (Schedule Manager)** — Our cooperative single-threaded model is safe for SIL (Docker/POSIX), but on real STM32 hardware, ISRs can preempt the main loop and corrupt shared state in Com shadow buffers, RTE signal buffer, Dem event arrays, WdgM counters, and Can driver state. Professional AUTOSAR has SchM critical sections.

3. **CanTp (ISO-TP / ISO 15765-2)** — Dcm is limited to 8-byte single-frame CAN 2.0B responses. No multi-frame transport. UDS DIDs are capped at 5 bytes of data. Can't support SecurityAccess (0x27), ECUReset (0x11), ReadMemoryByAddress, or any response >8 bytes. Professional AUTOSAR has CanTp for segmented transfer.

## Phase Overview

| Phase | Name | Scope | Status |
|-------|------|-------|--------|
| 1 | DET module + integration | Det.h/Det.c + add `Det_ReportError()` calls to all 18 modules | DONE |
| 2 | SchM critical sections | SchM.h/SchM.c + protect 5 modules with shared state | DONE |
| 3 | CanTp standalone | CanTp.h/CanTp.c implementing ISO 15765-2 SF/FF/CF/FC | DONE |
| 4 | CanTp + Dcm integration | PduR routing, Dcm buffer expansion 8→128, new UDS services | DONE |
| 5 | SIL integration test | Full Docker test on Netcup | IN PROGRESS |

## Phase 1: DET (Default Error Tracer)

### New Files

| File | Description |
|------|-------------|
| `firmware/shared/bsw/services/Det.h` | API: `Det_Init()`, `Det_ReportError()`, `Det_ReportRuntimeError()` |
| `firmware/shared/bsw/services/Det.c` | Implementation: error log ring buffer, SIL_DIAG output, optional callback hook |
| `firmware/shared/bsw/test/test_Det_asild.c` | Unit test |

### Det API Design

```c
/* Error IDs per AUTOSAR convention */
#define DET_E_PARAM_POINTER     0x01u  /* NULL pointer passed */
#define DET_E_UNINIT            0x02u  /* Module not initialized */
#define DET_E_PARAM_VALUE       0x03u  /* Parameter out of range */

/* Module IDs (unique per BSW module) */
#define DET_MODULE_CAN      0x01u
#define DET_MODULE_CANIF     0x02u
#define DET_MODULE_PDUR      0x03u
#define DET_MODULE_COM       0x04u
#define DET_MODULE_DCM       0x05u
#define DET_MODULE_DEM       0x06u
#define DET_MODULE_WDGM      0x07u
#define DET_MODULE_BSWM      0x08u
#define DET_MODULE_E2E       0x09u
#define DET_MODULE_RTE       0x0Au
#define DET_MODULE_SPI       0x0Bu
#define DET_MODULE_ADC       0x0Cu
#define DET_MODULE_DIO       0x0Du
#define DET_MODULE_GPT       0x0Eu
#define DET_MODULE_PWM       0x0Fu
#define DET_MODULE_IOHWAB    0x10u
#define DET_MODULE_UART      0x11u
#define DET_MODULE_NVM       0x12u

/* API */
void Det_Init(void);
void Det_ReportError(uint16 ModuleId, uint8 InstanceId,
                     uint8 ApiId, uint8 ErrorId);
```

### Integration Pattern

Transform every silent `return` into a `Det_ReportError()` call. Example for PduR:

```c
/* BEFORE (current) */
void PduR_Init(const PduR_ConfigType* ConfigPtr) {
    if (ConfigPtr == NULL_PTR) {
        pdur_initialized = FALSE;
        pdur_config = NULL_PTR;
        return;                    /* <-- silent failure */
    }
    ...
}

/* AFTER */
void PduR_Init(const PduR_ConfigType* ConfigPtr) {
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_PDUR, 0u,
                        PDUR_API_INIT, DET_E_PARAM_POINTER);
        pdur_initialized = FALSE;
        pdur_config = NULL_PTR;
        return;
    }
    ...
}
```

### Modules to Instrument (all 18)

| Layer | Module | File | Error points |
|-------|--------|------|-------------|
| MCAL | Can | `mcal/Can.c` | Init NULL, uninit, invalid HW obj |
| MCAL | Spi | `mcal/Spi.c` | Init NULL, uninit, invalid channel |
| MCAL | Adc | `mcal/Adc.c` | Init NULL, uninit, invalid group |
| MCAL | Pwm | `mcal/Pwm.c` | Init NULL, uninit, invalid channel |
| MCAL | Dio | `mcal/Dio.c` | Init NULL, uninit, invalid channel |
| MCAL | Gpt | `mcal/Gpt.c` | Init NULL, uninit, invalid channel |
| MCAL | Uart | `mcal/Uart.c` | Init NULL, uninit |
| EcuAl | CanIf | `ecual/CanIf.c` | Init NULL, uninit, invalid PDU |
| EcuAl | PduR | `ecual/PduR.c` | Init NULL, uninit, NULL PduInfo |
| EcuAl | IoHwAb | `ecual/IoHwAb.c` | Init NULL, uninit |
| Services | Com | `services/Com.c` | Init NULL, uninit, invalid signal/PDU |
| Services | Dcm | `services/Dcm.c` | Init NULL, uninit, NULL PduInfo |
| Services | Dem | `services/Dem.c` | Init NULL, uninit, invalid event |
| Services | WdgM | `services/WdgM.c` | Init NULL, uninit, invalid SE |
| Services | BswM | `services/BswM.c` | Init NULL, uninit |
| Services | E2E | `services/E2E.c` | NULL config/state/data |
| Services | NvM | `services/NvM.c` | Init NULL, uninit, invalid block |
| RTE | Rte | `rte/Rte.c` | Init NULL, uninit, invalid signal |

### DONE Criteria
- [ ] Det.h/Det.c compiles and passes unit tests
- [ ] All 18 modules call `Det_ReportError()` on every parameter/init check
- [ ] `make test` passes (all existing tests still green)
- [ ] SIL_DIAG output shows DET errors when triggered

---

## Phase 2: SchM (Schedule Manager — Critical Sections)

### New Files

| File | Description |
|------|-------------|
| `firmware/shared/bsw/services/SchM.h` | API: `SchM_Enter_<Module>()`, `SchM_Exit_<Module>()` |
| `firmware/shared/bsw/services/SchM.c` | Implementation: POSIX = no-op, STM32 = `__disable_irq()`/`__enable_irq()` with nesting |
| `firmware/shared/bsw/test/test_SchM_asild.c` | Unit test |

### SchM API Design

```c
/* Per-module critical section enter/exit (AUTOSAR pattern) */
void SchM_Enter_Com_COM_EXCLUSIVE_AREA_0(void);
void SchM_Exit_Com_COM_EXCLUSIVE_AREA_0(void);
void SchM_Enter_Rte_RTE_EXCLUSIVE_AREA_0(void);
void SchM_Exit_Rte_RTE_EXCLUSIVE_AREA_0(void);
/* ... etc for each protected module */

/* Implementation strategy:
 * - POSIX/SIL: empty macros (cooperative = no preemption risk)
 * - STM32:     __disable_irq() / __enable_irq() with nesting counter
 */
#if defined(PLATFORM_POSIX)
  #define SchM_Enter_Com_COM_EXCLUSIVE_AREA_0()  ((void)0)
  #define SchM_Exit_Com_COM_EXCLUSIVE_AREA_0()   ((void)0)
#else
  void SchM_Enter_Com_COM_EXCLUSIVE_AREA_0(void);
  void SchM_Exit_Com_COM_EXCLUSIVE_AREA_0(void);
#endif
```

### Modules Needing Critical Sections (5)

| Module | Shared State | Risk on STM32 |
|--------|-------------|---------------|
| Com | `com_tx_pdu_buf[]`, `com_rx_pdu_buf[]`, `com_tx_pending[]` | ISR writes RX buffer while main loop reads |
| Rte | `rte_signal_buffer[48]` | ISR-triggered Com callback vs main loop SWC read |
| Dem | `dem_events[]`, `dem_debounce_cnt[]` | Multiple SWCs report events concurrently |
| WdgM | `wdgm_alive_cnt[]`, `wdgm_se_status[]` | Alive counter updated from different task priorities |
| Can | `can_state`, mailbox buffers | CAN RX ISR vs main loop TX |

### Integration Pattern

```c
/* In Com.c — wrap shadow buffer access */
void Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr) {
    ...
    SchM_Enter_Com_COM_EXCLUSIVE_AREA_0();
    /* pack signal into TX PDU buffer */
    com_tx_pdu_buf[pdu_idx][byte_offset] = ...;
    com_tx_pending[pdu_idx] = TRUE;
    SchM_Exit_Com_COM_EXCLUSIVE_AREA_0();
    ...
}
```

### DONE Criteria
- [x] SchM.h/SchM.c compiles (POSIX = no-op, STM32 = interrupt disable)
- [x] Unit test verifies nesting counter behavior (11 tests pass)
- [x] 5 modules wrapped with SchM_Enter/Exit at shared-state access points (Com, Rte, Dem, WdgM, Can)
- [x] `make test` passes (21/21 suites, 518 tests, 0 failures)
- [x] No performance regression in SIL (no-op macros = zero overhead)

---

## Phase 3: CanTp Standalone (ISO 15765-2 Transport Protocol)

### New Files

| File | Description |
|------|-------------|
| `firmware/shared/bsw/services/CanTp.h` | API + types + constants |
| `firmware/shared/bsw/services/CanTp.c` | ISO 15765-2 state machine |
| `firmware/shared/bsw/test/test_CanTp_asild.c` | Unit test (SF, FF/CF, FC) |

### CanTp Design

**Frame types** (ISO 15765-2):

| Frame | PCI byte | Purpose |
|-------|----------|---------|
| SF (Single Frame) | `0x0N` (N=len) | Messages ≤ 7 bytes |
| FF (First Frame) | `0x1N NN` | First segment of multi-frame (declares total length) |
| CF (Consecutive Frame) | `0x2N` (N=sequence) | Subsequent segments, SN wraps 0-F |
| FC (Flow Control) | `0x30 BS STmin` | Receiver controls sender pace |

**State machine**:

```
IDLE ──SF_RX──► INDICATE_UPPER
  │
  ├──FF_RX──► WAIT_CF (start timer, send FC)
  │              │
  │              ├──CF_RX──► (accumulate, check SN)
  │              │             ├──complete──► INDICATE_UPPER
  │              │             └──error──► IDLE
  │              └──timeout──► IDLE (abort)
  │
  └──TX_REQUEST──► TX_SF (if ≤7) or TX_FF (if >7)
                     │
                     TX_FF──► WAIT_FC_RX
                                ├──FC_RX──► TX_CF (send segments)
                                └──timeout──► IDLE (abort)
```

**Key parameters**:
- `CANTP_MAX_PAYLOAD` = 128 bytes (covers all needed UDS responses)
- `CANTP_N_CR_TIMEOUT_MS` = 1000 (ISO 15765-2 N_Cr timeout)
- `CANTP_N_BS_TIMEOUT_MS` = 1000 (N_Bs timeout for FC)
- `CANTP_STMIN_MS` = 10 (minimum separation time)
- Block Size (BS) = 0 (no flow control pause — continuous)

**PduLengthType widening**: `ComStack_Types.h` must change `PduLengthType` from `uint8` to `uint16` to support payloads >255 bytes. Impact: all modules that use `PduInfoType.SduLength` — verify no truncation.

### API

```c
void CanTp_Init(const CanTp_ConfigType* ConfigPtr);
void CanTp_MainFunction(void);   /* 10ms cyclic — manages timers/state */

/* Called by PduR on CAN RX for TP PDU IDs */
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

/* Called by upper layer (Dcm) to transmit segmented response */
Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/* Upper-layer callbacks (provided by Dcm) */
extern void Dcm_TpRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr,
                                NotifResultType Result);
extern Std_ReturnType PduR_CanTpTransmit(PduIdType TxPduId,
                                          const PduInfoType* PduInfoPtr);
```

### DONE Criteria
- [x] CanTp.h/CanTp.c compiles standalone
- [x] Unit test covers: SF RX/TX, FF+CF RX (multi-frame reassembly), FF+CF TX (segmentation), FC handling, timeout abort, sequence number wrap (25 tests pass)
- [x] `PduLengthType` widened to `uint16` in `ComStack_Types.h`
- [x] Existing module tests still pass with widened type
- [x] `make test` all green (22/22 suites, 543 tests, 0 failures)

---

## Phase 4: CanTp + Dcm Integration

### Modified Files

| File | Change |
|------|--------|
| `firmware/shared/bsw/include/ComStack_Types.h` | `PduLengthType` → `uint16` |
| `firmware/shared/bsw/ecual/PduR.c` + `PduR.h` | Add `PDUR_DEST_CANTP` route, `PduR_CanTpTransmit()` |
| `firmware/shared/bsw/services/Dcm.h` | `DCM_TX_BUF_SIZE` 8→128, add SID 0x27/0x11 constants |
| `firmware/shared/bsw/services/Dcm.c` | Expand TX buffer, add SecurityAccess (0x27) and ECUReset (0x11), use CanTp_Transmit for responses >7 bytes |
| `firmware/cvc/cfg/Dcm_Cfg_Cvc.c` | Add new DIDs that return >7 bytes of data |
| `firmware/cvc/src/main.c` | Wire `CanTp_Init()` + `CanTp_MainFunction()` into init/scheduler |
| Per-ECU `PduR_Cfg_*.c` | Add CanTp routing entries for diag CAN IDs |

### PduR Routing Change

```c
/* Add new destination */
typedef enum {
    PDUR_DEST_COM  = 0u,
    PDUR_DEST_DCM  = 1u,
    PDUR_DEST_CANTP = 2u   /* NEW: route diag PDUs through CanTp */
} PduR_DestType;

/* Routing: diagnostic CAN IDs (0x7DF, 0x7E0) → CanTp instead of direct Dcm */
/* CanTp reassembles, then calls Dcm_TpRxIndication() */
```

### Dcm Expansion

```c
/* Increased buffer for multi-frame responses */
#define DCM_TX_BUF_SIZE  128u   /* was 8u */

/* New UDS services */
#define DCM_SID_ECU_RESET       0x11u
#define DCM_SID_SECURITY_ACCESS 0x27u
```

**SecurityAccess (0x27)**: Seed-key challenge for extended session. Seed = random 4 bytes, key = XOR with secret (placeholder — real crypto for STM32 later).

**ECUReset (0x11)**: Soft reset sub-function (0x01). Sends positive response, then triggers BswM state transition.

### DONE Criteria
- [ ] PduR routes diag PDUs through CanTp
- [ ] Dcm handles multi-frame responses via CanTp_Transmit
- [ ] SecurityAccess 0x27 works in SIL (seed → key → unlock extended session)
- [ ] ECUReset 0x11 triggers BswM reset
- [ ] Existing UDS services (0x10, 0x22, 0x3E) still work through CanTp
- [ ] `make test` all green

---

## Phase 5: SIL Integration Test

### ECU Wiring (pre-deployment)

**CVC main.c** — DONE (2026-03-03):
- [x] Added `#include "CanTp.h"`
- [x] Added `CanTp_ConfigType cantp_config` (rxPduId=0, txPduId=CVC_COM_TX_UDS_RSP, fcTxPduId=CVC_COM_TX_UDS_RSP, upperRxPduId=0)
- [x] Changed PduR diag routing: `PDUR_DEST_DCM` → `PDUR_DEST_CANTP` for UDS CAN IDs (0x7DF, 0x7E0)
- [x] Added `CanTp_Init(&cantp_config)` after PduR_Init
- [x] Added `CanTp_MainFunction()` in 10ms task (before Dcm_MainFunction)

**FZC / RZC** — No changes needed (no UDS CAN routing configured)

**make test** — 22/22 suites, 554 tests, 0 failures

### Test on Netcup Docker

1. Deploy updated firmware (CVC, FZC, RZC containers)
2. Verify CanTp multi-frame: send UDS ReadDID for a >7-byte DID, verify complete response
3. Verify SecurityAccess: send 0x27 seed request, compute key, send key, verify session unlock
4. Verify ECUReset: send 0x11 01, verify positive response + container restarts
5. Verify DET: trigger a known DET error (e.g., call API before init), verify SIL_DIAG log output
6. Verify existing E2E tests still pass (no regression from SchM no-ops)
7. Run full 8-scenario E2E test suite — all tests reach verdict

### Test Runner Fix (2026-03-03)

E2E tests initially failed with "CVC did not reach RUN within 60s" for all scenarios. Root cause: **3 bugs in test runner MQTT monitoring**.

1. **paho-mqtt v2.1 callback signature** — `message_callback_add` in v2.x passes 4 args `(client, userdata, msg, properties)`, but our `_on_message` only accepted 3. Silent TypeError swallowed by paho. Fix: added `properties=None` default parameter.

2. **Missing `on_connect` re-subscription** — After MQTT reconnect, subscriptions were lost. Fix: added `_subscribe()` method + `on_mqtt_connect()` callback wired through `app.py`.

3. **Monitor reset order** — `_monitor.reset()` was called BEFORE `_reset()` (container restart). During restart, CVC continues running for ~20s sending VehicleState=1, polluting the monitor with stale state. `_wait_for_run()` returned immediately on stale state, then CVC actually restarted into INIT. Fix: reset monitor AFTER `_reset()` returns.

After fix: CVC correctly reaches RUN, DTCs are received, state transitions tracked. Pinned `paho-mqtt==2.1.0` in requirements.txt.

Lessons learned: `docs/reference/lessons-learned/infrastructure/PROCESS-paho-mqtt-v2-callback-compat.md`

### E2E Test Results (post-fix)

CVC reaches RUN for all scenarios. DTCs are broadcast and received by test runner. However, **all 8 scenarios still FAIL** because CVC's Vehicle State Manager does not transition RUN→SAFE_STOP when fault DTCs are received. This is a firmware behavior gap, not a test runner issue. CVC VSM fault reaction logic is needed as a separate task.

### DONE Criteria
- [x] ECU wiring complete (CVC CanTp init + routing)
- [ ] Multi-frame UDS response works end-to-end
- [ ] SecurityAccess handshake succeeds
- [ ] DET errors visible in logs
- [x] All 8 E2E tests reach verdict — CVC reaches RUN, DTCs received (verdicts FAIL due to missing VSM fault reaction)
- [x] CI green (all 3 workflows passing)
- [ ] CVC VSM fault→SAFE_STOP transition (separate task — firmware gap)

---

## Files Summary

### New Files (6)

| File | ASIL |
|------|------|
| `firmware/shared/bsw/services/Det.h` | D |
| `firmware/shared/bsw/services/Det.c` | D |
| `firmware/shared/bsw/services/SchM.h` | D |
| `firmware/shared/bsw/services/SchM.c` | D |
| `firmware/shared/bsw/services/CanTp.h` | D |
| `firmware/shared/bsw/services/CanTp.c` | D |

### New Test Files (3)

| File |
|------|
| `firmware/shared/bsw/test/test_Det_asild.c` |
| `firmware/shared/bsw/test/test_SchM_asild.c` |
| `firmware/shared/bsw/test/test_CanTp_asild.c` |

### Modified Files (22+)

| File | Change |
|------|--------|
| `firmware/shared/bsw/include/ComStack_Types.h` | `PduLengthType` uint8→uint16 |
| `firmware/shared/bsw/ecual/PduR.h` | Add `PDUR_DEST_CANTP` |
| `firmware/shared/bsw/ecual/PduR.c` | Route diag PDUs to CanTp |
| `firmware/shared/bsw/services/Dcm.h` | Buffer 8→128, new SID constants |
| `firmware/shared/bsw/services/Dcm.c` | Multi-frame TX, SecurityAccess, ECUReset |
| `firmware/shared/bsw/services/Com.c` | Add SchM critical sections + Det calls |
| `firmware/shared/bsw/rte/Rte.c` | Add SchM critical sections + Det calls |
| `firmware/shared/bsw/services/Dem.c` | Add SchM critical sections + Det calls |
| `firmware/shared/bsw/services/WdgM.c` | Add SchM critical sections + Det calls |
| `firmware/shared/bsw/mcal/Can.c` | Add SchM critical sections + Det calls |
| All other 13 BSW .c files | Add `Det_ReportError()` calls |
| `firmware/cvc/src/main.c` | Wire CanTp_Init + CanTp_MainFunction, PduR diag→CanTp routing |

## Verification

1. `make test` after each phase — all green before moving to next
2. Deploy to Netcup after Phase 5
3. Run E2E test suite from dashboard — no regression
4. Check SIL_DIAG logs for DET error output on triggered faults
5. Send multi-frame UDS request from dashboard — verify complete response
