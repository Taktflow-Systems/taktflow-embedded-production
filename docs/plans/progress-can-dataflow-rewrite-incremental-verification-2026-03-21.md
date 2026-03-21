# Session Progress — 2026-03-21

## What Was Built

### New BSW Modules (3)
- [CanSM](firmware/bsw/services/CanSM/) — L1/L2 bus-off recovery state machine
- [FiM](firmware/bsw/services/FiM/) — Function Inhibition Manager (Dem event → SWC inhibition)
- [Xcp](firmware/bsw/services/Xcp/) — XCP slave (CONNECT, SHORT_UPLOAD, SHORT_DOWNLOAD, SET_MTA, UPLOAD)
- [SchM_Timing](firmware/bsw/services/SchM/src/SchM_Timing.c) — WCET measurement (DWT/RTI/clock_gettime)

### Modified BSW Modules
- [Com.h](firmware/bsw/services/Com/include/Com.h) + [Com.c](firmware/bsw/services/Com/src/Com.c)
  - E2E supervision state machine (windowed: VALID/NODATA/INIT/INVALID)
  - DIRECT TX mode (`Com_TriggerIPDUSend`)
  - TRIGGERED_ON_CHANGE (skip TX for unchanged event payloads)
  - Signal quality API (`Com_GetRxPduQuality`: FRESH/E2E_FAIL/TIMED_OUT)
  - DTC on E2E failure (`Dem_ReportErrorStatus` per-PDU)
  - TX confirmation monitoring (`g_dbg_com_tx_stuck`)
  - Startup delay (`COM_STARTUP_DELAY_MS`)
  - Signal groups (`Com_FlushTxPdu`)
  - Update bits (`UpdateBitPos` per signal)
  - Type-safe macros (`Com_SendSignal_u8/u16/s16`)
  - O(1) TX PDU lookup (`com_tx_pdu_index[]`)
  - `Com_TxModeType` enum (PERIODIC/DIRECT/MIXED/NONE)
- [E2E.h](firmware/bsw/services/E2E/include/E2E.h) + [E2E.c](firmware/bsw/services/E2E/src/E2E.c)
  - `E2E_SMCheck()` with configurable window sizes
  - REPEATED ≠ OK (stuck sender detection)
- [PduR.h](firmware/bsw/ecual/PduR/include/PduR.h) + [PduR.c](firmware/bsw/ecual/PduR/src/PduR.c)
  - `PDUR_DEST_XCP` routing destination
- [CanIf.c](firmware/bsw/ecual/CanIf/src/CanIf.c)
  - `CanSM_ControllerBusOff()` called from bus-off handler
- [Can_Posix.c](firmware/platform/posix/src/Can_Posix.c)
  - Software TX ID loopback filter for vcan
- [Can.h](firmware/bsw/mcal/Can/include/Can.h)
  - `CAN_MAX_RX_PER_CALL` = 64

### DBC Changes
- [taktflow_vehicle.dbc](gateway/taktflow_vehicle.dbc) — 8 new XCP CAN messages (0x550-0x557)
- Total: 45 messages (was 37)

### Codegen Changes
- [model.py](tools/arxmlgen/model.py) — `satisfies`, `asil`, `e2e_max_delta` fields on Pdu
- [reader.py](tools/arxmlgen/reader.py) — extracts Satisfies/ASIL/MaxDeltaCounter from DBC, `_apply_dbc_traceability()`
- [Com_Cfg.c.j2](tools/arxmlgen/templates/com/Com_Cfg.c.j2) — TxMode, E2E SM windows, DEM event, update bits, `@satisfies` comments
- [PduR_Cfg.c.j2](tools/arxmlgen/templates/pdur/PduR_Cfg.c.j2) — `PDUR_DEST_XCP` routing for `XCP_Req_*`
- [Makefile.posix](firmware/platform/posix/Makefile.posix) — added CanSM/FiM/Xcp/SchM_Timing to build

