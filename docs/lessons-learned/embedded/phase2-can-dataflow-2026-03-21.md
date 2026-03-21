# Lessons Learned: Phase 2-4 CAN Data Flow Rewrite — 2026-03-21

## Context

Single-session rewrite of the entire CAN data flow stack from DBC through to SWC runtime, driven by gap analysis against production AUTOSAR systems.

## Timeline

### Phase 2: E2E in Com Layer
1. Added `E2E_Protect()` in `Com_MainFunction_Tx` before `PduR_Transmit`
2. Added `E2E_Check()` in `Com_RxIndication` after storing PDU data
3. Added per-PDU E2E state arrays (`com_e2e_tx_state`, `com_e2e_rx_state`)
4. Updated `Com_TxPduConfigType` with E2E fields (E2eProtected, E2eDataId, E2eCounterBit, E2eCrcBit)
5. Updated `Com_RxPduConfigType` with E2E fields (E2eProtected, E2eDataId, E2eMaxDelta)
6. Updated codegen template `Com_Cfg.c.j2` — Jinja `default()` doesn't catch `None`, used `if x is not none else 0`
7. Regenerated all 7 ECU configs — 93 files

### Phase 3: SWC Cleanup (31 → 0 violations)
1. FZC: Replaced `E2E_Protect + PduR_Transmit` with `Com_SendSignal` in Swc_FzcCom.c
2. FZC: Removed `Com_SendSignal` from Swc_FzcSafety.c and Swc_Brake.c (replaced with `Rte_Write`)
3. FZC: Moved heartbeat TX from Swc_Heartbeat.c to Swc_FzcCom.c bridge
4. RZC: Same pattern — Swc_RzcCom.c uses Com_SendSignal, E2E_Check delegated to Com
5. CVC: Swc_CvcCom.c uses Com_SendSignal for Vehicle_State, E2E wrappers become no-ops
6. CVC: Swc_EStop.c and Swc_Pedal.c use Rte_Write only, bridges added in Swc_CvcCom.c
7. Had to verify generated signal names against `*_Cfg.h` — names like `FZC_COM_SIG_BRAKE_FAULT_FAULT_TYPE` not guessable
8. Step 7 reached 0 errors, 0 warnings

### Gap Analysis Round 1 (12 perspectives)
1. Wrote `can-dataflow-gap-analysis.md` — 35 gaps across 12 stakeholder perspectives
2. Fixed 10 of 14 gaps:
   - Signal quality enum (`Com_SignalQualityType`) + `Com_GetRxPduQuality()` API
   - DTC reporting on E2E failure (`Dem_ReportErrorStatus` in Com_RxIndication)
   - Type-safe macros (`Com_SendSignal_u8/u16/s16` with `_Static_assert`)
   - Compile-time assert (`COM_TX_MAIN_PERIOD_MS` vs `BSW_SCHEDULER_PERIOD_MS`)
   - Dead code removal (unused `pdu[]`, `pdu_info` from Phase 3)
   - Bus load budget document, HMI spec, CAN bus topology doc

### Gap Analysis Round 2 (10 iterations vs production AUTOSAR)
1. Wrote `can-dataflow-vs-production.md` — compared against Vector MICROSAR, EB tresos, AUTOSAR R22-11
2. Built 3 new BSW modules:
   - **CanSM** — L1/L2 bus-off recovery state machine
   - **FiM** — Function Inhibition Manager with Dem event linking
   - **Xcp** — XCP slave (CONNECT, SHORT_UPLOAD, SHORT_DOWNLOAD, SET_MTA, UPLOAD)
3. Added to Com:
   - DIRECT TX mode (`Com_TriggerIPDUSend`, `COM_TX_MODE_DIRECT/PERIODIC/MIXED/NONE`)
   - TRIGGERED_ON_CHANGE (payload comparison, skip TX for unchanged event PDUs)
   - TX confirmation monitoring (`com_tx_confirm_cnt`, `g_dbg_com_tx_stuck`)
   - Startup delay (`COM_STARTUP_DELAY_MS` with ceiling division)
   - Signal groups (`Com_FlushTxPdu` for atomic multi-signal commit)
   - Update bits (per-signal `UpdateBitPos` field)
   - E2E supervision state machine (windowed: `E2E_SMCheck` with VALID/NODATA/INIT/INVALID)
