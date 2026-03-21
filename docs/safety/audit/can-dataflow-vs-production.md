# CAN Data Flow: Taktflow vs Production Automotive — 10 Iteration Comparison

**Date:** 2026-03-21
**Method:** Compare Taktflow implementation against AUTOSAR Classic Platform R22-11, Vector MICROSAR, EB tresos, and documented OEM practices. Each iteration deepens the analysis.

---

## Iteration 1: AUTOSAR COM Transmission Modes

**Production standard:** AUTOSAR COM defines 4 I-PDU transmission modes: DIRECT, PERIODIC, MIXED, NONE. Each signal has a transfer property (TRIGGERED, TRIGGERED_ON_CHANGE, PENDING, etc.). The interaction layer decides *when* to send based on signal write events and periodic timers independently.

**Taktflow:** Com_MainFunction_Tx has two modes — cyclic (CycleTimeMs > 0) and event (CycleTimeMs == 0). No MIXED mode. No per-signal transfer property. No TRIGGERED_ON_CHANGE. No DIRECT mode (all TX deferred to next MainFunction cycle = 10ms latency).

| Feature | Production | Taktflow | Gap |
|---------|-----------|----------|-----|
| DIRECT mode (immediate TX on trigger) | Yes | No | 10ms latency on all event messages |
| MIXED mode (periodic + immediate on trigger) | Yes | No | Can't combine baseline periodic with event triggers |
| TRIGGERED_ON_CHANGE | Yes | No | Every cycle sends even if value unchanged |
| Per-signal transfer property | Yes (5 types) | No | All signals on a PDU share same TX behavior |
| Minimum Delay Timer (MDT) | Yes | No | No minimum inter-TX gap — could flood bus |
| TX mode selection (True/False based on filter) | Yes | No | No filter-based mode switching |

**Impact:** Without TRIGGERED_ON_CHANGE, event PDUs like Brake_Fault (0x210) send every cycle even when no new fault, wasting bus bandwidth. Without DIRECT mode, safety-critical events have guaranteed 10ms latency floor.

---

## Iteration 2: AUTOSAR COM Signal Groups & Update Bits

**Production standard:** Signal groups guarantee atomic multi-signal updates. `Com_SendSignalGroup()` copies shadow buffer to I-PDU atomically. Update bits (1 per signal) tell the receiver which signals were actually written in this I-PDU instance. Signal invalidation via `Com_InvalidateSignal()` writes a configured invalid value and triggers `Com_CbkInv` callback on receiver.

**Taktflow:** No signal groups — each `Com_SendSignal()` independently writes to the PDU buffer. No update bits. No signal invalidation. No notification callbacks (no `Com_CbkRxAck`, no `Com_CbkInv`).

| Feature | Production | Taktflow | Gap |
|---------|-----------|----------|-----|
| Signal groups (atomic multi-signal write) | Yes | No | Race condition: partial update visible to receiver between two SendSignal calls |
| Update bits | Yes | No | Receiver can't distinguish "signal not updated" from "signal is zero" |
| Signal invalidation | Yes | No | No way to signal "data not available" vs "data is zero" |
| RX notification callback | Yes | No | SWC has no event-driven reception — must poll |
| TX confirmation callback | Yes | No | Sender doesn't know if frame reached CAN driver |
| I-PDU callouts (pre-TX, post-RX hooks) | Yes | No | No application-level inspection/modification of raw PDU |

**Impact:** Vehicle_State (0x100) has Mode + FaultMask + TorqueLimit. If sender writes Mode then FaultMask in two separate Com_SendSignal calls, a concurrent Com_MainFunction_Tx could send an I-PDU with new Mode but old FaultMask. In production AUTOSAR, `Com_SendSignalGroup()` prevents this.

---

## Iteration 3: AUTOSAR E2E State Machine & Profiles

**Production standard:** E2E Library has a supervision state machine with states VALID, NODATA, INIT, WRONG, ERROR. Transitions based on configurable window of consecutive valid/invalid checks (e.g., 3 consecutive OK → VALID, 2 consecutive ERROR → ERROR state). Multiple profiles (P01 through P07) with increasing CRC/counter size.

**Taktflow:** E2E_Check returns a status enum (OK, REPEATED, WRONG_SEQ, ERROR) but has no state machine. Single check → accept or discard. Uses only Profile P01 (4-bit counter, 8-bit CRC). No windowed supervision.