### SWC Changes
- [Swc_CvcCom.c](firmware/ecu/cvc/src/Swc_CvcCom.c) — heartbeat/estop/torque bridge via Com_SendSignal (removed PduR_Transmit)
- [Swc_FzcCom.c](firmware/ecu/fzc/src/Swc_FzcCom.c) — same pattern, removed E2E_Protect + PduR_Transmit
- [Swc_RzcCom.c](firmware/ecu/rzc/src/Swc_RzcCom.c) — same pattern
- [Swc_Heartbeat.c](firmware/ecu/cvc/src/Swc_Heartbeat.c) (CVC/FZC/RZC) — removed dead E2E configs
- [Swc_EStop.c](firmware/ecu/cvc/src/Swc_EStop.c) — uses Rte_Write (removed Com_SendSignal)
- [Swc_Pedal.c](firmware/ecu/cvc/src/Swc_Pedal.c) — uses Rte_Write (removed Com_SendSignal)
- [Swc_Brake.c](firmware/ecu/fzc/src/Swc_Brake.c) — uses Rte_Write (removed Com_SendSignal)
- [Swc_FzcSafety.c](firmware/ecu/fzc/src/Swc_FzcSafety.c) — uses Rte_Write + `Com_GetRxPduQuality` for steer/brake staleness
- [Swc_VehicleState.c](firmware/ecu/cvc/src/Swc_VehicleState.c) — SC_KILL→SHUTDOWN, MOTOR_CUTOFF→DEGRADED

### ECU main.c Changes
- [CVC main.c](firmware/ecu/cvc/src/main.c)
  - CanSM/FiM/XCP/SchM_Timing init + scheduler
  - CanIf + PduR configs: replaced hand-written with `extern` to generated
- [FZC main.c](firmware/ecu/fzc/src/main.c) — CanSM/XCP/Timing init + CanSM_MainFunction
- [RZC main.c](firmware/ecu/rzc/src/main.c) — CanSM/XCP/Timing init + CanSM_MainFunction

### Tools
- [gen_a2l.py](tools/xcp/gen_a2l.py) — ELF → A2L for XCP measurement
- [xcp_test.py](tools/xcp/xcp_test.py) — pyXCP verification (PCAN-USB or python-can)
- [xcp_config.json](tools/xcp/xcp_config.json) — pyXCP config for CVC on PCAN
- [gen_traceability_matrix.py](tools/trace/gen_traceability_matrix.py) — auto scans DBC+source+tests
- [add_verifies_tags.py](tools/trace/add_verifies_tags.py) — batch adds @verifies to test files
- [Makefile.coverage](test/Makefile.coverage) — gcov + lcov for branch coverage

### CI Workflows
- [ci.yml](.github/workflows/ci.yml) — 7-step pipeline + unit tests + syntax check + traceability
- [test.yml](.github/workflows/test.yml) — gcov/lcov coverage collection
- [traceability.yml](.github/workflows/traceability.yml) — auto-generate traceability matrix

### Test Files

**Unit tests (52 tests):**
- [test_Com_asild.c](test/unit/bsw/test_Com_asild.c) — 30 tests (signal, E2E, quality, timing)
- [test_CanSM_asild.c](test/unit/bsw/test_CanSM_asild.c) — 10 tests (bus-off recovery)
- [test_E2E_SM_asild.c](test/unit/bsw/test_E2E_Sm_asild.c) — 12 tests (windowed supervision)
- [stubs_com_test.c](test/unit/bsw/stubs_com_test.c) — Det/SchM/Rte/Dem stubs

**Integration tests:**
- [test_bsw_dataflow.c](test/integration/bsw/test_bsw_dataflow.c) — Layer 3: real Com+PduR+CanIf+E2E (6 tests)
- [test_can_socket_rx.py](test/integration/bsw/test_can_socket_rx.py) — Layer 3: socket receive (3 tests)
- [test_cvc_single_ecu.py](test/integration/layer4/test_cvc_single_ecu.py) — Layer 4: basic CVC (6 tests)
- [test_cvc_full.py](test/integration/layer4/test_cvc_full.py) — Layer 4: full CVC suite (30 tests)
- [test_cvc_fzc_dual.py](test/integration/layer5/test_cvc_fzc_dual.py) — Layer 5: basic CVC+FZC (6 tests)
- [test_cvc_fzc_full.py](test/integration/layer5/test_cvc_fzc_full.py) — Layer 5: full dual ECU (34 tests)