4. Added WCET measurement (`SchM_Timing` — DWT on Cortex-M, RTI on TMS570, clock_gettime on POSIX)
5. Added XCP tooling (gen_a2l.py, xcp_test.py, xcp_config.json)
6. Added coverage tooling (test/Makefile.coverage with gcov + lcov)
7. DBC updated with 8 XCP CAN messages (0x550-0x557)

### Top-Down Wiring Audit
1. Found 7 of 12 modules DISCONNECTED (code existed but not initialized/scheduled)
2. Wired all into CVC main.c:
   - `CanSM_Init()`, `FiM_Init()`, `Xcp_Init()`, `SchM_TimingInit()` in BSW init block
   - `CanSM_MainFunction()` in 10ms task, `FiM_MainFunction()` in 100ms task
   - `CanSM_ControllerBusOff()` called from `CanIf_ControllerBusOff()`
   - `SchM_TimingStart/Stop` instrumented in `Com_MainFunction_Tx`
   - `Com_GetRxPduQuality()` called from `Swc_FzcSafety.c` for steer/brake commands
   - E2E SM config made per-PDU via codegen (`E2eSmWindowValid/Invalid` fields)

### 10-Identity Sanity Check
1. Found 11 issues across 10 reviewer perspectives
2. Fixed all:
   - **Safety bug**: E2E SM treated REPEATED as OK (stuck sender undetected) → fixed to only count STATUS_OK
   - **Dead code**: Removed unused `com_e2e_sm_default`
   - **Performance**: O(n) TX PDU search → O(1) `com_tx_pdu_index[]` lookup table
   - **Design**: Startup delay integer rounding → ceiling division macro
   - **Design**: TX confirm counter semantics documented

### TDD Phase
1. Built and ran 52 unit tests across 3 suites:
   - **Com**: 30 tests (signal send/receive, E2E discard, signal quality, TX timing)
   - **CanSM**: 10 tests (init, state transitions, L1/L2 bus-off recovery)
   - **E2E SM**: 12 tests (state machine transitions, windowed evaluation, REPEATED handling)
2. TDD caught 2 real CanSM bugs:
   - `Init(NULL)` didn't reset state to UNINIT
   - Bus-off re-entry reset L1 attempt counter, preventing L1→L2 escalation
3. Created `stubs_com_test.c` for Det, SchM, Rte, CanIf, SchM_Timing mocks

## Key Mistakes

1. **Jinja `default()` doesn't catch `None`** — only catches undefined. Use `if x is not none else 0`.
2. **Generated signal names aren't guessable** — always grep `*_Cfg.h` for exact names before using in code.
3. **Writing code before tests** — built CanSM, FiM, XCP without tests. TDD caught 2 bugs that would have shipped.
4. **REPEATED frame = OK was wrong** — a stuck sender sends the same counter forever. Must only count truly new frames.
5. **Disconnected modules aren't real** — code that isn't called from init+scheduler doesn't exist. Always audit the wiring.

## Key Principles

1. **DBC is truth** — every CAN parameter flows DBC → ARXML → codegen → runtime. Never hand-edit generated files.
2. **Top-down audit after every feature** — check the full chain, not just "does it compile."
3. **Windowed E2E supervision** — single-error discard is too aggressive for real CAN (EMC noise). Use consecutive-error windows.
4. **TDD for BSW modules** — write tests first, fix code to match. Tests define behavior, not the other way around.
5. **O(1) hot-path lookups** — `Com_SendSignal` is called thousands of times per second. Linear search is unacceptable.

## Metrics

| Metric | Before | After |
|--------|--------|-------|
| BSW modules | 18 | 21 (+CanSM, FiM, Xcp) |
| Data flow violations | 31 | 0 |
| Production gaps (vs AUTOSAR) | 12 | 0 fixed + 0 open |
| Unit tests | 21 | 52 |
| Test suites | 1 | 3 |
| Bugs caught by TDD | 0 | 2 |
| Pipeline gates | 7 | 7 (all passing) |
| DBC messages | 37 | 45 (+8 XCP) |
| Generated config files | 93 | 93 |