| Feature | Production | Taktflow | Gap |
|---------|-----------|----------|-----|
| E2E state machine (VALID/NODATA/INIT/WRONG/ERROR) | Yes | No | No windowed supervision — single bad frame = immediate discard |
| Configurable valid/invalid windows | Yes (e.g., WindowSizeValid=3) | No | No hysteresis — one CRC glitch discards frame |
| E2E Profile P04 (32-bit CRC, 16-bit counter) | Available | No | P01 4-bit counter limits MaxDeltaCounter to 15 — insufficient for high-rate messages |
| E2E Profile P05/P06 (16-bit CRC) | Available | No | 8-bit CRC has 1/256 false-accept probability |
| Data ID inclusion modes (4 modes) | Yes | 1 mode only | Only supports single DataID value, not list-based or nibble-swapped |
| E2E status mapping to Dem event | Via E2E supervision | Manual | No automatic Dem event mapping from E2E state transitions |

**Impact:** A single bit-flip on CAN (common at automotive EMC levels) causes E2E_STATUS_ERROR → frame discard → stale signal. Production systems tolerate isolated errors via windowed supervision (e.g., 1 error in 5 frames = still VALID). Our system is overly sensitive.

---

## Iteration 4: AUTOSAR PduR Routing Architecture

**Production standard:** PduR supports 1:N fan-out (one TX PDU to multiple lower layers), N:1 fan-in, FIFO and Last-is-Best buffering, TP gateway-on-the-fly (stream routing without full buffer), and separate IF/TP routing paths.

**Taktflow:** PduR is a thin pass-through. `PduR_Transmit()` calls `CanIf_Transmit()` directly. `PduR_CanIfRxIndication()` routes to Com or Dcm based on static table. No buffering, no fan-out, no TP routing, no gateway-on-the-fly.

| Feature | Production | Taktflow | Gap |
|---------|-----------|----------|-----|
| TX buffering (FIFO or Last-is-Best) | Yes | No | If CanIf_Transmit returns busy, PDU is lost |
| 1:N fan-out (same TX PDU to CAN + Ethernet) | Yes | No | Single lower layer only |
| N:1 fan-in (multiple sources to one PDU) | Yes | No | — |
| TP gateway-on-the-fly | Yes | No | Full TP message must be buffered before forwarding |
| Post-build routing table reconfiguration | Yes | No | Routing is compile-time fixed |
| Metadata support (CAN ID, source address) | Yes | No | No metadata passed through routing |

**Impact:** If CAN hardware TX FIFO is full (3-deep on STM32 FDCAN), `PduR_Transmit` → `CanIf_Transmit` → `Can_Write` returns E_NOT_OK and the frame is silently lost. Production PduR would buffer and retry. For safety-critical messages, this is a gap — but currently mitigated by Com's "pending stays TRUE until success" retry.

---

## Iteration 5: AUTOSAR CanIf & CAN State Machine (CanSM)

**Production standard:** CanSM manages CAN controller states (UNINIT, STOPPED, STARTED, SLEEP), handles bus-off recovery with configurable L1/L2 recovery cycles, coordinates with ComM for communication mode management, and uses CanIf's transceiver control for sleep/wake.

**Taktflow:** No CanSM module. Bus-off recovery is a 10ms timer hack in experimental ThreadX code. No transceiver mode control. No ComM. No coordinated communication mode management.

| Feature | Production | Taktflow | Gap |
|---------|-----------|----------|-----|
| CanSM state machine | Yes (UNINIT/STOPPED/STARTED/SLEEP) | No | No formal CAN state management |
| Bus-off recovery (L1: fast, L2: slow) | Yes, configurable | Ad-hoc 10ms timer | No configurable recovery strategy |
| Transceiver mode control (NORMAL/STANDBY/SLEEP) | Yes via CanTrcv | No | Transceiver always powered |
| ComM (Communication Manager) | Yes | No | No coordinated comm mode changes |
| I-PDU group start/stop | Yes via Com_IpduGroupControl | No | Can't selectively enable/disable message groups |
| Wakeup validation sequence | Yes | No | No wake-up source validation |
| Partial networking (selective ECU wake) | Yes | No | All ECUs always awake |

**Impact:** No CanSM means bus-off recovery is ad-hoc and untestable. No ComM means diagnostic sessions can't suppress application communication (standard practice during reprogramming). No partial networking means the entire CAN bus must be powered even if only one ECU needs servicing.

---

## Iteration 6: AUTOSAR COM Deadline Monitoring (TX side)

**Production standard:** AUTOSAR COM monitors TX confirmation. If `PduR_ComTransmit` is called but no `Com_TxConfirmation` arrives within `ComTxFirstTimeout` / `ComTimeout`, deadline monitoring fires `ComTimeoutNotification`. This detects TX path failures (CAN driver stuck, bus-off not detected, hardware fault).