### Documentation

**Safety audits:**
- [can-dataflow-gap-analysis.md](docs/safety/audit/can-dataflow-gap-analysis.md) — 12 perspectives, 35 gaps
- [can-dataflow-vs-production.md](docs/safety/audit/can-dataflow-vs-production.md) — 10 iterations vs AUTOSAR
- [10-identity-sanity-check.md](docs/safety/audit/10-identity-sanity-check.md) — 11 findings
- [topdown-traceability-audit.md](docs/safety/audit/topdown-traceability-audit.md) — 13-layer chain
- [vsm-hara-alignment-audit.md](docs/safety/audit/vsm-hara-alignment-audit.md) — VSM vs HARA

**Design specs:**
- [vsm-state-machine.md](docs/safety/design/vsm-state-machine.md) — 6 states, 17 events, HARA mapping
- [can-bus-topology.md](docs/aspice/system/can-bus-topology.md) — physical layer
- [can-bus-load-budget.md](docs/aspice/system/can-bus-load-budget.md) — 14.2% nominal
- [hmi-fault-notification.md](docs/aspice/system/hmi-fault-notification.md) — dashboard spec

**Plans:**
- [plan-incremental-firmware-verification.md](docs/plans/plan-incremental-firmware-verification.md) — 6-layer verification
- [plan-vsm-realignment.md](docs/plans/plan-vsm-realignment.md) — SC_KILL + MOTOR_CUTOFF fix

**Verification:**
- [traceability-matrix.md](docs/safety/verification/traceability-matrix.md) — 306/353 traced (86%)

**Lessons learned:**
- [phase2-can-dataflow-2026-03-21.md](docs/lessons-learned/embedded/phase2-can-dataflow-2026-03-21.md)
- [layer4-runtime-bugs-2026-03-21.md](docs/lessons-learned/embedded/layer4-runtime-bugs-2026-03-21.md)

---

## Verification Status

```
Layer 1: Com unit              30/30  ✅
Layer 2: E2E SM + CanSM        22/22  ✅
Layer 3: BSW integration        6/6   ✅
Layer 3: Socket RX              3/3   ✅
Layer 4: CVC full              30/30  ✅ ALL PASS
Layer 5: CVC+FZC basic         34/34  ✅ ALL PASS
Layer 5b: CVC+FZC comprehensive 71/71 ✅ ALL PASS
Layer 6: Full 7-ECU SIL        not started
```

## Test Suite — 1,021 Tests (CI GREEN)

| Suite | Tests | Status |
|-------|-------|--------|
| Com signals (generated) | 273 | PASS |
| VSM transitions (generated) | 107 | PASS |
| E2E messages (generated) | 100 | PASS |
| Com negative (generated) | 52 | PASS |
| E2E negative (generated) | 45 | PASS |
| E2E SM full (generated) | 41 | PASS |
| XCP security (generated) | 33 | PASS (2 ignored 64-bit) |
| CanSM full (generated) | 32 | PASS |
| Dem (generated) | 22 | PASS |
| WdgM (generated) | 19 | PASS |
| Rte (generated) | 18 | PASS |
| Det (generated) | 14 | PASS |
| FiM (generated) | 13 | PASS |
| Com existing | 30 | PASS |
| CanSM existing | 10 | PASS |
| E2E SM existing | 12 | PASS |
| Layer 4 integration | 30 | PASS |
| Layer 5 integration | 34 | PASS |
| Layer 5b comprehensive | 71 | PASS |
| **TOTAL** | **1,021** | **CI GREEN** |

## Pipeline Status

| Gate | Status |
|------|--------|
| Step 1: DBC (12-point) | 12/12 PASS |
| Step 3: ARXML generation | PASS |
| Step 4: ARXML validation | PASS |
| Step 5: Codegen (93 files) | PASS |
| Step 6: Round-trip | 3/3 PASS |
| Step 7: Data flow | 0 errors, 0 warnings |
| Syntax check (7 ECUs) | PASS |
| Unit tests (821) | 821/821 PASS |
| Integration tests (135) | 135/135 PASS |
| Traceability | 306/353 (86%) |
| CI (GitHub Actions) | **ALL GATES PASS** |

## Bugs Found and Fixed (20)

| # | Bug | Found By | Root Cause |
|---|-----|----------|-----------|
| 1 | E2E SM REPEATED = OK | 10-identity audit | Stuck sender undetected |
| 2 | CanSM Init(NULL) no reset | TDD | State not reset to UNINIT |
| 3 | CanSM L1 counter reset on re-entry | TDD | Infinite L1 loop, no L2 |
| 4 | VSM SC_KILL → SAFE_STOP | HARA audit | Should be SHUTDOWN (ISO 26262) |
| 5 | VSM MOTOR_CUTOFF → SAFE_STOP | HARA research | Should be DEGRADED (fail-silent) |
| 6 | vcan loopback starvation | Layer 3 socket test | RECV_OWN_MSGS ignored by vcan |
| 7 | PduR hand-written config CVC | Layer 4 debug | 13 entries shadowed 33 generated |
| 8 | CanIf hand-written config CVC | Layer 4 debug | Same pattern as PduR |
| 9 | Dead E2E code in heartbeats | Layer 4 build | -Werror caught unused vars |
| 10 | Dead cutoff_data in Swc_Brake | Layer 4 build | -Werror |
| 11 | 19 zombie CVC processes | Layer 4 timing test | Test scripts didn't cleanup |
| 12 | PERIODIC TX required pending | Layer 4 timing | Body_Control/E-Stop never sent |
| 13 | Docker SIL on vcan0 | Layer 4 phantom | Old containers injected frames |
| 14 | E2E test exact counter | CI | PERIODIC fires multiple times |
| 15 | Sub-byte signal packing | Layer 5b | <8-bit writes full byte, corrupts neighbor |
| 16 | Multi-byte signal packing | Layer 5b | 12-bit FaultMask overwrites 4-bit Mode |
| 17 | FZC PduR hand-written | Layer 5b XCP test | No XCP routing, same as CVC bug |
| 18 | FaultMask uint8→uint16 | Layer 5b | 12-bit signal read as uint16 from uint8 ptr |
| 19 | XCP no authentication | 10-auditor review | Any CAN device could write memory |
| 20 | XCP null address crash | 10-auditor review | SIGSEGV on SHORT_UPLOAD to addr 0 |

## Audit Fixes Applied

| Finding | Severity | Status |
|---------|----------|--------|
| XCP Seed & Key | CRITICAL | FIXED |
| XCP null address | CRITICAL | FIXED |
| Com COM_UINT32 dead path | MAJOR | FIXED |
| Com 16-bit bounds check | MAJOR | FIXED |
| Com SchM_TimingStop leak | MAJOR | FIXED |
| SWC layering (PduR.h/E2E.h) | MAJOR | FIXED |
| Dead code in Swc_CvcCom | MAJOR | FIXED (191 lines removed) |
| VSM GetRxStatus linker | MAJOR | FIXED |

## What's Next — Layer 6

1. **Build remaining 5 ECUs** (RZC, SC, BCM, ICU, TCU) for POSIX
2. **Fix RZC main.c** — same extern config pattern as CVC/FZC
3. **Docker SIL** — all 7 ECUs + plant-sim + CAN gateway
4. **SIL scenario tests** — 16 scenarios from test/sil/scenarios/
5. **Physical bench flash** — CVC + FZC + RZC on STM32

## Key Rule

**Never hand-write what codegen generates.** All config structs in ECU main.c must be `extern` to generated `*_Cfg_*.c` files. No exceptions.