**Taktflow:** Only RX deadline monitoring. TX path has no confirmation monitoring. `PduR_Transmit` return value is checked but TX completion confirmation doesn't exist.

| Feature | Production | Taktflow | Gap |
|---------|-----------|----------|-----|
| TX deadline monitoring | Yes | No | Cannot detect stuck TX path |
| TX confirmation callback (Com_TxConfirmation) | Yes | No | Sender never knows if frame reached bus |
| TX timeout notification | Yes | No | No callback on TX failure |
| TX first-timeout (initial TX) | Yes | No | — |
| TX error counter exposure | Yes | debug counter only | `com_tx_send_count` is debug-only, not safety-accessible |

**Impact:** If the CAN driver silently fails (hardware fault, register corruption), our COM layer will keep calling `PduR_Transmit` which returns E_OK (driver accepted it) but the frame never reaches the bus. RX-side timeout on *other ECUs* will eventually detect the missing message, but the sender has no local indication. Production COM detects this within milliseconds via TX confirmation timeout.

---

## Iteration 7: OEM DBC/ARXML Metadata Practices

**Production standard:** OEM CAN matrices carry extensive metadata beyond what we have: `GenMsgSendType` (cyclic, event, cyclicIfActive, etc.), `GenMsgDelayTime` (minimum inter-TX delay), `GenMsgStartDelayTime` (initial startup delay), `GenMsgFastOnStart` (fast initial burst), `NmMessage` flag, `DiagRequest/DiagResponse` flags, `GenSigSendType` (per-signal), `SigType` (Range, ASCII, DTC, etc.), and long symbol names.

**Taktflow:** We have `GenMsgCycleTime`, `E2E_DataID`, `E2E_MaxDeltaCounter`, `Satisfies`, `ASIL`, `Owner`, `ProducedBy`, `GenSigStartValue`. Missing many standard attributes.

| Attribute | Standard Practice | Taktflow | Gap |
|-----------|------------------|----------|-----|
| GenMsgSendType (cyclic/event/cyclicIfActive) | Yes | No — inferred from CycleTimeMs | No "cyclicIfActive" (send only when data changes) |
| GenMsgDelayTime (min inter-TX) | Yes | No | No minimum delay timer |
| GenMsgStartDelayTime | Yes | No | All ECUs start TX immediately — potential startup storm |
| GenMsgFastOnStart (fast initial burst) | Yes | No | No fast-start for heartbeats |
| GenMsgNrOfRepetitions (event repetition) | Defined in DBC | Not used by codegen | Attribute exists but codegen ignores it |
| NmMessage flag | Yes | No | No NM identification |
| GenSigSendType (per-signal TX trigger) | Yes | No | All signals in PDU share TX behavior |
| GenSigCycleTime (signal-level cycle override) | Yes | No | — |
| SystemSignalLongSymbol | Yes | No | 32-char DBC limit on signal names |

**Impact:** Without `GenMsgStartDelayTime`, all 7 ECUs start transmitting simultaneously on power-up, causing a CAN arbitration storm. Production systems stagger startup delays (e.g., CVC at 0ms, FZC at 20ms, RZC at 40ms). Without `GenMsgDelayTime`, event messages have no rate limiting below the 10ms MainFunction period.

---

## Iteration 8: AUTOSAR XCP Calibration & Measurement

**Production standard:** Every production ECU has XCP (Universal Measurement and Calibration Protocol) or legacy CCP for runtime variable measurement and calibration parameter adjustment. Calibration tools (Vector CANape, ETAS INCA) connect via CAN and read/write ECU RAM using A2L file descriptions. DAQ lists provide synchronous measurement at configurable rates.

**Taktflow:** No XCP/CCP support. Debug visibility is limited to `volatile` debug counters read via UART or UDS ReadDataByIdentifier. No runtime calibration capability. No A2L file.

| Feature | Production | Taktflow | Gap |
|---------|-----------|----------|-----|
| XCP on CAN | Standard | No | No runtime measurement/calibration protocol |
| A2L file (ECU description) | Standard | No | No machine-readable variable description |
| DAQ lists (synchronous measurement) | Standard | No | Cannot measure internal signals at cycle-accurate timing |
| STIM (value injection) | XCP feature | No | Cannot override signal values for testing |
| Calibration parameters in NvM | Standard | No | All parameters in flash, not runtime-adjustable |
| Seed & Key security for XCP | Standard | No | — |

**Impact:** During bench testing, we can't measure internal variables (e.g., E2E alive counter, Com cycle count, signal quality state) without UART printf or UDS hacks. Production teams use CANape + XCP to see *everything* in real-time. For development velocity, this is the single biggest tooling gap.

---

## Iteration 9: ISO 26262 Part 6 Verification Requirements for ASIL D

**Production standard:** ASIL D software units require MC/DC (Modified Condition/Decision Coverage), requirements-based testing with equivalence classes + boundary values + error guessing, back-to-back testing, resource usage verification, fault injection, and interface testing.

**Taktflow:** Unity-based unit tests with statement-level coverage. No MC/DC measurement. No automated coverage tool. No back-to-back testing. No resource usage testing (stack, timing). Limited fault injection (DLC boundary, null pointer, E2E corruption as of today).

| Requirement (Part 6 Table 9) | ASIL D Level | Taktflow | Gap |
|-------------------------------|-------------|----------|-----|
| MC/DC coverage | ++ (required) | No | No coverage measurement tool at all |
| Branch coverage | ++ (required) | Not measured | Tests exist but coverage not quantified |
| Statement coverage | + (recommended) | Not measured | — |
| Back-to-back testing | ++ (required for model-based) | No | No model to compare against |
| Resource usage (stack/timing) | Required | No | No stack depth analysis, no WCET |
| Fault injection | ++ (required) | Partial | E2E corruption tested, but not register corruption, stack overflow, timing violation |
| Interface testing | ++ (required) | Partial | API boundary tests exist, but no inter-module interface test |
| Requirements traceability | ++ (required) | Partial | @verifies tags in tests, but no tool-generated matrix |

**Impact:** For ISO 26262 certification, we need gcov or LDRA/Parasoft for MC/DC on Com.c, E2E.c, and all ASIL D units. Without MC/DC, the safety case has a hard gap — no assessor will accept "we have tests but don't measure coverage" for ASIL D.

---

## Iteration 10: AUTOSAR FiM + Dem Integration for Graceful Degradation

**Production standard:** FiM (Function Inhibition Manager) provides centralized functionality degradation. When Dem confirms a fault event, FiM evaluates inhibition conditions and SWCs poll `FiM_GetFunctionPermission(FID)` before executing. This creates a formal degradation hierarchy — fault → DTC confirmed → FiM inhibits → SWC degrades → reduced capability communicated via COM.

**Taktflow:** Fault handling is scattered across SWCs. Each SWC independently checks fault conditions and decides its own degradation. No centralized inhibition. Dem stores DTCs but doesn't influence SWC behavior. No FiM module.

| Feature | Production | Taktflow | Gap |
|---------|-----------|----------|-----|
| FiM module | Standard | No | No centralized function inhibition |
| Dem → FiM event propagation | Automatic | No | SWCs poll RTE signals independently |
| Inhibition conditions (boolean logic on DEM events) | Yes | No | Each SWC has its own if/else fault logic |
| Permission check API (`FiM_GetFunctionPermission`) | Standard | No | No standard degradation query interface |
| I-PDU group control (stop TX on fault) | Via Com_IpduGroupControl | No | Can't stop a group of messages atomically |
| Degradation hierarchy documentation | Standard (safety concept) | Partial | Vehicle state machine exists but not formalized as FiM function IDs |

**Impact:** When a brake fault occurs, Swc_FzcSafety writes `MOTOR_CUTOFF=1` to RTE, Swc_Brake checks its own fault latch, and Swc_FzcCom reads RTE and sends. If any SWC's fault check has a bug, the degradation chain breaks. Production FiM provides a single source of truth: if `FID_BRAKE_CONTROL` is inhibited, the brake SWC doesn't run regardless of its internal logic.

---

## Cumulative Gap Summary (10 Iterations)

| Iteration | Domain | Critical Gaps | Severity |
|-----------|--------|---------------|----------|
| 1 | COM TX Modes | No DIRECT/MIXED/TRIGGERED_ON_CHANGE, no MDT | Medium — 10ms latency floor on events |
| 2 | Signal Groups | No atomic multi-signal write, no update bits, no invalidation | High — data consistency risk |
| 3 | E2E State Machine | No windowed supervision, P01 only | High — overly sensitive to single errors |
| 4 | PduR Routing | No TX buffering, no fan-out | Low — mitigated by Com retry |
| 5 | CanSM/ComM | No CAN state machine, no comm mode mgmt | Medium — bus-off recovery is ad-hoc |
| 6 | TX Deadline | No TX confirmation monitoring | Medium — stuck TX path undetected locally |
| 7 | DBC Metadata | Missing startup delay, event repetition, per-signal TX | Medium — startup arbitration storm |
| 8 | XCP Calibration | No runtime measurement/calibration | High — major development velocity gap |
| 9 | ISO 26262 Coverage | No MC/DC, no coverage tool, no WCET | **Critical** — certification blocker |
| 10 | FiM Degradation | No centralized inhibition manager | Medium — fault handling scattered |

---

## Classification: Must-Have vs Should-Have vs Nice-to-Have

### Must-Have for Production (certification blockers)

| # | Gap | Why | Effort |
|---|-----|-----|--------|
| 9.1 | **MC/DC coverage measurement** | ISO 26262 Table 9 requires ++ for ASIL D | Medium — integrate gcov + lcov or commercial tool |
| 9.2 | **Branch coverage measurement** | ISO 26262 Table 9 requires ++ for ASIL B+ | Small — same tooling as MC/DC |
| 3.1 | **E2E windowed supervision state machine** | Single-error sensitivity causes unnecessary signal loss | Medium — implement AUTOSAR E2E SM |
| 2.1 | **Signal groups (atomic multi-signal write)** | Data consistency for Vehicle_State, Motor_Status | Medium — add Com_SendSignalGroup |
| 9.3 | **WCET analysis (worst-case execution time)** | Required for timing safety argument | Medium — static analysis or measurement |

### Should-Have for Production Quality

| # | Gap | Why | Effort |
|---|-----|-----|--------|
| 1.1 | **DIRECT TX mode** (immediate send on trigger) | Reduces worst-case event latency from 10ms to <1ms | Medium |
| 5.1 | **CanSM with bus-off recovery strategy** | Formal, testable recovery | Medium |
| 6.1 | **TX confirmation monitoring** | Detect stuck TX path locally | Small |
| 7.1 | **Startup delay staggering** | Prevent CAN arbitration storm | Small — add GenMsgStartDelayTime to codegen |
| 10.1 | **FiM or equivalent inhibition** | Centralized degradation | Large |
| 2.2 | **Update bits** | Distinguish "not updated" from "value is zero" | Medium |
| 1.2 | **TRIGGERED_ON_CHANGE** | Reduce unnecessary TX | Small |

### Nice-to-Have (development velocity / future)

| # | Gap | Why | Effort |
|---|-----|-----|--------|
| 8.1 | **XCP on CAN** | Runtime measurement for bench development | Large |
| 4.1 | **PduR TX buffering** | Handle CAN FIFO full gracefully | Medium |
| 2.3 | **Signal invalidation** | "No data" vs "zero" distinction | Small |
| 7.2 | **GenMsgNrOfRepetitions in codegen** | Event message reliability | Small |
| 5.2 | **Partial networking** | Power management for vehicle sleep | Large |
| 15.1 | **CAN FD support** | Higher throughput, larger payloads | Large — transceiver + driver changes |

---

## Fix Status (updated 2026-03-21)

| # | Gap | Status | Implementation |
|---|-----|--------|----------------|
| 1.2 | TRIGGERED_ON_CHANGE | **DONE** | Com_MainFunction_Tx compares payload vs previous, skips event TX if unchanged |
| 2.1 | Signal groups | **DONE** | `Com_FlushTxPdu()` API for atomic multi-signal commit |
| 3.1 | E2E supervision state machine | **DONE** | `E2E_SMCheck()` with VALID/NODATA/INIT/INVALID states, configurable windows |
| 6.1 | TX confirmation monitoring | **DONE** | `com_tx_confirm_cnt[]` tracks failed TX, `g_dbg_com_tx_stuck[]` counter |
| 7.1 | Startup delay staggering | **DONE** | `COM_STARTUP_DELAY_MS` suppresses TX after init (configurable per ECU) |
| 8.1 | XCP on CAN | **DONE** | Xcp.h/Xcp.c slave, DBC CAN IDs, PduR routing, A2L gen, pyXCP test |
| 9.1 | Coverage tooling | **DONE** | `test/Makefile.coverage` with gcov + lcov + branch coverage |
| 1.1 | DIRECT TX mode | OPEN | Still 10ms latency floor (acceptable for bench) |
| 2.2 | Update bits | OPEN | Not implemented (low priority for bench) |
| 5.1 | CanSM | OPEN | No formal CAN state machine (ad-hoc bus-off recovery) |
| 9.3 | WCET analysis | OPEN | No static timing analysis tool |
| 10.1 | FiM | OPEN | No centralized inhibition (large effort) |

**Score: 7/12 fixed, 5 open (3 large effort, 2 low priority).**
